#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"

/*
 * =================================================================================
 * Communication Protocol v1
 * =================================================================================
 * A simple 5-byte packet sent from PC to Pico over UART.
 *
 * - Byte 0: Header (0xA5)
 * - Byte 1: Button State (uint8_t)
 *   - Bit 0: Button 1 state (1 for pressed, 0 for released)
 *   - Bits 1-7: Reserved for future use
 * - Byte 2: Joystick X (int8_t, -127 to 127)
 * - Byte 3: Joystick Y (int8_t, -127 to 127)
 * - Byte 4: Checksum (uint8_t)
 *   - Calculated as: (Byte 0 ^ Byte 1 ^ Byte 2 ^ Byte 3)
 * =================================================================================
 */
#define PROTOCOL_HEADER 0xA5
#define PACKET_SIZE 5

// Struct to hold the received controller data
typedef struct {
    uint8_t button_state;
    int8_t joy_x;
    int8_t joy_y;
} controller_data_t;


// Use UART1 for communication, pins 4 and 5
#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5

// Function to set up UART
void setup_uart() {
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    printf("UART Initialized\n");
}

// This function will be called repeatedly in the main loop
void process_uart() {
    static uint8_t packet_buffer[PACKET_SIZE];
    static uint8_t buffer_idx = 0;

    // Process all available bytes from UART
    while (uart_is_readable(UART_ID)) {
        uint8_t ch = uart_getc(UART_ID);

        // The first byte of a packet must be the header.
        // If we're not at the start of a packet, we wait for a header byte.
        if (buffer_idx == 0) {
            if (ch == PROTOCOL_HEADER) {
                packet_buffer[buffer_idx++] = ch;
            }
        } else {
            // We are already building a packet, so store the next byte.
            packet_buffer[buffer_idx++] = ch;

            // If the packet is complete (all bytes received)
            if (buffer_idx >= PACKET_SIZE) {
                // Calculate checksum from the first 4 bytes
                uint8_t calculated_checksum = packet_buffer[0] ^ packet_buffer[1] ^ packet_buffer[2] ^ packet_buffer[3];
                uint8_t received_checksum = packet_buffer[4];

                // Validate checksum
                if (calculated_checksum == received_checksum) {
                    // Checksum is valid, parse the data
                    controller_data_t data;
                    data.button_state = packet_buffer[1];
                    data.joy_x = (int8_t)packet_buffer[2];
                    data.joy_y = (int8_t)packet_buffer[3];

                    // Print parsed data for debugging
                    printf("OK: Buttons=0x%02X, X=%d, Y=%d\n",
                           data.button_state, data.joy_x, data.joy_y);
                } else {
                    // Checksum failed
                    printf("Error: Checksum failed!\n");
                }

                // Reset buffer index to wait for the next packet header
                buffer_idx = 0;
            }
        }
    }
}

int main()
{
    // Initialize stdio for debugging output over USB
    stdio_init_all();
    
    // Setup UART for communication with PC
    setup_uart();

    // Main loop
    while (true) {
        // Continuously process incoming UART data
        process_uart();
    }

    return 0; // Should not be reached
}
