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

// --- Non-intrusive debug counters ---
static uint32_t checksum_fail_count = 0;

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
// --- CRC8 Implementation from hdtodd/CRC8-Library ---
static uint8_t crc8_table[256];
static void build_crc8_table(uint8_t poly) {
    for (uint16_t i = 0; i < 256; i++) {
        uint8_t c = i;
        for (uint8_t j = 0; j < 8; j++) {
            c = ((c & 0x80) == 0) ? (c << 1) : ((c << 1) ^ poly);
        }
        crc8_table[i] = c;
    }
}

static uint8_t crc8(uint8_t *data, int len, uint8_t init) {
    uint8_t crc = init;
    while (len-- > 0) {
        crc = crc8_table[(crc ^ *data++)];
    }
    return crc;
}
// ----------------------------------------------------


// Helper to print a buffer as a hex string
void print_buf_hex(const uint8_t* buf, size_t len) {
    // Allocate 1 extra byte for the null terminator to fix overflow warning.
    char hex_str[3 * len + 6];
    strcpy(hex_str, "RAW: ");
    for (size_t i = 0; i < len; ++i) {
        sprintf(hex_str + 5 + 3 * i, "%02X ", buf[i]);
    }
    hex_str[5 + 3 * len] = '\0';
    debug_puts(hex_str);
    debug_puts("\r\n");
}

void process_uart() {
    static uint8_t pb[23];
    static uint8_t idx = 0;

    while (uart_is_readable(UART_ID)) {
        uint8_t ch = uart_getc(UART_ID);

        // State 0: Searching for header
        if (idx == 0) {
            if (ch == 0xA6) {
                pb[0] = ch;
                idx = 1;
            }
        }
        // State 1: Receiving payload
        else {
            pb[idx] = ch;
            idx++;

            if (idx >= 23) {
                // Verify packet with CRC8 instead of XOR
                uint8_t calculated_crc = crc8(pb, 22, 0xFF);
                uint8_t received_crc = pb[22];

                if (calculated_crc == received_crc) {
                    memcpy(&gamepad_data, &pb[1], sizeof(gamepad_data));
                } else {
                    checksum_fail_count++;
                }

                // Reset to state 0 to search for the next header
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

      // D-Pad: The GUI sends a bitmask (U=1, D=2, L=4, R=8).
      // The firmware needs to convert this to the DS4 HAT values (0-7 for directions, 15 for neutral).
      // A lookup table is the cleanest way to do this conversion.
      static const uint8_t dpad_map[16] = {
        15, // 0000: Neutral
        0,  // 0001: Up
        4,  // 0010: Down
        15, // 0011: Up|Down -> Invalid, treat as neutral
        6,  // 0100: Left
        7,  // 0101: Up|Left
        5,  // 0110: Down|Left
        15, // 0111: Up|Down|Left -> Invalid
        2,  // 1000: Right
        1,  // 1001: Up|Right
        3,  // 1010: Down|Right
        15, // 1011: Up|Down|Right -> Invalid
        15, // 1100: Left|Right -> Invalid
        15, // 1101: Up|Left|Right -> Invalid
        15, // 1110: Down|Left|Right -> Invalid
        15  // 1111: All -> Invalid
      };
      uint8_t dpad_mask = gamepad_data.dpad & 0x0F;
      report.dpad = dpad_map[dpad_mask];

      // Buttons
      // User reported that Circle and Cross were swapped.
      // Assuming bit 0 is Cross and bit 1 is Circle from the input.
      report.cross = (gamepad_data.buttons >> 0) & 1;
      report.circle = (gamepad_data.buttons >> 1) & 1;
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

      // Gyro and Accelerometer
      report.accel_x = gamepad_data.accel_x;
      report.accel_y = gamepad_data.accel_y;
      report.accel_z = gamepad_data.accel_z;
      report.gyro_x = gamepad_data.gyro_x;
      report.gyro_y = gamepad_data.gyro_y;
      report.gyro_z = gamepad_data.gyro_z;

      // Report counter
      static uint8_t ds4_report_counter = 0;
      report.report_counter = ds4_report_counter++;

      // --- DEBUG: Check endpoint status and memory integrity before sending ---
      // We discovered previously that sending the report with ID 0 (and the ID in the buffer)
      // is the correct method for this setup.
      bool success = tud_hid_report(0, &report, sizeof(report));

      if (should_print_debug) {
          char debug_str[100];
          static uint32_t last_fail_count = 0;
          if (checksum_fail_count > last_fail_count) {
              sprintf(debug_str, "DEBUG: UART checksum failures detected. Total fails: %lu\r\n", checksum_fail_count);
              debug_puts(debug_str);
              last_fail_count = checksum_fail_count;
          }

          sprintf(debug_str, "DEBUG HID: tud_hid_ready()=%d\r\n", tud_hid_ready());
          debug_puts(debug_str);
          sprintf(debug_str, "DEBUG HID: Pre-send dpad value = %u\r\n", report.dpad);
          debug_puts(debug_str);
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
    build_crc8_table(0x07); // Initialize CRC8 table with standard polynomial
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
