#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/uart.h"
#include "tusb.h"
#include "bsp/board.h"
#include "class/hid/hid_device.h"

#define USE_PRO_CONTROLLER 1

#if CFG_TUD_HID_SONY
#include "ds4_report.h"
#elif CFG_TUD_HID_NINTENDO
#if USE_PRO_CONTROLLER
#include "pro_switch_report.h"
#else
#include "switch_report.h"
#endif
#endif

// Struct to hold the received v3 controller data from UART
typedef struct __attribute__((packed)) {
    uint16_t buttons;
    int8_t   lx, ly, rx, ry;
    uint8_t  l2, r2;
    uint8_t  dpad;
    int16_t  ax, ay, az;
    int16_t  gx, gy, gz;
} gamepad_data_v3_t;

static gamepad_data_v3_t gamepad_data;
static uint8_t report_counter = 0;

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
            if (ch == 0xA7) {
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
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint (Report ID = 0, Type = OUTPUT)
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
  // tud_hid_report(0, buffer, bufsize);
  // For Switch Pro Controller, we need to handle output reports
#if CFG_TUD_HID_NINTENDO && USE_PRO_CONTROLLER
    // echo back anything we received from host
    // A real NINTENDO Switch send report buffer and wait for our reply via REPORT ID
    //
    // Reference:
    // https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/bluetooth_hid_subcommands_notes.md
    printf("Set report received, id %d, type %d, size %d\n", report_id, report_type, bufsize);
    for(int i=0; i<bufsize; ++i) printf("%02x ", buffer[i]);
    printf("\n");
#else
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
#endif
}

#if CFG_TUD_HID_NINTENDO
uint8_t dpad_to_switch_hat(uint8_t dpad_mask) {
#if USE_PRO_CONTROLLER
    static const uint8_t hat_map[16] = {
        PRO_SWITCH_HAT_NOTHING, PRO_SWITCH_HAT_UP, PRO_SWITCH_HAT_DOWN, PRO_SWITCH_HAT_NOTHING,
        PRO_SWITCH_HAT_LEFT, PRO_SWITCH_HAT_UPLEFT, PRO_SWITCH_HAT_DOWNLEFT, PRO_SWITCH_HAT_NOTHING,
        PRO_SWITCH_HAT_RIGHT, PRO_SWITCH_HAT_UPRIGHT, PRO_SWITCH_HAT_DOWNRIGHT, PRO_SWITCH_HAT_NOTHING,
        PRO_SWITCH_HAT_NOTHING, PRO_SWITCH_HAT_NOTHING, PRO_SWITCH_HAT_NOTHING, PRO_SWITCH_HAT_NOTHING
    };
#else
    static const uint8_t hat_map[16] = {
        SWITCH_HAT_NOTHING, SWITCH_HAT_UP, SWITCH_HAT_DOWN, SWITCH_HAT_NOTHING,
        SWITCH_HAT_LEFT, SWITCH_HAT_UPLEFT, SWITCH_HAT_DOWNLEFT, SWITCH_HAT_NOTHING,
        SWITCH_HAT_RIGHT, SWITCH_HAT_UPRIGHT, SWITCH_HAT_DOWNRIGHT, SWITCH_HAT_NOTHING,
        SWITCH_HAT_NOTHING, SWITCH_HAT_NOTHING, SWITCH_HAT_NOTHING, SWITCH_HAT_NOTHING
    };
#endif
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
#if USE_PRO_CONTROLLER
      hid_pro_switch_report_t report = {0};
      report.report_id = PRO_SWITCH_REPORT_ID;
      report.timer = report_counter++;

      // Button mapping
      if (gamepad_data.buttons & (1 << 1)) report.buttons[0] |= PRO_SWITCH_MASK_A;
      if (gamepad_data.buttons & (1 << 0)) report.buttons[0] |= PRO_SWITCH_MASK_B;
      if (gamepad_data.buttons & (1 << 3)) report.buttons[0] |= PRO_SWITCH_MASK_X;
      if (gamepad_data.buttons & (1 << 2)) report.buttons[0] |= PRO_SWITCH_MASK_Y;
      if (gamepad_data.buttons & (1 << 4)) report.buttons[0] |= PRO_SWITCH_MASK_L;
      if (gamepad_data.buttons & (1 << 5)) report.buttons[0] |= PRO_SWITCH_MASK_R;
      if (gamepad_data.l2 > 30) report.buttons[0] |= PRO_SWITCH_MASK_ZL;
      if (gamepad_data.r2 > 30) report.buttons[0] |= PRO_SWITCH_MASK_ZR;
      if (gamepad_data.buttons & (1 << 8)) report.buttons[1] |= (PRO_SWITCH_MASK_MINUS >> 8);
      if (gamepad_data.buttons & (1 << 9)) report.buttons[1] |= (PRO_SWITCH_MASK_PLUS >> 8);
      if (gamepad_data.buttons & (1 << 10)) report.buttons[1] |= (PRO_SWITCH_MASK_L3 >> 8);
      if (gamepad_data.buttons & (1 << 11)) report.buttons[1] |= (PRO_SWITCH_MASK_R3 >> 8);
      if (gamepad_data.buttons & (1 << 12)) report.buttons[1] |= (PRO_SWITCH_MASK_HOME >> 8);
      if (gamepad_data.buttons & (1 << 13)) report.buttons[1] |= (PRO_SWITCH_MASK_CAPTURE >> 8);

      // D-pad
      report.hat = dpad_to_switch_hat(gamepad_data.dpad);

      // Analog sticks
      report.lx = gamepad_data.lx + 128;
      report.ly = gamepad_data.ly + 128;
      report.rx = gamepad_data.rx + 128;
      report.ry = gamepad_data.ry + 128;

      // IMU data
      memcpy(&report.imu_data[0], &gamepad_data.ax, 2);
      memcpy(&report.imu_data[2], &gamepad_data.ay, 2);
      memcpy(&report.imu_data[4], &gamepad_data.az, 2);
      memcpy(&report.imu_data[6], &gamepad_data.gx, 2);
      memcpy(&report.imu_data[8], &gamepad_data.gy, 2);
      memcpy(&report.imu_data[10], &gamepad_data.gz, 2);

      tud_hid_report(0, &report, sizeof(report));
#else
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
#endif
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

void debug_task() {
    static uint32_t start_ms = 0;
    const uint32_t interval_ms = 100;
    if (board_millis() - start_ms < interval_ms) {
        return;
    }
    start_ms += interval_ms;

    char buf[256];
    sprintf(buf, "RX: btns=%04x, lx=%d, ly=%d, rx=%d, ry=%d, l2=%d, r2=%d, dpad=%02x, ax=%d, ay=%d, az=%d, gx=%d, gy=%d, gz=%d\r\n",
            gamepad_data.buttons, gamepad_data.lx, gamepad_data.ly,
            gamepad_data.rx, gamepad_data.ry, gamepad_data.l2,
            gamepad_data.r2, gamepad_data.dpad,
            gamepad_data.ax, gamepad_data.ay, gamepad_data.az,
            gamepad_data.gx, gamepad_data.gy, gamepad_data.gz);
    printf("%s", buf);
}

int main() {
    stdio_init_all();
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
