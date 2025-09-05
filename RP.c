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
#include "switch_spi_flash.h"
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
static uint8_t report_counter = 0;

#if CFG_TUD_HID_NINTENDO
// State for Switch Pro Controller handshake
static bool switch_is_ready = false;
static bool switch_is_initialized = false;

// Function to read from emulated SPI flash, adapted from GP2040-CE
void read_spi_flash(uint32_t address, uint8_t* buffer, uint16_t size) {
    if (address >= 0x6000 && address < 0x7000) {
        uint16_t offset = address - 0x6000;
        if (offset + size <= sizeof(factory_config_data)) {
            memcpy(buffer, &factory_config_data[offset], size);
        } else {
            memset(buffer, 0xFF, size); // Out of bounds
        }
    } else if (address >= 0x8000 && address < 0x9000) {
        uint16_t offset = address - 0x8000;
        if (offset + size <= sizeof(user_calibration_data)) {
            memcpy(buffer, &user_calibration_data[offset], size);
        } else {
            memset(buffer, 0xFF, size); // Out of bounds
        }
    } else {
        memset(buffer, 0xFF, size); // Address not implemented
    }
}
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

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
  // This is not used by the Switch Pro Controller communication protocol.
  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint (Report ID = 0, Type = OUTPUT)
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    if (report_type != HID_REPORT_TYPE_OUTPUT) return;

    uint8_t reply[64] = {0};
    const uint8_t report_id_output = buffer[0];

    if (report_id_output == 0x80) { // Host-sent command
        const uint8_t subcommand_id = buffer[1];
        if (subcommand_id == 0x01 || subcommand_id == 0x02 || subcommand_id == 0x03) { // Handshake commands (incl. baud rate 0x03)
            reply[0] = 0x81; // Reply ID
            reply[1] = subcommand_id; // ACK the subcommand
            tud_hid_report(0, reply, sizeof(reply));
        } else if (subcommand_id == 0x04) { // Disable timeout, handshake complete
            switch_is_ready = true;
        }
    } else if (report_id_output == 0x01 || report_id_output == 0x10) { // Rumble + Subcommand
        const uint8_t subcommand_cmd = (report_id_output == 0x01) ? buffer[10] : buffer[1];

        reply[0] = 0x21; // Standard reply report ID
        reply[1] = report_counter;

        uint8_t subcommand_ack = 0x80; // Default ACK

        if (subcommand_cmd == 0x02) { // Request device info
            subcommand_ack = 0x82; // Specific ACK for device info
            reply[15] = 0x03; // Controller type (Pro Controller)
            reply[16] = 0x48; // Colors
        } else if (subcommand_cmd == 0x10) { // SPI Flash Read
            uint32_t address = buffer[11] | (buffer[12] << 8) | (buffer[13] << 16) | (buffer[14] << 24);
            uint8_t size = buffer[15];

            subcommand_ack = 0x90; // ACK for SPI read
            memcpy(&reply[15], &buffer[11], 5); // Copy address and size back into reply
            read_spi_flash(address, &reply[20], size); // Read from flash and copy into reply
        }

        reply[13] = subcommand_ack;
        reply[14] = subcommand_cmd;
        tud_hid_report(0, reply, sizeof(reply));
    }
}

#if CFG_TUD_HID_NINTENDO
// dpad_to_switch_hat is no longer used for Pro Controller mode.
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
      if (!switch_is_initialized && tud_hid_ready()) {
          // Send initial identification report to start handshake
          uint8_t identify_report[64] = {0};
          identify_report[0] = 0x81;
          identify_report[1] = 0x01; // IDENTIFY
          tud_hid_report(0, identify_report, sizeof(identify_report));
          switch_is_initialized = true;
      }

      if (switch_is_ready) {
        hid_nintendo_report_t report = {0};
        report.report_id = 0x30;
        report.timer = report_counter++; // Use the global report_counter
        report.connection_info_battery_level = 0x80; // Full battery, wired connection

        // Clear button data
        report.inputs.buttons[0] = 0;
        report.inputs.buttons[1] = 0;
        report.inputs.buttons[2] = 0;

        // --- Button Mapping ---
        // Byte 0: Y, X, B, A, R, ZR
      if (gamepad_data.buttons & BUTTON_MASK_Y) report.inputs.buttons[0] |= PRO_CONTROLLER_MASK_Y_0;
      if (gamepad_data.buttons & BUTTON_MASK_X) report.inputs.buttons[0] |= PRO_CONTROLLER_MASK_X_0;
      if (gamepad_data.buttons & BUTTON_MASK_B) report.inputs.buttons[0] |= PRO_CONTROLLER_MASK_B_0;
      if (gamepad_data.buttons & BUTTON_MASK_A) report.inputs.buttons[0] |= PRO_CONTROLLER_MASK_A_0;
        if (gamepad_data.buttons & BUTTON_MASK_R1) report.inputs.buttons[0] |= PRO_CONTROLLER_MASK_R_0;
        if (gamepad_data.r2 > 30) report.inputs.buttons[0] |= PRO_CONTROLLER_MASK_ZR_0;

        // Byte 1: -, +, R3, L3, Home, Capture
        if (gamepad_data.buttons & BUTTON_MASK_SELECT) report.inputs.buttons[1] |= PRO_CONTROLLER_MASK_MINUS_1;
        if (gamepad_data.buttons & BUTTON_MASK_START) report.inputs.buttons[1] |= PRO_CONTROLLER_MASK_PLUS_1;
        if (gamepad_data.buttons & BUTTON_MASK_R3) report.inputs.buttons[1] |= PRO_CONTROLLER_MASK_R3_1;
        if (gamepad_data.buttons & BUTTON_MASK_L3) report.inputs.buttons[1] |= PRO_CONTROLLER_MASK_L3_1;
        if (gamepad_data.buttons & BUTTON_MASK_HOME) report.inputs.buttons[1] |= PRO_CONTROLLER_MASK_HOME_1;
        if (gamepad_data.buttons & BUTTON_MASK_CAPTURE) report.inputs.buttons[1] |= PRO_CONTROLLER_MASK_CAPTURE_1;

        // Byte 2: Dpad, L, ZL
        if (gamepad_data.dpad & DPAD_MASK_DOWN) report.inputs.buttons[2] |= PRO_CONTROLLER_MASK_DPAD_DOWN_2;
        if (gamepad_data.dpad & DPAD_MASK_UP) report.inputs.buttons[2] |= PRO_CONTROLLER_MASK_DPAD_UP_2;
        if (gamepad_data.dpad & DPAD_MASK_RIGHT) report.inputs.buttons[2] |= PRO_CONTROLLER_MASK_DPAD_RIGHT_2;
        if (gamepad_data.dpad & DPAD_MASK_LEFT) report.inputs.buttons[2] |= PRO_CONTROLLER_MASK_DPAD_LEFT_2;
        if (gamepad_data.buttons & BUTTON_MASK_L1) report.inputs.buttons[2] |= PRO_CONTROLLER_MASK_L_2;
        if (gamepad_data.l2 > 30) report.inputs.buttons[2] |= PRO_CONTROLLER_MASK_ZL_2;

        // --- Analog Stick Mapping ---
        // Scale signed 8-bit (-128 to 127) to unsigned 12-bit (0 to 4095)
        uint16_t lx_scaled = (uint16_t)((gamepad_data.lx + 128) << 4);
        uint16_t ly_scaled = (uint16_t)((gamepad_data.ly + 128) << 4);
        uint16_t rx_scaled = (uint16_t)((gamepad_data.rx + 128) << 4);
        uint16_t ry_scaled = (uint16_t)((gamepad_data.ry + 128) << 4);

        // Invert Y axis for Switch standard
        ly_scaled = 4095 - ly_scaled;
        ry_scaled = 4095 - ry_scaled;

        set_switch_analog_x(&report.inputs.left_stick, lx_scaled);
        set_switch_analog_y(&report.inputs.left_stick, ly_scaled);
        set_switch_analog_x(&report.inputs.right_stick, rx_scaled);
        set_switch_analog_y(&report.inputs.right_stick, ry_scaled);

        report.vibrator_report = 0;

        tud_hid_report(0, &report, sizeof(report));
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
    while (true) {
        tud_task();
        hid_task();
        process_uart();
        debug_task();
    }
    return 0;
}
