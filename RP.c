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
    //... (Nintendo path unchanged)
  #elif CFG_TUD_HID_SONY
    if ( tud_hid_ready() ) {
      // --- BLANK REPORT TEST ---
      // Create a blank report, only setting the report ID.
      hid_ds4_report_t report = {0};
      report.report_id = 0x01;

      // For this test, we must set a neutral D-pad value that is valid.
      // The descriptor logical range is 0-7. A value outside this range is
      // treated as the "Null State". 8 is the standard for neutral.
      // Let's try sending a report with a valid D-pad value first,
      // like UP, and then a neutral value.
      // The original code sent 8 for neutral which might be the issue.
      // Let's try sending a value within the 0-7 range.
      report.dpad = DS4_HAT_UP; // Use 0 for UP, which is inside the valid range.
      report.left_stick_x = 128;
      report.left_stick_y = 128;
      report.right_stick_x = 128;
      report.right_stick_y = 128;


      bool success = tud_hid_report(1, &report, sizeof(report));

      // --- DEBUG START: Print the result of the blank report test ---
      static uint32_t last_hid_debug_ms = 0;
      if (board_millis() - last_hid_debug_ms > 500) {
          last_hid_debug_ms = board_millis();
          char debug_str[100];
          sprintf(debug_str, "DEBUG HID: tud_hid_ready() = %d\r\n", tud_hid_ready());
          debug_puts(debug_str);
          sprintf(debug_str, "DEBUG HID: Sent BLANK report. Success = %d\r\n", success);
          debug_puts(debug_str);
      }
      // --- DEBUG END ---
    }
  #else // GENERIC
    //... (Generic path unchanged)
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
            debug_puts("DEBUG: Main loop is alive.\r\n");
        }
        // --- DEBUG END ---
    }
    return 0;
}
