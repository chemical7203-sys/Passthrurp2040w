#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/uart.h"
#include "tusb.h"
#include "bsp/board.h"

// D-Pad Hat switch standard values
typedef enum {
    HAT_SWITCH_NEUTRAL = 8,
    HAT_SWITCH_UP = 0,
    HAT_SWITCH_UP_RIGHT = 1,
    HAT_SWITCH_RIGHT = 2,
    HAT_SWITCH_DOWN_RIGHT = 3,
    HAT_SWITCH_DOWN = 4,
    HAT_SWITCH_DOWN_LEFT = 5,
    HAT_SWITCH_LEFT = 6,
    HAT_SWITCH_UP_LEFT = 7,
} hat_switch_t;

// Struct to hold the received v2 controller data
typedef struct __attribute__((packed)) {
    uint16_t buttons;
    int8_t lx, ly, rx, ry;
    uint8_t l2, r2;
    uint8_t dpad;
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
                    // Checksum OK, parse the packet into the global state manually
                    gamepad_data.buttons = (uint16_t)packet_buffer[1] | ((uint16_t)packet_buffer[2] << 8);
                    gamepad_data.lx      = (int8_t)packet_buffer[3];
                    gamepad_data.ly      = (int8_t)packet_buffer[4];
                    gamepad_data.rx      = (int8_t)packet_buffer[5];
                    gamepad_data.ry      = (int8_t)packet_buffer[6];
                    gamepad_data.l2      = packet_buffer[7];
                    gamepad_data.r2      = packet_buffer[8];
                    gamepad_data.dpad    = packet_buffer[9];

                    // Send parsed data back for debugging
                    char debug_buf[128];
                    sprintf(debug_buf, "Rcvd: B:%04x LX:%d LY:%d RX:%d RY:%d L2:%u R2:%u D:%u\r\n",
                        gamepad_data.buttons, gamepad_data.lx, gamepad_data.ly,
                        gamepad_data.rx, gamepad_data.ry, gamepad_data.l2,
                        gamepad_data.r2, gamepad_data.dpad);
                    uart_puts(UART_ID, debug_buf);
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
    // DPAD_UP:1, DPAD_DOWN:2, DPAD_LEFT:4, DPAD_RIGHT:8
    static const uint8_t hat_map[16] = {
        HAT_SWITCH_NEUTRAL,    // 0000
        HAT_SWITCH_UP,         // 0001
        HAT_SWITCH_DOWN,       // 0010
        HAT_SWITCH_NEUTRAL,    // 0011 -> U+D not possible
        HAT_SWITCH_LEFT,       // 0100
        HAT_SWITCH_UP_LEFT,    // 0101
        HAT_SWITCH_DOWN_LEFT,  // 0110
        HAT_SWITCH_NEUTRAL,    // 0111 -> L+R not possible
        HAT_SWITCH_RIGHT,      // 1000
        HAT_SWITCH_UP_RIGHT,   // 1001
        HAT_SWITCH_DOWN_RIGHT, // 1010
        HAT_SWITCH_NEUTRAL,    // 1011
        HAT_SWITCH_NEUTRAL,    // 1100 -> L+R not possible
        HAT_SWITCH_NEUTRAL,    // 1101
        HAT_SWITCH_NEUTRAL,    // 1110
        HAT_SWITCH_NEUTRAL,    // 1111
    };
    return hat_map[dpad_mask & 0x0F];
}

void hid_task(void) {
  const uint32_t interval_ms = 8;
  static uint32_t start_ms = 0;

  if ( board_millis() - start_ms < interval_ms) return;
  start_ms += interval_ms;

  if ( tud_suspended() ) tud_remote_wakeup();

  if ( tud_hid_ready() ) {
    // Use a temporary byte array for the report to avoid padding/alignment issues
    uint8_t report[9] = {0};
    uint16_t buttons = gamepad_data.buttons;
    uint8_t hat = dpad_to_hat(gamepad_data.dpad);

    // Manually construct the report according to the HID descriptor
    // 1. Buttons (2 bytes)
    report[0] = buttons & 0xFF;
    report[1] = (buttons >> 8) & 0xFF;
    // 2. Hat (1 byte)
    report[2] = hat;
    // 3. Axes (4 bytes)
    report[3] = gamepad_data.lx;
    report[4] = gamepad_data.ly;
    report[5] = gamepad_data.rx;
    report[6] = gamepad_data.ry;
    // 4. Triggers (2 bytes)
    report[7] = gamepad_data.l2;
    report[8] = gamepad_data.r2;
    
    tud_hid_report(1, report, sizeof(report));
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
