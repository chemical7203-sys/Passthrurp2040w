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
    uint8_t report_payload[9] = {0};

    report_payload[0] = gamepad_data.buttons & 0xFF;
    report_payload[1] = (gamepad_data.buttons >> 8) & 0xFF;
    report_payload[2] = dpad_to_hat(gamepad_data.dpad);
    report_payload[3] = gamepad_data.lx + 128;
    report_payload[4] = gamepad_data.ly + 128;
    report_payload[5] = gamepad_data.l2;
    report_payload[6] = gamepad_data.r2;
    report_payload[7] = gamepad_data.rx + 128;
    report_payload[8] = gamepad_data.ry + 128;
    
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
