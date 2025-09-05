#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
#define SAMPLE_BUFFER_SIZE 32768 // 32KB buffer for samples
volatile bool capture_mode = false;
char uart_rx_buffer[128]; // Command buffer is small
uint16_t uart_rx_index = 0;
uint8_t sample_buffer[SAMPLE_BUFFER_SIZE];


// --- Function Prototypes ---
void setup_uart();
void handle_uart_command(char* command);
void capture_samples();

void setup_uart() {
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_puts(UART_ID, "\nRP2040 Initialized. Ready for commands.\n");
}

int main()
{
    stdio_init_all();
    // Set system clock to max for faster sampling
    // set_sys_clock_khz(250000, true);
    printf("RP2040 Booting...\n");
    setup_uart();
    cc1101_init();
    cc1101_configure();
    printf("CC1101 Initialized for Raw Sampling Mode.\n");
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
            capture_samples();
            capture_mode = false; // Auto-stop after capture
            uart_puts(UART_ID, "OK: Sampling Finished\n");
            printf("Sampling finished.\n");
        }

        sleep_ms(10); // Yield for a moment
    }
    return 0;
}

void handle_uart_command(char* command) {
    printf("Handling command: %s\n", command);
    if (strcmp(command, "C") == 0) {
        if (!capture_mode) {
            uart_puts(UART_ID, "OK: Starting digital sampling... Press remote.\n");
            capture_mode = true;
        }
    } else {
        uart_puts(UART_ID, "ERR: Unknown command. Only 'C' is supported in this version.\n");
    }
}

void capture_samples() {
    // Prepare for capture
    cc1101_strobe(CC1101_SRX);
    memset(sample_buffer, 0, SAMPLE_BUFFER_SIZE);

    // Wait for the first rising edge to start recording (with timeout)
    uint32_t start_time = time_us_32();
    while(!gpio_get(CC1101_PIN_GDO0)) {
        if (time_us_32() - start_time > 2000000) { // 2 second timeout
            uart_puts(UART_ID, "ERR: Capture timed out waiting for signal.\n");
            cc1101_strobe(CC1101_SIDLE);
            return;
        }
    }

    // --- High-speed sampling loop ---
    // We will sample for a fixed duration determined by the buffer size and sample rate.
    // Sample rate is approx 1MHz (1us per sample)
    uint32_t sample_end_time = time_us_32() + (SAMPLE_BUFFER_SIZE * 8);

    for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
        uint8_t byte = 0;
        for (int j = 0; j < 8; j++) {
            byte |= (gpio_get(CC1101_PIN_GDO0) << (7-j));
            busy_wait_us(1); // Wait for 1 microsecond
        }
        sample_buffer[i] = byte;
        // Check for early exit if signal ends
        if (time_us_32() > sample_end_time) break;
    }

    cc1101_strobe(CC1101_SIDLE);

    // Send the captured buffer to PC as a hex string
    uart_puts(UART_ID, "SAMPLES:");
    char hex_byte[3];
    for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
        sprintf(hex_byte, "%02X", sample_buffer[i]);
        uart_puts(UART_ID, hex_byte);
    }
    uart_puts(UART_ID, "\n");
}
