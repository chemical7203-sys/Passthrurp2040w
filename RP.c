#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/uart.h"
#include "tusb.h"
#include "bsp/board.h"
#include "class/hid/hid_device.h" // Required for Switch mode structs and enums

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
                    memcpy(&gamepad_data, &packet_buffer[1], sizeof(gamepad_data));
                }
                buffer_idx = 0;
            }
        }
    }
}

// --- HID Task and Callbacks ---
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) { return 0; }
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) { }

uint8_t dpad_to_switch_hat(uint8_t dpad_mask) {
    static const uint8_t hat_map[16] = {
        SWITCH_HAT_NOTHING, SWITCH_HAT_UP, SWITCH_HAT_DOWN, SWITCH_HAT_NOTHING,
        SWITCH_HAT_LEFT, SWITCH_HAT_UP_LEFT, SWITCH_HAT_DOWN_LEFT, SWITCH_HAT_NOTHING,
        SWITCH_HAT_RIGHT, SWITCH_HAT_UP_RIGHT, SWITCH_HAT_DOWN_RIGHT, SWITCH_HAT_NOTHING,
        SWITCH_HAT_NOTHING, SWITCH_HAT_NOTHING, SWITCH_HAT_NOTHING, SWITCH_HAT_NOTHING
    };
    return hat_map[dpad_mask & 0x0F];
}

void hid_task(void) {
  const uint32_t interval_ms = 5;
  static uint32_t start_ms = 0;
  if ( board_millis() - start_ms < interval_ms) return;
  start_ms += interval_ms;

  if ( tud_suspended() ) tud_remote_wakeup();

  if ( tud_hid_ready() ) {
    #if INPUT_MODE_SWITCH
      hid_nintendo_report_t report = {0};
      report.hat = dpad_to_switch_hat(gamepad_data.dpad);
      report.lx = gamepad_data.lx + 128;
      report.ly = gamepad_data.ly + 128;
      report.rx = gamepad_data.rx + 128;
      report.ry = gamepad_data.ry + 128;
      if (gamepad_data.buttons & (1<<0)) report.buttons |= SWITCH_MASK_B;
      if (gamepad_data.buttons & (1<<1)) report.buttons |= SWITCH_MASK_A;
      // ... full button mapping needed here
      tud_hid_report(0, &report, sizeof(report));
    #elif INPUT_MODE_DS4
      // TODO: Implement DS4 report
      uint8_t report[1] = {0};
      tud_hid_report(0, report, sizeof(report));
    #else // GENERIC
      hid_gamepad_report_t report = {0}; // Assumes standard gamepad report
      // ... mapping for generic
      tud_hid_report(1, &report, sizeof(report));
    #endif
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
