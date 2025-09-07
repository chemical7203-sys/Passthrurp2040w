#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include <string.h>

// UART defines
#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5

// CC1101 SPI defines
#define SPI_PORT spi0
#define SPI_MISO_PIN 16
#define SPI_MOSI_PIN 19
#define SPI_SCLK_PIN 18
#define SPI_CSN_PIN 17

// CC1101 GDO pin
#define GDO0_PIN 20

// Signal capture settings
#define MAX_PULSES 250 // Max number of pulses to record in one transmission
#define END_OF_TRANSMISSION_US 5000 // A gap of 5ms or more marks the end of a signal

// Global variables for signal capture
volatile uint32_t pulse_timings[MAX_PULSES];
volatile uint16_t pulse_count = 0;
volatile bool capture_done = false;
absolute_time_t last_edge_time;

// Global variables for storing a signal to transmit
uint32_t stored_pulse_timings[MAX_PULSES];
uint16_t stored_pulse_count = 0;

// State machine for device operation
typedef enum {
    STATE_IDLE,
    STATE_ARMED_TO_CAPTURE,
} DeviceState;

DeviceState current_state = STATE_IDLE;


// CC1101 Register Definitions
#define CC1101_IOCFG0       0x02    // GDO0 output pin configuration
#define CC1101_FIFOTHR      0x03    // RX FIFO and TX FIFO thresholds
#define CC1101_PKTLEN       0x06    // Packet length
#define CC1101_PKTCTRL0     0x07    // Packet automation control
#define CC1101_ADDR         0x09    // Device address
#define CC1101_CHANNR       0x0A    // Channel number
#define CC1101_FSCTRL1      0x0B    // Frequency synthesizer control
#define CC1101_FREQ2        0x0D    // Frequency control word, high byte
#define CC1101_FREQ1        0x0E    // Frequency control word, middle byte
#define CC1101_FREQ0        0x0F    // Frequency control word, low byte
#define CC1101_MDMCFG4      0x10    // Modem configuration
#define CC1101_MDMCFG3      0x11    // Modem configuration
#define CC1101_MDMCFG2      0x12    // Modem configuration
#define CC1101_MDMCFG1      0x13    // Modem configuration
#define CC1101_MDMCFG0      0x14    // Modem configuration
#define CC1101_DEVIATN      0x15    // Modem deviation setting
#define CC1101_MCSM1        0x17    // Main Radio Control State Machine configuration
#define CC1101_MCSM0        0x18    // Main Radio Control State Machine configuration
#define CC1101_FOCCFG       0x19    // Frequency Offset Compensation configuration
#define CC1101_AGCCTRL2     0x1B    // AGC control
#define CC1101_WORCTRL      0x1E    // Wake on Radio control
#define CC1101_FSCAL3       0x23    // Frequency synthesizer calibration
#define CC1101_FSCAL2       0x24    // Frequency synthesizer calibration
#define CC1101_FSCAL1       0x25    // Frequency synthesizer calibration
#define CC1101_FSCAL0       0x26    // Frequency synthesizer calibration
#define CC1101_TEST2        0x2C    // Various test settings
#define CC1101_TEST1        0x2D    // Various test settings
#define CC1101_TEST0        0x2E    // Various test settings
#define CC1101_PATABLE      0x3E    // PATABLE address
#define CC1101_TXFIFO       0x3F    // TX FIFO address
#define CC1101_RXFIFO       0x3F    // RX FIFO address

// Strobe commands
#define CC1101_SRES         0x30    // Reset chip.
#define CC1101_SFSTXON      0x31    // Enable and calibrate frequency synthesizer (if MCSM0.FS_AUTOCAL=1).
#define CC1101_SXOFF        0x32    // Turn off crystal oscillator.
#define CC1101_SCAL         0x33    // Calibrate frequency synthesizer and turn it off.
#define CC1101_SRX          0x34    // Enable RX. Perform calibration first if coming from IDLE and MCSM0.FS_AUTOCAL=1.
#define CC1101_STX          0x35    // Enable TX. Perform calibration first if coming from IDLE and MCSM0.FS_AUTOCAL=1.
#define CC1101_SIDLE        0x36    // Exit RX / TX, turn off frequency synthesizer and exit Wake-On-Radio mode if applicable.
#define CC1101_SNOP         0x3D    // No operation. May be used to get access to the chip status byte.

// SPI Read/Write flags
#define WRITE_BURST         0x40
#define READ_SINGLE         0x80
#define READ_BURST          0xC0

// Function Prototypes
void write_register(uint8_t addr, uint8_t value);
uint8_t read_register(uint8_t addr);
void cc1101_strobe(uint8_t strobe);
void reset_cc1101(void);
void init_cc1101(void);
void read_burst_register(uint8_t addr, uint8_t *buffer, uint8_t count);

// CC1101 configuration registers for 433MHz OOK/ASK
static const uint8_t cc1101_regs_433mhz[] = {
    CC1101_FSCTRL1, 0x06,
    CC1101_FREQ2,   0x10,
    CC1101_FREQ1,   0xB1,
    CC1101_FREQ0,   0x3B,
    CC1101_MDMCFG4, 0x8C, // RX BW 101.56kHz
    CC1101_MDMCFG3, 0x22, // 2.4kBaud
    CC1101_MDMCFG2, 0x02, // ASK/OOK, no sync
    CC1101_MDMCFG1, 0x22,
    CC1101_MDMCFG0, 0xF8,
    CC1101_CHANNR,  0x00,
    CC1101_DEVIATN, 0x15,
    CC1101_FOCCFG,  0x16,
    CC1101_AGCCTRL2,0x43,
    CC1101_WORCTRL, 0xFB,
    CC1101_FSCAL3,  0xE9,
    CC1101_FSCAL2,  0x2A,
    CC1101_FSCAL1,  0x00,
    CC1101_FSCAL0,  0x1F,
    CC1101_TEST2,   0x81,
    CC1101_TEST1,   0x35,
    CC1101_TEST0,   0x09,
    CC1101_PKTCTRL0,0x00, // Fixed packet length, no CRC
    CC1101_ADDR,    0x00,
    CC1101_PKTLEN,  0x0A, // Packet length 10 bytes, adjust as needed
    CC1101_IOCFG0,  0x06, // GDO0 asserts on sync word sent/received
    CC1101_MCSM1,   0x0C, // Stay in RX after packet
    CC1101_MCSM0,   0x18, // Auto calibrate from IDLE to RX
    0xFF, 0xFF // End of list marker
};


void write_register(uint8_t addr, uint8_t value) {
    gpio_put(SPI_CSN_PIN, 0);
    while(gpio_get(SPI_MISO_PIN)); // Wait for MISO to go low
    spi_write_blocking(SPI_PORT, &addr, 1);
    spi_write_blocking(SPI_PORT, &value, 1);
    gpio_put(SPI_CSN_PIN, 1);
}

uint8_t read_register(uint8_t addr) {
    uint8_t value;
    addr |= READ_SINGLE;
    gpio_put(SPI_CSN_PIN, 0);
    while(gpio_get(SPI_MISO_PIN)); // Wait for MISO to go low
    spi_write_blocking(SPI_PORT, &addr, 1);
    spi_read_blocking(SPI_PORT, 0, &value, 1);
    gpio_put(SPI_CSN_PIN, 1);
    return value;
}

void read_burst_register(uint8_t addr, uint8_t *buffer, uint8_t count) {
    addr |= READ_BURST;
    gpio_put(SPI_CSN_PIN, 0);
    while(gpio_get(SPI_MISO_PIN));
    spi_write_blocking(SPI_PORT, &addr, 1);
    spi_read_blocking(SPI_PORT, 0, buffer, count);
    gpio_put(SPI_CSN_PIN, 1);
}


void cc1101_strobe(uint8_t strobe) {
    gpio_put(SPI_CSN_PIN, 0);
    while(gpio_get(SPI_MISO_PIN));
    spi_write_blocking(SPI_PORT, &strobe, 1);
    gpio_put(SPI_CSN_PIN, 1);
}

void reset_cc1101(void) {
    gpio_put(SPI_CSN_PIN, 0);
    sleep_us(10);
    gpio_put(SPI_CSN_PIN, 1);
    sleep_us(40);
    cc1101_strobe(CC1101_SRES);
    sleep_ms(1);
}

void init_cc1101(void) {
    reset_cc1101();
    for (int i = 0; cc1101_regs_433mhz[i] != 0xFF; i += 2) {
        write_register(cc1101_regs_433mhz[i], cc1101_regs_433mhz[i+1]);
    }
}

void transmit_signal(uint32_t* timings, uint16_t count);


void gpio_callback(uint gpio, uint32_t events) {
    // This check prevents the ISR from running while the main loop is processing data
    if (capture_done) {
        return;
    }

    absolute_time_t now = get_absolute_time();
    uint32_t duration_us = absolute_time_diff_us(last_edge_time, now);
    last_edge_time = now;

    // A long gap indicates the end of a previous transmission and the start of a new one.
    // We reset the counter.
    if (duration_us > END_OF_TRANSMISSION_US) {
        pulse_count = 0;
        // The first "pulse" is the long gap, we don't store it but we've started capturing.
        return;
    }

    // Store the pulse duration if there's space
    if (pulse_count < MAX_PULSES) {
        pulse_timings[pulse_count++] = duration_us;
    } else {
        // If we run out of space, it's probably noise, so we reset.
        pulse_count = 0;
    }

    // If we have a reasonable number of pulses, start checking for the end-of-transmission gap.
    // This is a simple timeout check on the interrupt itself. If no edge comes for a while,
    // we assume the signal is over. We'll handle this in the main loop with a timer.
}


void setup_uart() {
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_puts(UART_ID, "\n\nRP2040 CC1101 Cloner\n");
}

void transmit_signal(uint32_t* timings, uint16_t count) {
    if (count == 0) {
        uart_puts(UART_ID, "STATUS:No signal stored to transmit\n");
        return;
    }

    uart_puts(UART_ID, "STATUS:Transmitting...\n");

    // Put CC1101 into IDLE and configure for TX
    cc1101_strobe(CC1101_SIDLE);
    // Set PATABLE for high output power (+10 dBm)
    write_register(CC1101_PATABLE, 0xC0);
    // Configure GDO0 as an output on the Pico
    gpio_init(GDO0_PIN);
    gpio_set_dir(GDO0_PIN, GPIO_OUT);
    
    // Enter TX mode
    cc1101_strobe(CC1101_STX);
    sleep_ms(1); // Wait for oscillator to stabilize

    // Replay the signal by toggling the pin
    // Assume signal starts LOW, so first pulse is HIGH
    bool level = true;
    for (int i = 0; i < count; i++) {
        gpio_put(GDO0_PIN, level);
        busy_wait_us_32(timings[i]);
        level = !level;
    }
    gpio_put(GDO0_PIN, 0); // Ensure pin is low after transmission

    // Return CC1101 to RX mode
    cc1101_strobe(CC1101_SIDLE);
    // Re-configure GDO0 as input on the Pico
    gpio_init(GDO0_PIN);
    gpio_set_dir(GDO0_PIN, GPIO_IN);
    gpio_pull_down(GDO0_PIN);
    // Re-enter RX mode
    cc1101_strobe(CC1101_SRX);

    uart_puts(UART_ID, "STATUS:Transmit complete\n");
}


int main()
{
    stdio_init_all();
    setup_uart();

    // SPI initialisation
    spi_init(SPI_PORT, 4 * 1000 * 1000); // 4MHz
    gpio_set_function(SPI_MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_SCLK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MOSI_PIN, GPIO_FUNC_SPI);

    // Chip select
    gpio_init(SPI_CSN_PIN);
    gpio_set_dir(SPI_CSN_PIN, GPIO_OUT);
    gpio_put(SPI_CSN_PIN, 1);
    
    printf("CC1101 Cloner Initializing...\n");

    // Initialize CC1101 and enter RX mode
    init_cc1101();
    cc1101_strobe(CC1101_SRX);
    
    // GDO0 pin setup
    gpio_pull_down(GDO0_PIN);
    // Interrupt will be disabled initially and enabled on command
    gpio_set_irq_enabled_with_callback(GDO0_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false, &gpio_callback);

    uart_puts(UART_ID, "STATUS:Ready\n");

    while (true) {
        // --- 1. Handle incoming commands from PC ---
        if (uart_is_readable(UART_ID)) {
            char cmd = uart_getc(UART_ID);

            if (cmd == 'c' && current_state == STATE_IDLE) {
                current_state = STATE_ARMED_TO_CAPTURE;
                pulse_count = 0;
                capture_done = false;
                last_edge_time = get_absolute_time();
                gpio_set_irq_enabled(GDO0_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
                uart_puts(UART_ID, "STATUS:Armed\n");
            } else if (cmd == 't' && current_state == STATE_IDLE) {
                transmit_signal(stored_pulse_timings, stored_pulse_count);
                // After transmitting, we are ready for a new command
                uart_puts(UART_ID, "STATUS:Ready\n");
            }
        }

        // --- 2. State Machine Logic ---
        if (current_state == STATE_ARMED_TO_CAPTURE) {
            // Timeout check
            if (!capture_done && pulse_count > 0 && absolute_time_diff_us(last_edge_time, get_absolute_time()) > END_OF_TRANSMISSION_US) {
                if (pulse_count > 10) {
                    capture_done = true;
                } else {
                    pulse_count = 0;
                }
            }

            // Process captured data
            if (capture_done) {
                gpio_set_irq_enabled(GDO0_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);

                // Store the captured signal
                memcpy(stored_pulse_timings, (void*)pulse_timings, pulse_count * sizeof(uint32_t));
                stored_pulse_count = pulse_count;

                // Send data to PC
                uart_puts(UART_ID, "DATA:");
                char uart_buf[16];
                for (int i = 0; i < pulse_count; i++) {
                    sprintf(uart_buf, "%lu", stored_pulse_timings[i]);
                    uart_puts(UART_ID, uart_buf);
                    if (i < pulse_count - 1) {
                        uart_puts(UART_ID, ",");
                    }
                }
                uart_puts(UART_ID, "\n");

                current_state = STATE_IDLE;
                uart_puts(UART_ID, "STATUS:Ready\n");
            }
        }

        sleep_ms(1);
    }

    return 0;
}
