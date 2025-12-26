#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "tusb.h"
#include "bsp/board.h"
#include "class/hid/hid_device.h"

#if CFG_TUD_HID_SONY
#include "ds4_report.h"
#elif CFG_TUD_HID_NINTENDO
#include "switch_report.h"
#endif

// GPIO Pin for the 433MHz RF Transmitter's DATA line
#define RF_TX_PIN 15

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

// --- RF 433MHz rc-switch implementation ---
// This is a C port of the send method from the sui77/rc-switch C++ library.
// It is hardcoded to use Protocol 1, which is the most common.
// All timings and logic are based on the library's source code.

typedef struct {
    uint8_t high;
    uint8_t low;
} HighLow;

typedef struct {
    uint16_t pulseLength;
    HighLow syncFactor;
    HighLow zero;
    HighLow one;
} Protocol;

// Definition for Protocol 1 from the rc-switch library
const Protocol protocol = {
    350,      // pulseLength
    { 1, 31 },  // syncFactor
    { 1, 3 },   // zero
    { 3, 1 }    // one
};
const int nRepeatTransmit = 10; // Default repeat count

void transmit(HighLow pulses) {
    gpio_put(RF_TX_PIN, 1);
    sleep_us(protocol.pulseLength * pulses.high);
    gpio_put(RF_TX_PIN, 0);
    sleep_us(protocol.pulseLength * pulses.low);
}

// A C port of RCSwitch::send(unsigned long code, unsigned int length)
void send_rc_code(unsigned long code, unsigned int length) {
    for (int n = 0; n < nRepeatTransmit; n++) {
        // Send the code bits, MSB first
        for (int i = length - 1; i >= 0; i--) {
            if ((code >> i) & 1) {
                transmit(protocol.one);
            } else {
                transmit(protocol.zero);
            }
        }
        // Send the sync pulse *after* the code bits for each repetition
        transmit(protocol.syncFactor);
    }
}


// --- UART and CRC8 ---
#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5

void setup_uart() {
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
}

void debug_puts(const char *s) { uart_puts(UART_ID, s); }

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

void process_uart() {
    static uint8_t packet_buffer[32];
    static uint8_t packet_idx = 0;
    static uint8_t expected_len = 0;

    while (uart_is_readable(UART_ID)) {
        uint8_t ch = uart_getc(UART_ID);

        if (packet_idx == 0) {
            if (ch == 0xA6) {
                packet_buffer[0] = ch;
                expected_len = 23;
                packet_idx = 1;
            } else if (ch == 0xA7) {
                packet_buffer[0] = ch;
                expected_len = 6;
                packet_idx = 1;
            }
        } else {
            packet_buffer[packet_idx++] = ch;

            if (packet_idx >= expected_len) {
                uint8_t calculated_crc = crc8(packet_buffer, expected_len - 1, 0xFF);
                uint8_t received_crc = packet_buffer[expected_len - 1];

                if (calculated_crc == received_crc) {
                    if (packet_buffer[0] == 0xA6) {
                        memcpy(&gamepad_data, &packet_buffer[1], sizeof(gamepad_data));
                    } else if (packet_buffer[0] == 0xA7) {
                        unsigned long rf_code;
                        memcpy(&rf_code, &packet_buffer[1], sizeof(rf_code));
                        // The user's code is 24 bits long
                        send_rc_code(rf_code, 24);
                    }
                }
                packet_idx = 0;
                expected_len = 0;
            }
        }
    }
}

// --- Switch Pro Controller Handshake Logic ---
#if CFG_TUD_HID_NINTENDO

// Simple handshake responses to keep the Switch happy for basic wired operation.
// Based on reverse engineering logs and similar open source implementations.

static uint8_t global_packet_counter = 0;

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void) instance; (void) report_type;
    // The Switch often requests Feature Report 0x80 (MAC address / pairing info)
    // or other setup packets.
    if (report_id == 0x80) {
        // Response format is mostly proprietary, but sending a "valid" looking block often works.
        // Usually, the Switch sends a command via SET_REPORT 0x80, then reads result via GET_REPORT 0x80.
        // However, standard USB HID GetReport(Feature) logic applies.
        // For simplicity, we just return zeros or a dummy MAC if specifically asked,
        // but often the logic is driven by SET_REPORT commands + interrupt IN responses.
        memset(buffer, 0, reqlen);
        buffer[0] = 0x80; // Report ID
        // Dummy MAC: 00:00:00:00:00:01
        if(reqlen > 10) buffer[4] = 0x01;
        return reqlen;
    }
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    (void) instance; (void) report_type;

    // The Switch Pro Controller handshake is complex.
    // It involves:
    // 1. Handshake cmd 0x02 (Protocol negotiation)
    // 2. 0x01 (USB pairing)
    // 3. 0x04 (Force USB)
    // 4. 0x10 (SPI flash read/write, LED set, Home light, Enable IMU, Enable Vibration, etc.)

    // For a minimal implementation to get 0x30 input reports accepted:
    // We need to reply to the commands sent to report 0x80 (Feature) or 0x01/0x10 (Output).
    // Actually, on USB, Pro Controller uses Report 0x80 for command/response.

    if (report_id == 0x80 && bufsize > 1) {
        uint8_t cmd = buffer[1];

        // Prepare a response to send via INTERRUPT IN (on endpoint 0x81).
        // The descriptor has Report ID 0x21 (33) defined for some responses,
        // but typically standard input report 0x30 or the dedicated ACK report 0x81 is used.
        // Wait, looking at the Gist: Report 0x81 is defined as Input.

        uint8_t response[64] = {0};
        response[0] = 0x81; // Reply Report ID
        response[1] = cmd;  // Echo command
        response[2] = 0x03; // Command completed | 0x00 ??

        // For now, we blindly acknowledge.
        // Real implementation requires parsing subcommands (like Enable IMU).

        // Specifically check for "Enable IMU" (Subcommand 0x40) or "Set Input Mode" (Subcommand 0x03).
        // buffer format: [ID] [Cmd] [SubCmd] [Data...]
        // Actually, common command is 0x01 (Rumble+Subcommand).
        // If report_id is 0x80 (Feature), it's usually the handshake.

        if (cmd == 0x02) { // Handshake
            response[2] = 0x02; // OK
        } else if (cmd == 0x01) { // Manual Pairing ???
             // Dummy
        }

        // Send the response immediately?
        // The TinyUSB HID stack doesn't support "sending response to SetReport" directly except via control pipe status.
        // But Switch expects a packet on the Interrupt IN endpoint.
        tud_hid_report(0, response, 64);
    }
    // Handle Output Report 0x01 (Rumble and Subcommand)
    else if (report_id == 0x01 && bufsize >= 2) {
        // Format: [01] [GlobalPacketCount] [RumbleData(4)] [SubCommandID] [SubCommandData...]
        // We should acknowledge subcommands using Input Report 0x21 (Vendor)

        uint8_t subcommand = buffer[10];

        uint8_t ack_report[64] = {0};
        ack_report[0] = 0x21; // Vendor Report ID for ACK
        ack_report[1] = (global_packet_counter + 1) & 0x0F; // Timer
        ack_report[2] = 0x90; // Connection info (USB, Charging)
        ack_report[3] = 0x01; // Button/Stick data could go here, but usually 0 for ACK
        // ... Bytes 4-12 Button/Stick ...
        ack_report[13] = subcommand; // Acknowledge the subcommand
        ack_report[14] = 0x83; // Reply Status (0x80 = ACK, + 3 bytes data?)

        // Specific handlers
        if (subcommand == 0x03) { // Set Input Report Mode
            // 0x30 = Standard, 0x3F = Simple
            // We want 0x30.
             ack_report[14] = 0x80;
        }
        else if (subcommand == 0x40) { // Enable IMU (6-Axis)
             ack_report[14] = 0x80;
        }
        else if (subcommand == 0x48) { // Enable Vibration
             ack_report[14] = 0x80;
        }
        else if (subcommand == 0x30) { // Set Player Lights
             ack_report[14] = 0x80;
        }

        tud_hid_report(0, ack_report, 64);
    }
}

#else
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
  (void) instance; (void) buffer; (void) reqlen; (void) report_id; (void) report_type;
  return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
  (void) instance; (void) report_id; (void) report_type; (void) buffer; (void) bufsize;
}
#endif

void hid_task(void) {
  const uint32_t interval_ms = 16;
  static uint32_t start_ms = 0;
  if ( board_millis() - start_ms < interval_ms) return;
  start_ms += interval_ms;

  if ( tud_suspended() ) tud_remote_wakeup();

  #if CFG_TUD_HID_SONY
    if ( tud_hid_ready() ) {
      hid_ds4_report_t report = {0};
      report.report_id = 0x01;
      report.left_stick_x = gamepad_data.lx + 128;
      report.left_stick_y = gamepad_data.ly + 128;
      report.right_stick_x = gamepad_data.rx + 128;
      report.right_stick_y = gamepad_data.ry + 128;
      report.l2_trigger = gamepad_data.l2;
      report.r2_trigger = gamepad_data.r2;

      static const uint8_t dpad_map[16] = { 15, 0, 4, 15, 6, 7, 5, 15, 2, 1, 3, 15, 15, 15, 15, 15 };
      report.dpad = dpad_map[gamepad_data.dpad & 0x0F];

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

      // Gyro and accelerometer data must be assigned in the order they appear in the struct
      report.gyro_x = gamepad_data.gyro_x;
      report.gyro_y = gamepad_data.gyro_y;
      report.gyro_z = gamepad_data.gyro_z;
      report.accel_x = gamepad_data.accel_x;
      report.accel_y = gamepad_data.accel_y;
      report.accel_z = gamepad_data.accel_z;

      static uint8_t ds4_report_counter = 0;
      report.report_counter = ds4_report_counter++;
      tud_hid_report(0, &report, sizeof(report));
    }
  #elif CFG_TUD_HID_NINTENDO
    if ( tud_hid_ready() ) {
        switch_pro_report_t report = {0};
        report.report_id = 0x30;
        report.timer = global_packet_counter++;
        report.battery_connection = 0x90; // Charging, USB connected

        // --- Button Mapping ---
        // Byte 0: Y, B, A, X, L, R, ZL, ZR
        if ((gamepad_data.buttons >> 2) & 1) report.buttons[0] |= SWITCH_MASK_Y; // Square -> Y
        if ((gamepad_data.buttons >> 0) & 1) report.buttons[0] |= SWITCH_MASK_B; // Cross -> B
        if ((gamepad_data.buttons >> 1) & 1) report.buttons[0] |= SWITCH_MASK_A; // Circle -> A
        if ((gamepad_data.buttons >> 3) & 1) report.buttons[0] |= SWITCH_MASK_X; // Triangle -> X
        if ((gamepad_data.buttons >> 4) & 1) report.buttons[0] |= SWITCH_MASK_L; // L1 -> L
        if ((gamepad_data.buttons >> 5) & 1) report.buttons[0] |= SWITCH_MASK_R; // R1 -> R
        if ((gamepad_data.buttons >> 6) & 1) report.buttons[0] |= SWITCH_MASK_ZL; // L2 -> ZL
        if ((gamepad_data.buttons >> 7) & 1) report.buttons[0] |= SWITCH_MASK_ZR; // R2 -> ZR

        // Byte 1: Minus, Plus, L3, R3, Home, Capture
        if ((gamepad_data.buttons >> 8) & 1) report.buttons[1] |= SWITCH_MASK_MINUS; // Share -> Minus
        if ((gamepad_data.buttons >> 9) & 1) report.buttons[1] |= SWITCH_MASK_PLUS;  // Options -> Plus
        if ((gamepad_data.buttons >> 10) & 1) report.buttons[1] |= SWITCH_MASK_L3;   // L3 -> L3
        if ((gamepad_data.buttons >> 11) & 1) report.buttons[1] |= SWITCH_MASK_R3;   // R3 -> R3
        if ((gamepad_data.buttons >> 12) & 1) report.buttons[1] |= SWITCH_MASK_HOME; // PS -> Home
        if ((gamepad_data.buttons >> 13) & 1) report.buttons[1] |= SWITCH_MASK_CAPTURE; // Touchpad -> Capture

        // Byte 2: Hat (D-Pad)
        // Switch Pro Controller uses the same 0-7, 8=Neutral encoding for Hat.
        static const uint8_t dpad_map[16] = { 8, 0, 4, 8, 6, 7, 5, 8, 2, 1, 3, 8, 8, 8, 8, 8 };
        report.buttons[2] = dpad_map[gamepad_data.dpad & 0x0F];

        // --- Analog Sticks (12-bit) ---
        // Input: -128..127. Target: 0..4095 (Center 2048)
        // Conversion: (val + 128) * 16 (approx). Or (val + 128) << 4.
        uint16_t lx = (gamepad_data.lx + 128) << 4;
        uint16_t ly = (gamepad_data.ly + 128) << 4;
        // Note: Y axis inversion might be needed depending on standard.
        // Usually, Up is Min (0) on Switch Pro ?? No, Switch Pro Stick Data:
        // Calibration data usually sets the center. Assuming 0-4095 range.
        // Let's stick to standard mapping.

        // Packing 12-bit values into 3 bytes:
        // Byte 0: X[7:0]
        // Byte 1: (Y[3:0] << 4) | (X[11:8])
        // Byte 2: Y[11:4]
        report.left_stick[0] = lx & 0xFF;
        report.left_stick[1] = ((ly & 0x0F) << 4) | ((lx >> 8) & 0x0F);
        report.left_stick[2] = (ly >> 4) & 0xFF;

        uint16_t rx = (gamepad_data.rx + 128) << 4;
        uint16_t ry = (gamepad_data.ry + 128) << 4;
        report.right_stick[0] = rx & 0xFF;
        report.right_stick[1] = ((ry & 0x0F) << 4) | ((rx >> 8) & 0x0F);
        report.right_stick[2] = (ry >> 4) & 0xFF;

        // --- IMU Data ---
        // Switch expects 3 samples per packet (sampled at 1.35ms intervals usually).
        // Since we only have 1 sample from UART, we duplicate it.
        // We also need to scale/orient the data correctly.
        // PS4: Accel (Usually 1G = ~4096 or ~8192 depending on setting). Gyro (deg/s).
        // Switch: Accel (1G = 4096). Gyro (1 = 0.07 dps ? Need calibration magic usually).
        // For now, raw pass-through or simple scaling.
        // PS4 Data is int16_t. Switch Data is int16_t.
        // Axis mapping (DS4 to Switch Pro):
        // DS4: X=Right, Y=Down, Z=Backward (Standard Accelerometer)
        // Switch: X=Right, Y=Up, Z=Backward (Check this!)
        // Usually requires remapping axes.
        // For this implementation, we map X->X, Y->-Y, Z->-Z based on common orientation diffs.
        // But let's start with 1:1 mapping for verification.

        for (int i = 0; i < 3; i++) {
            report.imu[i].accel_x = gamepad_data.accel_y; // Swap X/Y/Z as needed. Let's try direct map first.
            report.imu[i].accel_y = gamepad_data.accel_z; // This is a placeholder mapping!
            report.imu[i].accel_z = gamepad_data.accel_x; // Real mapping requires physical testing.

            // Actually, based on typical controller orientation:
            // DS4 X is Right. Switch X is Right.
            // DS4 Y is Down (Gravity +). Switch Y is Up ??
            // Let's just pass through for now.
            report.imu[i].accel_x = gamepad_data.accel_x;
            report.imu[i].accel_y = gamepad_data.accel_y;
            report.imu[i].accel_z = gamepad_data.accel_z;

            report.imu[i].gyro_x = gamepad_data.gyro_x;
            report.imu[i].gyro_y = gamepad_data.gyro_y;
            report.imu[i].gyro_z = gamepad_data.gyro_z;
        }

        tud_hid_report(0, &report, sizeof(report));
    }
  #endif
}

int main() {
    board_init();
    setup_uart();
    build_crc8_table(0x07);
    tusb_init();

    // Setup RF Transmitter Pin
    gpio_init(RF_TX_PIN);
    gpio_set_dir(RF_TX_PIN, GPIO_OUT);
    gpio_put(RF_TX_PIN, 0); // Start with pin low

    while (true) {
        tud_task();
        hid_task();
        process_uart();
    }
    return 0;
}
