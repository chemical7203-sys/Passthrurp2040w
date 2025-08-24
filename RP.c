#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/uart.h"
#include "tusb.h"
#include "bsp/board.h"

// Struct to hold the received v2 controller data from UART
typedef struct __attribute__((packed)) {
    uint16_t buttons;
    int8_t   lx, ly, rx, ry;
    uint8_t  l2, r2;
    uint8_t  dpad;
} gamepad_data_v2_t;

// Global instance to hold the latest controller data from UART
static gamepad_data_v2_t gamepad_data;

// --- Protocol and UART ---
#define PROTOCOL_V2_HEADER 0xA6
#define PROTOCOL_V2_SIZE 11
#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5

void setup_uart() {
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
}

void process_uart() {
    static uint8_t packet_buffer[PROTOCOL_V2_SIZE];
    static uint8_t buffer_idx = 0;
    while (uart_is_readable(UART_ID)) {
        uint8_t ch = uart_getc(UART_ID);
        if (buffer_idx == 0) {
            if (ch == PROTOCOL_V2_HEADER) packet_buffer[buffer_idx++] = ch;
        } else {
            packet_buffer[buffer_idx++] = ch;
            if (buffer_idx >= PROTOCOL_V2_SIZE) {
                uint8_t checksum = 0;
                for (int i = 0; i < PROTOCOL_V2_SIZE - 1; i++) checksum ^= packet_buffer[i];
                if (checksum == packet_buffer[PROTOCOL_V2_SIZE - 1]) {
                    gamepad_data.buttons = (uint16_t)packet_buffer[1] | ((uint16_t)packet_buffer[2] << 8);
                    gamepad_data.lx      = (int8_t)packet_buffer[3];
                    gamepad_data.ly      = (int8_t)packet_buffer[4];
                    gamepad_data.rx      = (int8_t)packet_buffer[5];
                    gamepad_data.ry      = (int8_t)packet_buffer[6];
                    gamepad_data.l2      = packet_buffer[7];
                    gamepad_data.r2      = packet_buffer[8];
                    gamepad_data.dpad    = packet_buffer[9];
                }
                buffer_idx = 0;
            }
        }
    }
}

// --- HID Task and Callbacks ---
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
  (void) instance; (void) report_id; (void) report_type; (void) buffer; (void) reqlen;
  return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
  (void) instance; (void) report_id; (void) report_type; (void) buffer; (void) bufsize;
}

uint8_t dpad_to_hat(uint8_t dpad_mask) {
    static const uint8_t hat_map[16] = { 8, 0, 4, 8, 6, 7, 5, 8, 2, 1, 3, 8, 8, 8, 8, 8 };
    return hat_map[dpad_mask & 0x0F];
}

void hid_task(void) {
  const uint32_t interval_ms = 8;
  static uint32_t start_ms = 0;

  if ( board_millis() - start_ms < interval_ms) return;
  start_ms += interval_ms;

  if ( tud_suspended() ) tud_remote_wakeup();

  if ( tud_hid_ready() ) {
    // Manually construct the report byte array to be 100% sure of the layout.
    // This avoids any C struct padding/alignment issues.
    // Layout must match the HID descriptor in usb_descriptors.c
    uint8_t report[10] = {0}; // 2 buttons, 1 hat, 6 axes = 9 bytes. Wait, size is 10?
    // Let's re-verify descriptor and size.
    // Descriptor: 16 buttons (2 bytes), 1 hat (1 byte), 6 axes (6 bytes) = 9 bytes total payload.
    // The HID Report ID is not part of the payload.
    uint8_t report_payload[9] = {0};

    // 1. Buttons (2 bytes)
    report_payload[0] = gamepad_data.buttons & 0xFF;
    report_payload[1] = (gamepad_data.buttons >> 8) & 0xFF;
    // 2. Hat (1 byte)
    report_payload[2] = dpad_to_hat(gamepad_data.dpad);
    // 3. Axes (LX, LY, RX, RY) (4 bytes)
    report_payload[3] = gamepad_data.lx;
    report_payload[4] = gamepad_data.ly;
    report_payload[5] = gamepad_data.rx;
    report_payload[6] = gamepad_data.ry;
    // 4. Triggers (Z, RZ) (2 bytes)
    report_payload[7] = gamepad_data.l2;
    report_payload[8] = gamepad_data.r2;
    
    tud_hid_report(1, report_payload, sizeof(report_payload));
  }
}

// --- Main ---
int main() {
    board_init();
    setup_uart();
    tusb_init();

    while (true) {
        tud_task();
        hid_task();
        process_uart();
    }
    return 0;
}
