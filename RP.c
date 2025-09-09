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
    int8_t   lx, ly;
    uint8_t  l2, r2;
    int8_t   rx, ry;
    uint8_t  dpad;
    int16_t  accel_x, accel_y, accel_z;
    int16_t  gyro_x, gyro_y, gyro_z;
} gamepad_data_v2_t;

static gamepad_data_v2_t gamepad_data;
static uint8_t report_counter = 0;
static hid_ds4_report_t last_sent_report; // For debugging raw report bytes

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
    // Expecting a 23-byte packet: 1 header + 21 payload + 1 checksum
    static uint8_t pb[23];
    static uint8_t idx = 0;
    while (uart_is_readable(UART_ID)) {
        uint8_t ch = uart_getc(UART_ID);
        if (idx == 0) {
            if (ch == 0xA6) {
                pb[idx++] = ch;
            }
        } else {
            pb[idx++] = ch;
            if (idx >= 23) {
                uint8_t cs = 0;
                // Checksum is now over the header and the 21-byte payload
                for (int i = 0; i < 22; i++) {
                    cs ^= pb[i];
                }
                if (cs == pb[22]) {
                    // Copy the 21-byte payload into the struct
                    memcpy(&gamepad_data, &pb[1], sizeof(gamepad_data));
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
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  printf("GET_REPORT: id=%02x\r\n", report_id);

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint (Report ID = 0, Type = OUTPUT)
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
  (void) instance;
  (void) report_type;
  (void) buffer;
  (void) bufsize;

  printf("SET_REPORT: id=%02x\r\n", report_id);

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
// dpad_to_generic_hat removed, as it's no longer used.
#else // For Generic
uint8_t dpad_to_generic_hat(uint8_t dpad_mask) {
    static const uint8_t hat_map[16] = { 8, 0, 4, 8, 6, 7, 5, 8, 2, 1, 3, 8, 8, 8, 8, 8 };
    return hat_map[dpad_mask & 0x0F];
}
#endif

void hid_task(void) {
  const uint32_t interval_ms = 16;
  static uint32_t start_ms = 0;
  if ( board_millis() - start_ms < interval_ms) return;
  start_ms += interval_ms;

  if ( tud_suspended() ) tud_remote_wakeup();

  if ( tud_hid_ready() ) {
    #if CFG_TUD_HID_NINTENDO
      hid_nintendo_report_t report = {0};

      // Button mapping uses direct 1-to-1 logic.
      // The padding fix should resolve any data corruption issues.
      if (gamepad_data.buttons & (1 << 1)) report.buttons |= SWITCH_MASK_A;
      if (gamepad_data.buttons & (1 << 0)) report.buttons |= SWITCH_MASK_B;
      if (gamepad_data.buttons & (1 << 3)) report.buttons |= SWITCH_MASK_X;
      if (gamepad_data.buttons & (1 << 2)) report.buttons |= SWITCH_MASK_Y;
      if (gamepad_data.buttons & (1 << 4)) report.buttons |= SWITCH_MASK_L;
      if (gamepad_data.buttons & (1 << 5)) report.buttons |= SWITCH_MASK_R;
      if (gamepad_data.l2 > 30) report.buttons |= SWITCH_MASK_ZL;
      if (gamepad_data.r2 > 30) report.buttons |= SWITCH_MASK_ZR;
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
      // Build and send a full DS4 report
      hid_ds4_report_t report = {0};

      report.report_id = 0x01;

      // Analog sticks - convert from int8 to uint8
      report.left_stick_x = gamepad_data.lx + 128;
      report.left_stick_y = gamepad_data.ly + 128;
      report.right_stick_x = gamepad_data.rx + 128;
      report.right_stick_y = gamepad_data.ry + 128;

      // Analog triggers
      report.l2_trigger = gamepad_data.l2;
      report.r2_trigger = gamepad_data.r2;

      // D-Pad - fix rotational bug
      // True state = (received state + 1) % 9.
      report.dpad = (gamepad_data.dpad + 1) % 9;

      // Buttons - map from gamepad_data.buttons to the bitfield
      // User reported buttons work, so assuming a direct mapping is fine.
      // PS4 bit order: 0:Circle, 1:Cross, 2:Square, 3:Triangle
      report.circle = (gamepad_data.buttons >> 0) & 1;
      report.cross = (gamepad_data.buttons >> 1) & 1;
      report.square = (gamepad_data.buttons >> 2) & 1;
      report.triangle = (gamepad_data.buttons >> 3) & 1;
      report.l1 = (gamepad_data.buttons >> 4) & 1;
      report.r1 = (gamepad_data.buttons >> 5) & 1;
      report.l2 = (gamepad_data.buttons >> 6) & 1;
      report.r2 = (gamepad_data.buttons >> 7) & 1;
      report.share = (gamepad_data.buttons >> 8) & 1;
      report.options = (gamepad_data.buttons >> 9) & 1;
      report.l3 = (gamepad_data.buttons >> 10) & 1;
      report.r3 = (gamepad_data.buttons >> 11) & 1;
      report.ps = (gamepad_data.buttons >> 12) & 1;
      report.tpad_click = (gamepad_data.buttons >> 13) & 1;

      // Report counter - just increment
      static uint8_t ds4_report_counter = 0;
      report.report_counter = ds4_report_counter++;

      // Send the report. Report ID is 1.
      tud_hid_report(1, &report, sizeof(report));
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

void debug_task() {
  // The new diagnostics are in the tud_hid_*_report_cb callbacks.
  // This task can be left empty for now to keep the output clean.
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
