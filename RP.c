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
volatile bool capture_mode = false;
char uart_rx_buffer[128];
uint8_t uart_rx_index = 0;

// --- Function Prototypes ---
void setup_uart();
void handle_uart_command(char* command);
uint8_t hex_char_to_int(char c);
void hex_string_to_bytes(const char* hex_str, uint8_t* byte_array, uint8_t* byte_count);

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
    
    printf("CC1101 Initialized and Configured.\n");
    uart_puts(UART_ID, "CC1101 Ready.\n");

    while (true) {
        // Check for incoming UART commands
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

        // Check for received packet if in capture mode
        if (capture_mode) {
            // Check GDO0 pin. If it's high, a packet has been received.
            if (gpio_get(CC1101_PIN_GDO0)) {
                uint8_t bytes_in_fifo = cc1101_read_status_reg(CC1101_RXBYTES) & CC1101_NUM_RXBYTES;

                if (bytes_in_fifo > 0) {
                    uint8_t packet_buffer[64];
                    // First byte is length, second is address (if enabled), then data
                    cc1101_read_burst_reg(CC1101_RXFIFO, packet_buffer, bytes_in_fifo);

                    // Send data to PC, prefixed with 'R:'
                    uart_puts(UART_ID, "R:");
                    for (int i = 0; i < bytes_in_fifo; i++) {
                        char hex_byte[3];
                        sprintf(hex_byte, "%02X", packet_buffer[i]);
                        uart_puts(UART_ID, hex_byte);
                    }
                    uart_puts(UART_ID, "\n");
                }

                // Flush RX FIFO and re-enter RX mode
                cc1101_strobe(CC1101_SFRX);
                cc1101_strobe(CC1101_SRX);
            }
        }
        sleep_ms(10); // Small delay to prevent busy-waiting
    }

    return 0;
}

void handle_uart_command(char* command) {
    printf("Handling command: %s\n", command);
    switch(command[0]) {
        case 'C':
            if (!capture_mode) {
                printf("Entering Capture Mode\n");
                uart_puts(UART_ID, "OK: Capture Mode ON\n");
                cc1101_strobe(CC1101_SRX); // Enter RX mode
                capture_mode = true;
            }
            break;
        case 'E':
            if (capture_mode) {
                printf("Exiting Capture Mode\n");
                uart_puts(UART_ID, "OK: Capture Mode OFF\n");
                cc1101_strobe(CC1101_SIDLE); // Exit RX mode to IDLE
                capture_mode = false;
            }
            break;
        case 'T':
            if (command[1] == ',') {
                 // Format: T,HEXDATA
                const char* hex_data = command + 2;
                uint8_t data_to_send[64];
                uint8_t data_len = 0;

                hex_string_to_bytes(hex_data, data_to_send, &data_len);

                if (data_len > 0) {
                    printf("Transmitting %d bytes\n", data_len);
                    cc1101_strobe(CC1101_SIDLE);
                    cc1101_write_reg(CC1101_PKTLEN, data_len);
                    cc1101_write_burst_reg(CC1101_TXFIFO, data_to_send, data_len);
                    cc1101_strobe(CC1101_STX); // Enter TX mode

                    // Wait for TX to finish (optional, can check MARCSTATE or GDO pin)
                    sleep_ms(100);

                    uart_puts(UART_ID, "OK: Transmitted\n");
                } else {
                    uart_puts(UART_ID, "ERR: Invalid hex data\n");
                }
            } else {
                 uart_puts(UART_ID, "ERR: Invalid transmit format\n");
            }
            break;
        default:
            uart_puts(UART_ID, "ERR: Unknown command\n");
            break;
    }
}

uint8_t hex_char_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0; // Should not happen with valid hex string
}

void hex_string_to_bytes(const char* hex_str, uint8_t* byte_array, uint8_t* byte_count) {
    *byte_count = 0;
    size_t len = strlen(hex_str);
    if (len % 2 != 0) return; // Invalid hex string

    for (size_t i = 0; i < len; i += 2) {
        uint8_t high = hex_char_to_int(hex_str[i]);
        uint8_t low = hex_char_to_int(hex_str[i+1]);
        byte_array[*byte_count] = (high << 4) | low;
        (*byte_count)++;
        if (*byte_count >= 64) break;
    }
}
