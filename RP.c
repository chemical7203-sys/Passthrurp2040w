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
#define SAMPLE_RATE_US 2
#define SAMPLE_BUFFER_SIZE 32768 // 32KB buffer
#define TOTAL_SAMPLES (SAMPLE_BUFFER_SIZE * 8)
#define PULSE_BUFFER_SIZE 1024

volatile bool capture_mode = false;
char uart_rx_buffer[8192];
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

        sleep_ms(10);
    }
    return 0;
}

void handle_uart_command(char* command) {
    printf("Handling command: %s\n", command);
    if (strcmp(command, "C") == 0) {
        if (!capture_mode) {
            uart_puts(UART_ID, "OK: Recording... Press remote button now.\n");
            capture_mode = true;
        }
    } else if (command[0] == 'P' && command[1] == ',') {
        transmit_pulses(command + 2);
    } else {
        uart_puts(UART_ID, "ERR: Unknown command. Supported: 'C', 'P,data'\n");
    }
}

void capture_and_process_signal() {
    // --- 1. Record Phase ---
    cc1101_strobe(CC1101_SRX);
    memset(sample_buffer, 0, SAMPLE_BUFFER_SIZE);
    printf("Starting raw recording...\n");

    // Blindly record for the duration of the buffer
    for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
        uint8_t byte = 0;
        for (int j = 0; j < 8; j++) {
            byte |= (gpio_get(CC1101_PIN_GDO0) << (7-j));
            busy_wait_us_32(SAMPLE_RATE_US);
        }
        sample_buffer[i] = byte;
    }

    cc1101_strobe(CC1101_SIDLE);
    printf("Recording complete. Analyzing %d samples...\n", TOTAL_SAMPLES);

    // --- 2. Analyze Phase ---
    uint16_t pulse_count = 0;
    int start_index = -1;
    bool idle_state_is_low = true; // Assume idle is low, can be improved later

    // Find first long period of idle to filter out initial noise
    int consecutive_idle_samples = 0;
    int first_signal_edge = -1;
    bool last_sample_state = (sample_buffer[0] >> 7) & 1;

    for (int i = 0; i < TOTAL_SAMPLES; i++) {
        bool current_sample_state = (sample_buffer[i / 8] >> (7 - (i % 8))) & 1;
        if (current_sample_state == !idle_state_is_low) { // Found potential signal
            if (consecutive_idle_samples > 5000 / SAMPLE_RATE_US) { // Found >5ms of silence
                first_signal_edge = i;
                break;
            }
        } else {
            consecutive_idle_samples++;
        }
    }

    if (first_signal_edge == -1) {
        uart_puts(UART_ID, "ERR: No signal found in recording.\n");
        return;
    }

    printf("Signal start detected at sample %d. Processing pulses.\n", first_signal_edge);

    // --- 3. Process Phase (Run-length encoding) ---
    bool current_run_state = (sample_buffer[first_signal_edge / 8] >> (7 - (first_signal_edge % 8))) & 1;
    uint32_t current_pulse_length = 0;

    for (int i = first_signal_edge; i < TOTAL_SAMPLES; i++) {
        bool sample = (sample_buffer[i / 8] >> (7 - (i % 8))) & 1;
        if (sample != current_run_state) {
            if (pulse_count < PULSE_BUFFER_SIZE) {
                pulse_buffer[pulse_count++] = current_pulse_length * SAMPLE_RATE_US;
            } else {
                break; // Pulse buffer full
            }
            current_pulse_length = 0;
            current_run_state = sample;
        }
        current_pulse_length++;
    }
    // Add the last pulse
    if (pulse_count < PULSE_BUFFER_SIZE) {
        pulse_buffer[pulse_count++] = current_pulse_length * SAMPLE_RATE_US;
    }

    // --- 4. Report Phase ---
    if (pulse_count > 0) {
        char temp_buf[20];
        uart_puts(UART_ID, "PULSE:");
        for (int i = 0; i < pulse_count; i++) {
            sprintf(temp_buf, "%lu,", pulse_buffer[i]);
            uart_puts(UART_ID, temp_buf);
        }
        uart_puts(UART_ID, "\n");
    } else {
        uart_puts(UART_ID, "ERR: No pulses found after signal start.\n");
    }
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
