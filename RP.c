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

// Function to send a debug string over the main UART channel (uart1)
void debug_puts(const char *s) {
    uart_puts(UART_ID, s);
}
// Helper to print a buffer as a hex string
void print_buf_hex(const uint8_t* buf, size_t len) {
    char hex_str[3 * len + 5]; // +5 for "RAW: " and null terminator
    strcpy(hex_str, "RAW: ");
    for (size_t i = 0; i < len; ++i) {
        sprintf(hex_str + 5 + 3 * i, "%02X ", buf[i]);
    }
    hex_str[5 + 3 * len] = '\0';
    debug_puts(hex_str);
    debug_puts("\r\n");
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
                    // Always copy data if checksum is ok
                    memcpy(&gamepad_data, &pb[1], sizeof(gamepad_data));

                    // --- Throttle debug printing to every 500ms ---
                    static uint32_t last_uart_debug_ms = 0;
                    if (board_millis() - last_uart_debug_ms > 500) {
                        last_uart_debug_ms = board_millis();

                        debug_puts("--- UART Packet Snapshot ---\r\n");
                        print_buf_hex(pb, 23);
                        debug_puts("DEBUG: Checksum OK.\r\n");

                        // Print parsed data from the now-updated gamepad_data
                        char debug_str[100];
                        sprintf(debug_str, "DEBUG: Parsed sticks (LX,LY,RX,RY): %d,%d,%d,%d\r\n", gamepad_data.lx, gamepad_data.ly, gamepad_data.rx, gamepad_data.ry);
                        debug_puts(debug_str);
                        sprintf(debug_str, "DEBUG: Parsed triggers (L2,R2): %u,%u\r\n", gamepad_data.l2, gamepad_data.r2);
                        debug_puts(debug_str);
                        sprintf(debug_str, "DEBUG: Parsed dpad: %u\r\n", gamepad_data.dpad);
                        debug_puts(debug_str);
                    }
                } else {
                    // Only print checksum fails if they happen, as they should be rare
                    debug_puts("DEBUG: Checksum FAILED.\r\n");
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
  (void) buffer;
  (void) reqlen;

  char debug_str[100];
  sprintf(debug_str, "DEBUG GET_REPORT: id=%02x, type=%d\r\n", report_id, report_type);
  debug_puts(debug_str);

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint (Report ID = 0, Type = OUTPUT)
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
  (void) instance;

  char debug_str[100];
  sprintf(debug_str, "DEBUG SET_REPORT: id=%02x, type=%d, size=%u\r\n", report_id, report_type, bufsize);
  debug_puts(debug_str);
  debug_puts("DEBUG SET_REPORT: Host sent data:\r\n");
  print_buf_hex(buffer, bufsize);
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
// We are using a generic gamepad report, so we need a generic hat conversion.
uint8_t dpad_to_generic_hat(uint8_t dpad_mask) {
    static const uint8_t hat_map[16] = { 8, 0, 4, 8, 6, 7, 5, 8, 2, 1, 3, 8, 8, 8, 8, 8 };
    return hat_map[dpad_mask & 0x0F];
}
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

  // The SONY path is the only one we are debugging.
  #if CFG_TUD_HID_NINTENDO
    if ( tud_hid_ready() ) {
      hid_nintendo_report_t report = {0};
      // ... (existing nintendo logic from original file)
      tud_hid_report(0, &report, sizeof(report));
    }
  #elif CFG_TUD_HID_SONY
    // --- DEBUG START: Throttle hid_task debugging ---
    static uint32_t last_hid_debug_ms = 0;
    bool should_print_debug = false;
    if (board_millis() - last_hid_debug_ms > 500) {
        last_hid_debug_ms = board_millis();
        should_print_debug = true;
    }
    // --- DEBUG END ---

    if ( tud_hid_ready() ) {
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

      // D-Pad - fix rotational bug and out-of-range value
      uint8_t corrected_dpad = (gamepad_data.dpad + 1) % 9;
      if (corrected_dpad == 8) { // 8 is standard neutral, but descriptor max is 7
        report.dpad = 15; // Use value from passinglink reference project
      } else {
        report.dpad = corrected_dpad;
      }

      // Buttons
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

      // Report counter
      static uint8_t ds4_report_counter = 0;
      report.report_counter = ds4_report_counter++;

      // --- DEBUG: Check endpoint status and memory integrity before sending ---
      bool ep_in_busy = tud_hid_n_ep_busy(0, TUD_DIR_IN);
      bool success = false;

      if (should_print_debug) {
          char debug_str[100];
          sprintf(debug_str, "DEBUG HID: tud_hid_ready()=%d, ep_busy=%d\r\n", tud_hid_ready(), ep_in_busy);
          debug_puts(debug_str);
          sprintf(debug_str, "DEBUG HID: Pre-send dpad value = %u\r\n", report.dpad);
          debug_puts(debug_str);
      }

      if (!ep_in_busy) {
        success = tud_hid_report(1, &report, sizeof(report));
      }

      if (should_print_debug) {
          char debug_str[50];
          sprintf(debug_str, "DEBUG HID: tud_hid_report() success = %d\r\n", success);
          debug_puts(debug_str);
      }
      // --- DEBUG END ---
    }
  #else // GENERIC
    if ( tud_hid_ready() ) {
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
    }
  #endif
}

void debug_task() {
  // The new diagnostics are in the tud_hid_*_report_cb callbacks.
  // This task can be left empty for now to keep the output clean.
}

int main() {
    board_init();
    setup_uart();
    tusb_init();

    // --- DEBUG START ---
    // Use a buffer to format the string for the debug output
    char debug_str[50];
    sprintf(debug_str, "DEBUG: sizeof(hid_ds4_report_t) = %u\r\n", (unsigned int)sizeof(hid_ds4_report_t));
    debug_puts(debug_str);
    // --- DEBUG END ---

    while (true) {
        tud_task();
        hid_task();
        process_uart();
        debug_task();

        // --- DEBUG START ---
        static uint32_t last_print_ms = 0;
        if (board_millis() - last_print_ms > 2000) {
            last_print_ms = board_millis();
            char debug_str[50];
            sprintf(debug_str, "DEBUG: Main loop alive. Mounted = %d\r\n", tud_mounted());
            debug_puts(debug_str);
        }
        // --- DEBUG END ---
    }
    return 0;
}
