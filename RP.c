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
#include "ps4_auth.h"
#include "crc32.h"
#elif CFG_TUD_HID_NINTENDO
#include "switch_report.h"
#endif

// Struct to hold the received v2 controller data from UART
// Includes padding bytes to solve potential UART timing/framing issues
typedef struct __attribute__((packed)) {
    uint8_t  dummy_start;
    uint16_t buttons;
    int8_t   lx, ly, rx, ry;
    uint8_t  l2, r2;
    uint8_t  dpad;
    uint8_t  dummy_end;
} gamepad_data_v2_t;

static gamepad_data_v2_t gamepad_data;

#if CFG_TUD_HID_SONY
// Global structs for DS4 report and authentication data
static PS4Report ps4_report;
static PS4AuthData ps4_auth_data;
static uint8_t last_report_counter = 0;
#else
static uint8_t report_counter = 0;
#endif


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
    // Expecting a 13-byte packet: 1 header + 11 payload + 1 checksum
    static uint8_t pb[13];
    static uint8_t idx = 0;
    while (uart_is_readable(UART_ID)) {
        uint8_t ch = uart_getc(UART_ID);
        if (idx == 0) {
            if (ch == 0xA6) {
                pb[idx++] = ch;
            }
        } else {
            pb[idx++] = ch;
            if (idx >= 13) {
                uint8_t cs = 0;
                // Checksum is now over the header and the 11-byte payload
                for (int i = 0; i < 12; i++) {
                    cs ^= pb[i];
                }
                if (cs == pb[12]) {
                    // Copy the 11-byte payload into the padded struct
                    memcpy(&gamepad_data, &pb[1], sizeof(gamepad_data));
                }
                idx = 0;
            }
        }
    }
}



#if CFG_TUD_HID_SONY
// Pre-defined responses for authentication feature reports
static const uint8_t output_0x03[] = {
    0x21, 0x27, 0x04, 0xcf, 0x00, 0x2c, 0x56,
    0x08, 0x00, 0x3d, 0x00, 0xe8, 0x03, 0x04, 0x00,
    0xff, 0x7f, 0x0d, 0x0d, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t output_0xa3[] = {
    0x4a, 0x75, 0x6e, 0x20, 0x20, 0x39, 0x20, 0x32,
    0x30, 0x31, 0x37, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x31, 0x32, 0x3a, 0x33, 0x36, 0x3a, 0x34, 0x31,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x08, 0xb4, 0x01, 0x00, 0x00, 0x00,
    0x07, 0xa0, 0x10, 0x20, 0x00, 0xa0, 0x02, 0x00
};

static uint8_t cur_nonce_chunk = 0;
#endif

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
  (void) instance;

#if CFG_TUD_HID_SONY
  // All other reports are feature reports for authentication
  if (report_type != HID_REPORT_TYPE_FEATURE) {
      return 0;
  }

  uint8_t data[64] = {0};
  uint32_t crc32;

  switch (report_id) {
    case 0x03: // PS4_DEFINITION
      memcpy(buffer, output_0x03, sizeof(output_0x03));
      return sizeof(output_0x03);

    case 0xA3: // PS4_GET_VERSION_DATE
      memcpy(buffer, output_0xa3, sizeof(output_0xa3));
      return sizeof(output_0xa3);

    case 0xF1: // PS4_GET_SIGNATURE_NONCE
      data[0] = 0xF1;
      data[1] = ps4_auth_data.nonce_id;
      data[2] = cur_nonce_chunk;
      data[3] = 0;

      memcpy(&data[4], &ps4_auth_data.ps4_auth_buffer[cur_nonce_chunk * 56], 56);
      crc32 = CRC32_calculate(data, 60);
      memcpy(&data[60], &crc32, sizeof(uint32_t));

      memcpy(buffer, &data[1], 63);
      cur_nonce_chunk++;
      if (cur_nonce_chunk == 19) {
        ps4_auth_data.passthrough_state = auth_idle_state;
        cur_nonce_chunk = 0;
      }
      return 63;

    case 0xF2: // PS4_GET_SIGNING_STATE
      data[0] = 0xF2;
      data[1] = ps4_auth_data.nonce_id;
      data[2] = (ps4_auth_data.passthrough_state == send_auth_dongle_to_console) ? 0 : 16;
      memset(&data[3], 0, 9);
      crc32 = CRC32_calculate(data, 12);
      memcpy(&data[12], &crc32, sizeof(uint32_t));
      memcpy(buffer, &data[1], 15);
      return 15;

    case 0xF3: // PS4_RESET_AUTH
      ps4_auth_reset(&ps4_auth_data);
      return 0; // Stall

    default:
      break;
  }

#else
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;
#endif

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint (Report ID = 0, Type = OUTPUT)
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
  (void) instance;

#if CFG_TUD_HID_SONY
  // We only handle FEATURE reports
  if (report_type != HID_REPORT_TYPE_FEATURE) {
    return;
  }

  // The host is sending us a nonce to sign
  if (report_id == 0xF0) { // PS4_SET_AUTH_PAYLOAD
    // Reconstruct the buffer for CRC check
    uint8_t crc_buffer[64];
    crc_buffer[0] = report_id;
    memcpy(&crc_buffer[1], buffer, bufsize);
    uint32_t received_crc = *(uint32_t*)(&crc_buffer[bufsize - 3]);

    if (CRC32_calculate(crc_buffer, bufsize - 3) != received_crc) {
        // CRC check failed, ignore the packet
        return;
    }

    // The nonce is received in chunks.
    // buffer[0] is nonce_id, buffer[1] is nonce_page
    uint8_t nonce_id = buffer[0];
    uint8_t nonce_page = buffer[1];

    // On the first page, we reset our state
    if (nonce_page == 0) {
      ps4_auth_data.nonce_id = nonce_id;
    } else if (nonce_id != ps4_auth_data.nonce_id) {
      // If the nonce ID changes unexpectedly, reset auth state
      ps4_auth_reset(&ps4_auth_data);
      return;
    }

    // Copy the nonce data into our buffer
    if (nonce_page < 4) { // Pages 0-3 are 56 bytes
        memcpy(&ps4_auth_data.ps4_auth_buffer[nonce_page * 56], &buffer[3], 56);
    } else if (nonce_page == 4) { // Page 4 is 32 bytes
        memcpy(&ps4_auth_data.ps4_auth_buffer[nonce_page * 56], &buffer[3], 32);
        // This is the last chunk, so we trigger the signing process
        ps4_auth_data.passthrough_state = send_auth_console_to_dongle;
    }
  }
#else
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) bufsize;
#endif
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
      hid_ds4_report_t report = {0};
      // Process any pending authentication tasks
      ps4_auth_process(&ps4_auth_data);

      // Map UART data to the DS4 report structure
      ps4_report.report_id = 1;
      ps4_report.left_stick_x = gamepad_data.lx + 128;
      ps4_report.left_stick_y = gamepad_data.ly + 128;
      ps4_report.right_stick_x = gamepad_data.rx + 128;
      ps4_report.right_stick_y = gamepad_data.ry + 128;
      ps4_report.l2_trigger = gamepad_data.l2;
      ps4_report.r2_trigger = gamepad_data.r2;

      // DPAD
      ps4_report.dpad = dpad_to_ds4_hat(gamepad_data.dpad);

      // Buttons - we reset them all and then set the ones that are pressed
      ps4_report.square = 0;
      ps4_report.cross = 0;
      ps4_report.circle = 0;
      ps4_report.triangle = 0;
      ps4_report.l1 = 0;
      ps4_report.r1 = 0;
      ps4_report.l2 = 0;
      ps4_report.r2 = 0;
      ps4_report.share = 0;
      ps4_report.options = 0;
      ps4_report.l3 = 0;
      ps4_report.r3 = 0;
      ps4_report.ps = 0;
      ps4_report.tpad = 0;

      if (gamepad_data.buttons & (1 << 0))  ps4_report.square = 1;
      if (gamepad_data.buttons & (1 << 1))  ps4_report.cross = 1;
      if (gamepad_data.buttons & (1 << 2))  ps4_report.circle = 1;
      if (gamepad_data.buttons & (1 << 3))  ps4_report.triangle = 1;
      if (gamepad_data.buttons & (1 << 4))  ps4_report.l1 = 1;
      if (gamepad_data.buttons & (1 << 5))  ps4_report.r1 = 1;
      if (gamepad_data.l2 > 30)             ps4_report.l2 = 1;
      if (gamepad_data.r2 > 30)             ps4_report.r2 = 1;
      if (gamepad_data.buttons & (1 << 8))  ps4_report.share = 1;
      if (gamepad_data.buttons & (1 << 9))  ps4_report.options = 1;
      if (gamepad_data.buttons & (1 << 10)) ps4_report.l3 = 1;
      if (gamepad_data.buttons & (1 << 11)) ps4_report.r3 = 1;
      if (gamepad_data.buttons & (1 << 12)) ps4_report.ps = 1;
      if (gamepad_data.buttons & (1 << 13)) ps4_report.tpad = 1;

      // Keep-alive and report counter
      ps4_report.report_counter = last_report_counter++;

      // Gyro and accelerometer data - set to zero as not provided by UART
      ps4_report.accel_x = 0;
      ps4_report.accel_y = 0;
      ps4_report.accel_z = 0;
      ps4_report.gyro_x = 0;
      ps4_report.gyro_y = 0;
      ps4_report.gyro_z = 0;

      // Touchpad data - set to not touched
      ps4_report.touchpad.p1.unpressed = 1;
      ps4_report.touchpad.p2.unpressed = 1;

      tud_hid_report(0, &ps4_report, sizeof(ps4_report));
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

    char buf[128];
    sprintf(buf, "RX: btns=%04x, lx=%d, ly=%d, rx=%d, ry=%d, l2=%d, r2=%d, dpad=%02x\r\n",
            gamepad_data.buttons, gamepad_data.lx, gamepad_data.ly,
            gamepad_data.rx, gamepad_data.ry, gamepad_data.l2,
            gamepad_data.r2, gamepad_data.dpad);
    uart_puts(UART_ID, buf);
}

int main() {
    board_init();
    setup_uart();
    tusb_init();

#if CFG_TUD_HID_SONY
    ps4_auth_initialize(&ps4_auth_data);
#endif

    while (true) {
        tud_task();
        hid_task();
        process_uart();
        debug_task();
    }
    return 0;
}
