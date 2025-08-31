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

// Struct to hold the received v2 controller data from UART
typedef struct __attribute__((packed)) {
    uint16_t buttons;
    int8_t   lx, ly, rx, ry;
    uint8_t  l2, r2;
    uint8_t  dpad;
} gamepad_data_v2_t;

static gamepad_data_v2_t gamepad_data;
static uint8_t report_counter = 0;

#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5

void setup_uart() { uart_init(UART_ID, BAUD_RATE); gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART); gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART); }
void process_uart() {
    static uint8_t pb[11];
    static uint8_t idx = 0;
    while (uart_is_readable(UART_ID)) {
        uint8_t ch = uart_getc(UART_ID);
        if (idx == 0) {
            if (ch == 0xA6) {
                pb[idx++] = ch;
            }
        } else {
            pb[idx++] = ch;
            if (idx >= 11) {
                uint8_t cs = 0;
                for (int i = 0; i < 10; i++) {
                    cs ^= pb[i];
                }
                if (cs == pb[10]) {
                    memcpy(&gamepad_data, &pb[1], sizeof(gamepad_data));
                    // Add a debug print to send data back to the host
                    printf("RX: btns=%04x, lx=%d, ly=%d, rx=%d, ry=%d, l2=%d, r2=%d, dpad=%02x\n",
                           gamepad_data.buttons, gamepad_data.lx, gamepad_data.ly,
                           gamepad_data.rx, gamepad_data.ry, gamepad_data.l2,
                           gamepad_data.r2, gamepad_data.dpad);
                }
                idx = 0;
            }
        }
    }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint (Report ID = 0, Type = OUTPUT)
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) bufsize;

  // echo back anything we received from host
  // tud_hid_report(0, buffer, bufsize);
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
#elif CFG_TUD_HID_SONY
uint8_t dpad_to_ds4_hat(uint8_t dpad_mask) {
    static const uint8_t hat_map[16] = {
        DS4_HAT_NOTHING, DS4_HAT_UP, DS4_HAT_DOWN, DS4_HAT_NOTHING,
        DS4_HAT_LEFT, DS4_HAT_UPLEFT, DS4_HAT_DOWNLEFT, DS4_HAT_NOTHING,
        DS4_HAT_RIGHT, DS4_HAT_UPRIGHT, DS4_HAT_DOWNRIGHT, DS4_HAT_NOTHING,
        DS4_HAT_NOTHING, DS4_HAT_NOTHING, DS4_HAT_NOTHING, DS4_HAT_NOTHING
    };
    return hat_map[dpad_mask & 0x0F];
}
#else // For Generic
uint8_t dpad_to_generic_hat(uint8_t dpad_mask) {
    static const uint8_t hat_map[16] = { 8, 0, 4, 8, 6, 7, 5, 8, 2, 1, 3, 8, 8, 8, 8, 8 };
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

      // Button mapping
      if (gamepad_data.buttons & (1 << 1)) report.buttons |= SWITCH_MASK_A;
      if (gamepad_data.buttons & (1 << 0)) report.buttons |= SWITCH_MASK_B;
      if (gamepad_data.buttons & (1 << 3)) report.buttons |= SWITCH_MASK_X;
      if (gamepad_data.buttons & (1 << 2)) report.buttons |= SWITCH_MASK_Y;
      if (gamepad_data.buttons & (1 << 4)) report.buttons |= SWITCH_MASK_L;
      if (gamepad_data.buttons & (1 << 5)) report.buttons |= SWITCH_MASK_R;
      if (gamepad_data.buttons & (1 << 6)) report.buttons |= SWITCH_MASK_ZL;
      if (gamepad_data.buttons & (1 << 7)) report.buttons |= SWITCH_MASK_ZR;
      if (gamepad_data.buttons & (1 << 8)) report.buttons |= SWITCH_MASK_MINUS;
      if (gamepad_data.buttons & (1 << 9)) report.buttons |= SWITCH_MASK_PLUS;
      if (gamepad_data.buttons & (1 << 10)) report.buttons |= SWITCH_MASK_L3;
      if (gamepad_data.buttons & (1 << 11)) report.buttons |= SWITCH_MASK_R3;
      if (gamepad_data.buttons & (1 << 12)) report.buttons |= SWITCH_MASK_HOME;
      if (gamepad_data.buttons & (1 << 13)) report.buttons |= SWITCH_MASK_CAPTURE;

      // D-pad
      report.hat = dpad_to_switch_hat(gamepad_data.dpad);

      // Analog sticks
      report.lx = gamepad_data.lx + 128;
      report.ly = gamepad_data.ly + 128;
      report.rx = gamepad_data.rx + 128;
      report.ry = gamepad_data.ry + 128;

      tud_hid_report(0, &report, sizeof(report));
    #elif CFG_TUD_HID_SONY
      hid_ds4_report_t report = {0};
      report.report_id = 1;
      report.left_stick_x = gamepad_data.lx + 128;
      report.left_stick_y = gamepad_data.ly + 128;
      report.right_stick_x = gamepad_data.rx + 128;
      report.right_stick_y = gamepad_data.ry + 128;
      report.l2_trigger = gamepad_data.l2;
      report.r2_trigger = gamepad_data.r2;
      report.dpad = dpad_to_ds4_hat(gamepad_data.dpad);

      if (gamepad_data.buttons & (1 << 0))  report.square = 1;
      if (gamepad_data.buttons & (1 << 1))  report.cross = 1;
      if (gamepad_data.buttons & (1 << 2))  report.circle = 1;
      if (gamepad_data.buttons & (1 << 3))  report.triangle = 1;
      if (gamepad_data.buttons & (1 << 4))  report.l1 = 1;
      if (gamepad_data.buttons & (1 << 5))  report.r1 = 1;
      if (gamepad_data.buttons & (1 << 6))  report.l2 = 1;
      if (gamepad_data.buttons & (1 << 7))  report.r2 = 1;
      if (gamepad_data.buttons & (1 << 8))  report.share = 1;
      if (gamepad_data.buttons & (1 << 9))  report.options = 1;
      if (gamepad_data.buttons & (1 << 10)) report.l3 = 1;
      if (gamepad_data.buttons & (1 << 11)) report.r3 = 1;
      if (gamepad_data.buttons & (1 << 12)) report.ps = 1;
      if (gamepad_data.buttons & (1 << 13)) report.tpad = 1;

      report.report_counter = report_counter++;

      // Gyro and accelerometer data - set to zero as not provided by UART
      report.accel_x = 0;
      report.accel_y = 0;
      report.accel_z = 0;
      report.gyro_x = 0;
      report.gyro_y = 0;
      report.gyro_z = 0;

      // Touchpad data - set to not touched
      report.touchpad.p1.unpressed = 1;
      report.touchpad.p2.unpressed = 1;

      tud_hid_report(0, &report, sizeof(report));
    #else // GENERIC
      hid_gamepad_report_t report = {0};
      report.buttons = gamepad_data.buttons;
      report.hat = dpad_to_generic_hat(gamepad_data.dpad);
      report.x = gamepad_data.lx;
      report.y = gamepad_data.ly;
      report.rx = gamepad_data.rx;
      report.ry = gamepad_data.ry;
      report.z = gamepad_data.l2;
      report.rz = gamepad_data.r2;
      tud_hid_report(1, &report, sizeof(report));
    #endif
  }
}

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
