#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/uart.h"
#include "tusb.h"
#include "bsp/board.h"

// D-Pad Hat switch values
typedef enum {
    HAT_SWITCH_NEUTRAL = 0,
    HAT_SWITCH_UP,
    HAT_SWITCH_UP_RIGHT,
    HAT_SWITCH_RIGHT,
    HAT_SWITCH_DOWN_RIGHT,
    HAT_SWITCH_DOWN,
    HAT_SWITCH_DOWN_LEFT,
    HAT_SWITCH_LEFT,
    HAT_SWITCH_UP_LEFT,
} hat_switch_t;

// Struct for the HID report that we send to the host
// The order of members MUST MATCH the HID report descriptor in usb_descriptors.c
typedef struct __attribute__((packed)) {
    uint16_t buttons;    // 16 buttons
    uint8_t hat;         // D-Pad
    int8_t x, y, rx, ry; // 4 axes
    uint8_t z, rz;       // 2 triggers
} hid_report_t;

// Struct to hold the received v2 controller data
typedef struct __attribute__((packed)) {
    uint16_t buttons;
    int8_t lx, ly, rx, ry;
    uint8_t l2, r2;
    uint8_t dpad;
} gamepad_data_v2_t;

// Global instance to hold the latest controller data from UART
static gamepad_data_v2_t gamepad_data;

// Global instance of the HID report to be sent
static hid_report_t hid_report;

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
                    // to avoid any potential struct padding/alignment issues.
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
    if (dpad_mask == 0b0001) return HAT_SWITCH_UP;
    if (dpad_mask == 0b1001) return HAT_SWITCH_UP_RIGHT;
    if (dpad_mask == 0b1000) return HAT_SWITCH_RIGHT;
    if (dpad_mask == 0b1010) return HAT_SWITCH_DOWN_RIGHT;
    if (dpad_mask == 0b0010) return HAT_SWITCH_DOWN;
    if (dpad_mask == 0b0110) return HAT_SWITCH_DOWN_LEFT;
    if (dpad_mask == 0b0100) return HAT_SWITCH_LEFT;
    if (dpad_mask == 0b0101) return HAT_SWITCH_UP_LEFT;
    return HAT_SWITCH_NEUTRAL;
}

void hid_task(void) {
  const uint32_t interval_ms = 5; // Send report more frequently
  static uint32_t start_ms = 0;

  if ( board_millis() - start_ms < interval_ms) return;
  start_ms += interval_ms;

  if ( tud_suspended() ) tud_remote_wakeup();

  if ( tud_hid_ready() ) {
    // Map the UART data to the HID report
    hid_report.x = gamepad_data.lx;
    hid_report.y = gamepad_data.ly;
    hid_report.rx = gamepad_data.rx;
    hid_report.ry = gamepad_data.ry;
    hid_report.z = gamepad_data.l2;
    hid_report.rz = gamepad_data.r2;
    hid_report.buttons = gamepad_data.buttons;
    hid_report.hat = dpad_to_hat(gamepad_data.dpad);
    
    tud_hid_report(1, &hid_report, sizeof(hid_report));
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
