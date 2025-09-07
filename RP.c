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
// Includes IMU data.
typedef struct __attribute__((packed)) {
    uint8_t  dummy_start;
    uint16_t buttons;
    int8_t   lx, ly, rx, ry;
    uint8_t  l2, r2;
    uint8_t  dpad;
    int16_t  ax, ay, az;
    int16_t  gx, gy, gz;
    uint8_t  dummy_end;
} gamepad_data_v3_t;

static gamepad_data_v3_t gamepad_data;
static uint8_t report_counter = 0;

// Global state for Pro Controller emulation
static struct {
    bool imu_enabled;
    uint8_t report_mode;
} pro_controller_state = {
    .imu_enabled = false,
    .report_mode = 0x3f, // Default to simple HID mode
};

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
    // Expecting a 25-byte packet: 1 header + 23 payload + 1 checksum
    static uint8_t pb[25];
    static uint8_t idx = 0;
    while (uart_is_readable(UART_ID)) {
        uint8_t ch = uart_getc(UART_ID);
        if (idx == 0) {
            if (ch == 0xA6) {
                pb[idx++] = ch;
            }
        } else {
            pb[idx++] = ch;
            if (idx >= 25) {
                uint8_t cs = 0;
                // Checksum is now over the header and the 23-byte payload
                for (int i = 0; i < 24; i++) {
                    cs ^= pb[i];
                }
                if (cs == pb[24]) {
                    // Copy the 23-byte payload into the padded struct
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
  if (report_type != HID_REPORT_TYPE_OUTPUT) {
    return;
  }

  // The first byte of the output report is the report ID.
  // The second byte is a packet counter.
  // The third byte is the subcommand.
  uint8_t subcommand = buffer[1];

  // Prepare a response buffer. The response for a subcommand is an input report with ID 0x21.
  uint8_t response[64] = {0};
  response[0] = 0x21; // Report ID for subcommand responses
  response[1] = buffer[0]; // Echo the packet counter

  // Acknowledge the subcommand
  response[2] = 0x80; // General ACK
  response[3] = subcommand;

  char debug_buf[128];

  switch (subcommand) {
    case SUBCOMMAND_SET_INPUT_REPORT_MODE:
      pro_controller_state.report_mode = buffer[2];
      sprintf(debug_buf, "Subcommand: Set Input Report Mode to 0x%02x\r\n", pro_controller_state.report_mode);
      uart_puts(UART_ID, debug_buf);
      break;

    case SUBCOMMAND_ENABLE_IMU:
      pro_controller_state.imu_enabled = (buffer[2] == 0x01);
      sprintf(debug_buf, "Subcommand: Set IMU Enabled to %d\r\n", pro_controller_state.imu_enabled);
      uart_puts(UART_ID, debug_buf);
      break;

    case SUBCOMMAND_REQUEST_DEVICE_INFO:
        // Respond with device info: Pro Controller, MAC address, etc.
        // This is a more complex response that we can fill in later if needed.
        // For now, just ACK.
        sprintf(debug_buf, "Subcommand: Request Device Info\r\n");
        uart_puts(UART_ID, debug_buf);
        break;

    default:
      sprintf(debug_buf, "Subcommand: unhandled 0x%02x\r\n", subcommand);
      uart_puts(UART_ID, debug_buf);
      break;
  }

  // Send the ACK response
  tud_hid_report(0x21, response, sizeof(response));
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
      if (pro_controller_state.report_mode == 0x30) {
        pro_controller_report_t report = {0};
        report.report_id = 0x30;
        report.timer = report_counter++;

        // Buttons
        // Byte 0: Y, B, A, X, L, R, ZL, ZR
        if (gamepad_data.buttons & (1 << 2)) report.buttons[0] |= 0x01; // Y
        if (gamepad_data.buttons & (1 << 0)) report.buttons[0] |= 0x02; // B
        if (gamepad_data.buttons & (1 << 1)) report.buttons[0] |= 0x04; // A
        if (gamepad_data.buttons & (1 << 3)) report.buttons[0] |= 0x08; // X
        if (gamepad_data.buttons & (1 << 4)) report.buttons[0] |= 0x10; // L
        if (gamepad_data.buttons & (1 << 5)) report.buttons[0] |= 0x20; // R
        if (gamepad_data.l2 > 30)            report.buttons[0] |= 0x40; // ZL
        if (gamepad_data.r2 > 30)            report.buttons[0] |= 0x80; // ZR

        // Byte 1: -, +, L3, R3, Home, Capture
        if (gamepad_data.buttons & (1 << 8)) report.buttons[1] |= 0x01; // -
        if (gamepad_data.buttons & (1 << 9)) report.buttons[1] |= 0x02; // +
        if (gamepad_data.buttons & (1 << 10)) report.buttons[1] |= 0x04; // L3
        if (gamepad_data.buttons & (1 << 11)) report.buttons[1] |= 0x08; // R3
        if (gamepad_data.buttons & (1 << 12)) report.buttons[1] |= 0x10; // Home
        if (gamepad_data.buttons & (1 << 13)) report.buttons[1] |= 0x20; // Capture

        // Byte 2: HAT
        report.buttons[2] = dpad_to_switch_hat(gamepad_data.dpad);

        // Analog Sticks (12-bit), Y axes are inverted
        uint16_t lx = (uint16_t)((int16_t)gamepad_data.lx + 128) * 16;
        uint16_t ly = (uint16_t)((int16_t)-gamepad_data.ly + 128) * 16;
        uint16_t rx = (uint16_t)((int16_t)gamepad_data.rx + 128) * 16;
        uint16_t ry = (uint16_t)((int16_t)-gamepad_data.ry + 128) * 16;

        report.sticks[0] = lx & 0xFF;
        report.sticks[1] = ((lx >> 8) & 0x0F) | ((ly & 0x0F) << 4);
        report.sticks[2] = (ly >> 4) & 0xFF;
        report.sticks[3] = rx & 0xFF;
        report.sticks[4] = ((rx >> 8) & 0x0F) | ((ry & 0x0F) << 4);
        report.sticks[5] = (ry >> 4) & 0xFF;

        // IMU data
        if (pro_controller_state.imu_enabled) {
            int16_t* imu_samples = (int16_t*)report.imu_data;
            // Sample 1
            imu_samples[0] = gamepad_data.ax;
            imu_samples[1] = gamepad_data.ay;
            imu_samples[2] = gamepad_data.az;
            imu_samples[3] = gamepad_data.gx;
            imu_samples[4] = gamepad_data.gy;
            imu_samples[5] = gamepad_data.gz;
            // Sample 2
            imu_samples[6] = gamepad_data.ax;
            imu_samples[7] = gamepad_data.ay;
            imu_samples[8] = gamepad_data.az;
            imu_samples[9] = gamepad_data.gx;
            imu_samples[10] = gamepad_data.gy;
            imu_samples[11] = gamepad_data.gz;
            // Sample 3
            imu_samples[12] = gamepad_data.ax;
            imu_samples[13] = gamepad_data.ay;
            imu_samples[14] = gamepad_data.az;
            imu_samples[15] = gamepad_data.gx;
            imu_samples[16] = gamepad_data.gy;
            imu_samples[17] = gamepad_data.gz;
        }

        tud_hid_report(report.report_id, &report, sizeof(report));
      }
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
    const uint32_t interval_ms = 1000; // Slower debug output
    if (board_millis() - start_ms < interval_ms) {
        return;
    }
    start_ms += interval_ms;

    char buf[256];
    sprintf(buf, "State: IMUEn=%d, Mode=0x%02x | RX: btns=%04x, lx=%d, ly=%d, rx=%d, ry=%d, dpad=%02x, ax=%d, ay=%d, az=%d, gx=%d, gy=%d, gz=%d\r\n",
            pro_controller_state.imu_enabled, pro_controller_state.report_mode,
            gamepad_data.buttons, gamepad_data.lx, gamepad_data.ly,
            gamepad_data.rx, gamepad_data.ry, gamepad_data.dpad,
            gamepad_data.ax, gamepad_data.ay, gamepad_data.az,
            gamepad_data.gx, gamepad_data.gy, gamepad_data.gz
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
