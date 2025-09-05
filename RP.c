#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/uart.h"
#include "tusb.h"
#include "bsp/board.h"
#include "class/hid/hid_device.h"

#if CFG_TUD_HID_SONY
#include "ds4_report.h"
#elif CFG_TUD_HID_NINTENDO
#include "switch_report.h"
#endif

// Struct to hold the received v3 controller data from UART
typedef struct __attribute__((packed)) {
    uint16_t buttons;
    int8_t   lx, ly, rx, ry;
    uint8_t  l2, r2;
    uint8_t  dpad;
    int16_t  accel_x, accel_y, accel_z;
    int16_t  gyro_x, gyro_y, gyro_z;
} gamepad_data_v3_t;

static gamepad_data_v3_t gamepad_data;
static uint8_t report_counter = 0;

#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5
#define V3_PACKET_LEN 23 // 1 header + 21 payload + 1 checksum
#define V3_PAYLOAD_LEN 21
#define V3_HEADER 0xA7

void setup_uart() {
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
}

void process_uart() {
    static uint8_t pb[V3_PACKET_LEN];
    static uint8_t idx = 0;
    while (uart_is_readable(UART_ID)) {
        uint8_t ch = uart_getc(UART_ID);
        if (idx == 0) {
            if (ch == V3_HEADER) {
                pb[idx++] = ch;
            }
        } else {
            pb[idx++] = ch;
            if (idx >= V3_PACKET_LEN) {
                uint8_t cs = 0;
                for (int i = 0; i < V3_PACKET_LEN - 1; i++) {
                    cs ^= pb[i];
                }
                if (cs == pb[V3_PACKET_LEN - 1]) {
                    memcpy(&gamepad_data, &pb[1], sizeof(gamepad_data));
                }
                idx = 0;
            }
        }
    }
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
  (void) instance; (void) report_id; (void) report_type; (void) buffer; (void) reqlen;
  return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
  (void) instance; (void) report_id; (void) report_type; (void) buffer; (void) bufsize;
}

#if CFG_TUD_HID_NINTENDO
uint8_t dpad_to_switch_hat(uint8_t dpad_mask) {
    static const uint8_t hat_map[16] = {
        SWITCH_HAT_NOTHING, SWITCH_HAT_UP, SWITCH_HAT_DOWN, SWITCH_HAT_NOTHING,
        SWITCH_HAT_LEFT, SWITCH_HAT_UPLEFT, SWITCH_HAT_DOWNLEFT, SWITCH_HAT_NOTHING,
        SWITCH_HAT_RIGHT, SWITCH_HAT_UPRIGHT, SWITCH_HAT_DOWNRIGHT, SWITCH_HAT_NOTHING,
        SWITCH_HAT_NOTHING, SWITCH_HAT_NOTHING, SWITCH_HAT_NOTHING, SWITCH_HAT_NOTHING
    };
    return hat_map[dpad_mask & 0x0F];
}
#endif

void hid_task(void) {
  const uint32_t interval_ms = 5;
  static uint32_t start_ms = 0;
  if ( board_millis() - start_ms < interval_ms) return;
  start_ms += interval_ms;

  if ( tud_suspended() ) tud_remote_wakeup();

  if ( tud_hid_ready() ) {
    #if CFG_TUD_HID_NINTENDO
      hid_nintendo_report_t report = {0};
      report.report_id = 0x30;
      report.timer = report_counter++;

      // Button mapping
      uint16_t buttons = gamepad_data.buttons;
      report.buttons_right = 0;
      report.buttons_mid = 0;
      report.buttons_left = 0; // Always 0

      if (buttons & (1 << 0)) report.buttons_right |= SWITCH_MASK_B;      // South
      if (buttons & (1 << 1)) report.buttons_right |= SWITCH_MASK_A;      // East
      if (buttons & (1 << 2)) report.buttons_right |= SWITCH_MASK_Y;      // West
      if (buttons & (1 << 3)) report.buttons_right |= SWITCH_MASK_X;      // North
      if (buttons & (1 << 4)) report.buttons_right |= SWITCH_MASK_L;      // L
      if (buttons & (1 << 5)) report.buttons_right |= SWITCH_MASK_R;      // R
      if (gamepad_data.l2 > 30) report.buttons_right |= SWITCH_MASK_ZL;
      if (gamepad_data.r2 > 30) report.buttons_right |= SWITCH_MASK_ZR;
      if (buttons & (1 << 8)) report.buttons_mid |= SWITCH_MASK_MINUS;
      if (buttons & (1 << 9)) report.buttons_mid |= SWITCH_MASK_PLUS;
      if (buttons & (1 << 10)) report.buttons_mid |= SWITCH_MASK_L3;
      if (buttons & (1 << 11)) report.buttons_mid |= SWITCH_MASK_R3;
      if (buttons & (1 << 12)) report.buttons_mid |= SWITCH_MASK_HOME;
      if (buttons & (1 << 13)) report.buttons_mid |= SWITCH_MASK_CAPTURE;

      // D-pad
      report.hat = dpad_to_switch_hat(gamepad_data.dpad);

      // Analog sticks
      report.lx = gamepad_data.lx + 128;
      report.ly = gamepad_data.ly + 128;
      report.rx = gamepad_data.rx + 128;
      report.ry = gamepad_data.ry + 128;

      // For now, we only send one IMU report. The Switch can handle this.
      report.imu_reports[0].accel_x = gamepad_data.accel_x;
      report.imu_reports[0].accel_y = gamepad_data.accel_y;
      report.imu_reports[0].accel_z = gamepad_data.accel_z;
      report.imu_reports[0].gyro_x = gamepad_data.gyro_x;
      report.imu_reports[0].gyro_y = gamepad_data.gyro_y;
      report.imu_reports[0].gyro_z = gamepad_data.gyro_z;

      tud_hid_report(0, &report, sizeof(report));
    #endif
  }
}

void debug_task() {
    static uint32_t start_ms = 0;
    const uint32_t interval_ms = 100;
    if (board_millis() - start_ms < interval_ms) {
        return;
    }
    start_ms += interval_ms;

    char buf[256];
    sprintf(buf, "RX: btns=%04x, s=%d,%d,%d,%d, t=%d,%d, d=%02x, a=%d,%d,%d, g=%d,%d,%d\r\n",
            gamepad_data.buttons, gamepad_data.lx, gamepad_data.ly,
            gamepad_data.rx, gamepad_data.ry, gamepad_data.l2,
            gamepad_data.r2, gamepad_data.dpad,
            gamepad_data.accel_x, gamepad_data.accel_y, gamepad_data.accel_z,
            gamepad_data.gyro_x, gamepad_data.gyro_y, gamepad_data.gyro_z
            );
    uart_puts(UART_ID, buf);
}

int main() {
    board_init();
    setup_uart();
    tusb_init();
    while (true) {
        tud_task();
        hid_task();
        process_uart();
        debug_task();
    }
    return 0;
}
