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
#define SAMPLE_RATE_US 2 // Sample every 2 microseconds (500 KHz)
#define SAMPLE_BUFFER_SIZE 32768 // 32KB buffer -> ~0.5 seconds of capture
#define PULSE_BUFFER_SIZE 1024

volatile bool capture_mode = false;
char uart_rx_buffer[8192]; // Large buffer for receiving pulse train for TX
uint16_t uart_rx_index = 0;
uint8_t sample_buffer[SAMPLE_BUFFER_SIZE];
uint32_t pulse_buffer[PULSE_BUFFER_SIZE];


// --- Function Prototypes ---
void setup_uart();
void handle_uart_command(char* command);
void capture_and_process_signal();
void transmit_pulses(char* data);

// A safer way to get the next token from a comma-separated string
char* safe_strtok(char** str, const char* delim) {
    if (*str == NULL) return NULL;
    char* token_start = *str;
    *str = strpbrk(token_start, delim);
    if (*str) {
        **str = '\0';
        (*str)++;
    }
    return token_start;
}

void setup_uart() {
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_puts(UART_ID, "\nRP2040 Signal Cloner Ready.\n");
}

int main()
{
    stdio_init_all();
    printf("RP2040 Booting...\n");
    setup_uart();
    cc1101_init();
    cc1101_configure();
    printf("CC1101 Initialized.\n");

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
            capture_and_process_signal();
            capture_mode = false; // Auto-stop after capture
        }

        sleep_ms(10); // Yield for a moment
    }
    return 0;
}

void handle_uart_command(char* command) {
    printf("Handling command: %s\n", command);
    if (strcmp(command, "C") == 0) {
        if (!capture_mode) {
            uart_puts(UART_ID, "OK: Starting capture... Press remote.\n");
            capture_mode = true;
        }
    } else if (command[0] == 'P' && command[1] == ',') {
        transmit_pulses(command + 2);
    } else {
        uart_puts(UART_ID, "ERR: Unknown command. Supported: 'C', 'P,data'\n");
    }
}

void capture_and_process_signal() {
    cc1101_strobe(CC1101_SRX);
    memset(sample_buffer, 0, SAMPLE_BUFFER_SIZE);

    // Wait for the first rising edge to start recording (with timeout)
    uint32_t start_time = time_us_32();
    while(!gpio_get(CC1101_PIN_GDO0)) {
        if (time_us_32() - start_time > 2000000) {
            uart_puts(UART_ID, "ERR: Capture timed out waiting for signal.\n");
            cc1101_strobe(CC1101_SIDLE);
            return;
        }
    }
    printf("Signal trigger detected.\n");

    // --- High-speed sampling loop ---
    for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
        uint8_t byte = 0;
        for (int j = 0; j < 8; j++) {
            byte |= (gpio_get(CC1101_PIN_GDO0) << (7-j));
            busy_wait_us_32(SAMPLE_RATE_US);
        }
        sample_buffer[i] = byte;
    }

    cc1101_strobe(CC1101_SIDLE);
    printf("Sampling complete. Processing...\n");

    // --- On-device processing: samples to pulse widths ---
    uint16_t pulse_count = 0;
    bool last_state = (sample_buffer[0] >> 7) & 1;
    uint32_t current_pulse_length = 0;

    for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
        for (int j = 7; j >= 0; j--) {
            bool current_state = (sample_buffer[i] >> j) & 1;
            if (current_state != last_state) {
                if (pulse_count < PULSE_BUFFER_SIZE) {
                    pulse_buffer[pulse_count++] = current_pulse_length * SAMPLE_RATE_US;
                } else {
                    goto end_processing; // Buffer full
                }
                current_pulse_length = 0;
                last_state = current_state;
            }
            current_pulse_length++;
        }
    }
    // Add the last pulse
    if (pulse_count < PULSE_BUFFER_SIZE) {
        pulse_buffer[pulse_count++] = current_pulse_length * SAMPLE_RATE_US;
    }

end_processing:
    printf("Processing complete. Found %d pulses.\n", pulse_count);

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
        uart_puts(UART_ID, "ERR: No pulses captured after trigger.\n");
    }
    uart_puts(UART_ID, "OK: Capture Finished\n");
}

void transmit_pulses(char* data) {
    uart_puts(UART_ID, "OK: Transmitting pulses...\n");
    gpio_init(CC1101_PIN_GDO0);
    gpio_set_dir(CC1101_PIN_GDO0, GPIO_OUT);
    cc1101_strobe(CC1101_STX);

    bool state = true;
    char* p = data;
    char* token;
    while((token = safe_strtok(&p, ",")) != NULL) {
        if (*token == '\0') continue;
        uint32_t duration = atoi(token);
        if (duration > 0) {
            gpio_put(CC1101_PIN_GDO0, state);
            busy_wait_us_32(duration);
            state = !state;
        }
    }

    gpio_put(CC1101_PIN_GDO0, 0);
    cc1101_strobe(CC1101_SIDLE);
    gpio_init(CC1101_PIN_GDO0);
    gpio_set_dir(CC1101_PIN_GDO0, GPIO_IN);
    uart_puts(UART_ID, "OK: Transmission finished.\n");
}
