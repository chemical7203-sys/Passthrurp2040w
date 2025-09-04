#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "cc1101_driver.h"
#include "cc1101_constants.h"

// UART defines for communication with PC
#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5

// --- Global State ---
#define PULSE_BUFFER_SIZE 512
volatile bool capture_mode = false;
volatile bool rssi_mode = false;
char uart_rx_buffer[128];
uint8_t uart_rx_index = 0;
uint32_t pulse_buffer[PULSE_BUFFER_SIZE];
uint16_t pulse_count = 0;


// --- Function Prototypes ---
void setup_uart();
void handle_uart_command(char* command);
void dump_registers();
void capture_pulses();
int16_t convert_rssi(uint8_t rssi_dec);

void setup_uart() {
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_puts(UART_ID, "\nRP2040 Initialized. Ready for commands.\n");
}

int main()
{
    stdio_init_all();
    printf("RP2040 Booting...\n");

    setup_uart();
    cc1101_init();
    cc1101_configure();
    
    printf("CC1101 Initialized and Configured for Raw Mode.\n");
    uart_puts(UART_ID, "CC1101 Ready.\n");

    while (true) {
        if (uart_is_readable(UART_ID)) {
            char c = uart_getc(UART_ID);
            if (c == '\n' || c == '\r') {
                if (uart_rx_index > 0) {
                    uart_rx_buffer[uart_rx_index] = '\0';
                    handle_uart_command(uart_rx_buffer);
                    uart_rx_index = 0;
                }
            } else if (uart_rx_index < sizeof(uart_rx_buffer) - 1) {
                uart_rx_buffer[uart_rx_index++] = c;
            }
        }

        if (capture_mode) {
            capture_pulses();
            // After capture is done, exit capture mode automatically
            capture_mode = false;
            uart_puts(UART_ID, "OK: Capture Finished\n");
            printf("Capture finished.\n");
        } else if (rssi_mode) {
            cc1101_strobe(CC1101_SIDLE);
            sleep_us(500);
            cc1101_strobe(CC1101_SRX);
            sleep_us(500);

            uint8_t rssi_raw = cc1101_read_status_reg(CC1101_RSSI);
            int16_t rssi_dbm = convert_rssi(rssi_raw);
            char rssi_msg[32];
            sprintf(rssi_msg, "RSSI_DBM:%d\n", rssi_dbm);
            uart_puts(UART_ID, rssi_msg);
            sleep_ms(100);
        } else {
            sleep_ms(10);
        }
    }
    return 0;
}

void handle_uart_command(char* command) {
    printf("Handling command: %s\n", command);
    switch(command[0]) {
        case 'C':
            if (!capture_mode && !rssi_mode) {
                printf("Entering Raw Pulse Capture Mode\n");
                uart_puts(UART_ID, "OK: Capturing raw pulses... Press remote.\n");
                capture_mode = true;
            }
            break;
        case 'S':
             if (!capture_mode && !rssi_mode) {
                printf("Entering RSSI Mode\n");
                uart_puts(UART_ID, "OK: RSSI Mode ON\n");
                rssi_mode = true;
            }
            break;
        case 'E':
            if (rssi_mode) { // Only 'E' for RSSI mode now
                printf("Exiting RSSI Mode\n");
                uart_puts(UART_ID, "OK: Mode OFF\n");
                cc1101_strobe(CC1101_SIDLE);
                rssi_mode = false;
            }
            break;
        case 'D':
            dump_registers();
            break;
        // 'T' command is temporarily disabled as it needs new logic for pulse trains
        default:
            uart_puts(UART_ID, "ERR: Unknown or disabled command\n");
            break;
    }
}

void capture_pulses() {
    pulse_count = 0;

    // Put radio in RX mode
    cc1101_strobe(CC1101_SRX);

    // Wait for the first edge (transition from idle low to high)
    // Timeout after 2 seconds if no signal
    uint32_t start_time = time_us_32();
    while(!gpio_get(CC1101_PIN_GDO0)) {
        if (time_us_32() - start_time > 2000000) {
            uart_puts(UART_ID, "ERR: Capture timed out waiting for signal.\n");
            cc1101_strobe(CC1101_SIDLE);
            return;
        }
    }

    // Start capturing edges
    bool current_state = gpio_get(CC1101_PIN_GDO0);
    uint32_t last_edge_time = time_us_32();

    // Capture for a max of 3 seconds or until buffer is full
    while(pulse_count < PULSE_BUFFER_SIZE && (time_us_32() - start_time < 5000000)) {
        bool new_state = gpio_get(CC1101_PIN_GDO0);
        if (new_state != current_state) {
            uint32_t now = time_us_32();
            uint32_t duration = now - last_edge_time;
            pulse_buffer[pulse_count++] = duration;
            last_edge_time = now;
            current_state = new_state;
        }
        // Timeout between edges (end of transmission)
        if (time_us_32() - last_edge_time > 100000) { // 100ms
            break;
        }
    }

    cc1101_strobe(CC1101_SIDLE);

    // Send captured pulse data to PC
    if (pulse_count > 0) {
        char temp_buf[20];
        uart_puts(UART_ID, "PULSE:");
        for (int i = 0; i < pulse_count; i++) {
            sprintf(temp_buf, "%lu,", pulse_buffer[i]);
            uart_puts(UART_ID, temp_buf);
        }
        uart_puts(UART_ID, "\n");
    } else {
        uart_puts(UART_ID, "ERR: No pulses captured.\n");
    }
}


void dump_registers() {
    uart_puts(UART_ID, "--- CC1101 Registers ---\n");
    char line[40];
    for (uint8_t i = 0x00; i <= 0x2E; i++) {
        uint8_t value = cc1101_read_reg(i);
        sprintf(line, "Reg 0x%02X: 0x%02X\n", i, value);
        uart_puts(UART_ID, line);
    }
    uart_puts(UART_ID, "------------------------\n");
}

int16_t convert_rssi(uint8_t rssi_raw) {
    int16_t rssi_dbm;
    if (rssi_raw >= 128) {
        rssi_dbm = (int16_t)((int16_t)(rssi_raw - 256) / 2) - 74;
    } else {
        rssi_dbm = (rssi_raw / 2) - 74;
    }
    return rssi_dbm;
}
