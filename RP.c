/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2021-2023, The GP2040-CE Project Team
 * SPDX-FileCopyrightText: Copyright (c) 2023, Jules Blok
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "hardware/uart.h"
#include "tusb.h"
#include "bsp/board.h"
#include "class/hid/hid_device.h"
#include "switch_report.h"

// Forward declarations
uint8_t dpad_to_switch_hat(uint8_t dpad_mask);
void pack_analog_stick(uint16_t x, uint16_t y, uint8_t* dest);

// Data from PC
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

// UART Defines
#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5

// Pro Controller State
static struct {
    bool is_ready;
    uint8_t input_mode;
    bool imu_enabled;
} pro_controller_state = {
    .is_ready = false,
    .input_mode = 0x3f,
    .imu_enabled = false
};

// Helper to pack 12-bit analog stick data
void pack_analog_stick(uint16_t x, uint16_t y, uint8_t* dest) {
    // This packing is weird. Based on GP2040-CE's SwitchAnalog struct.
    dest[0] = x & 0xFF;
    dest[1] = ((x >> 8) & 0x0F) | ((y & 0x0F) << 4);
    dest[2] = (y >> 4) & 0xFF;
}

// UART Functions
void setup_uart() {
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
}

void process_uart() {
    static uint8_t pb[25];
    static uint8_t idx = 0;
    while (uart_is_readable(UART_ID)) {
        uint8_t ch = uart_getc(UART_ID);
        if (idx == 0) {
            if (ch == 0xA6) pb[idx++] = ch;
        } else {
            pb[idx++] = ch;
            if (idx >= 25) {
                uint8_t cs = 0;
                for (int i = 0; i < 24; i++) cs ^= pb[i];
                if (cs == pb[24]) memcpy(&gamepad_data, &pb[1], sizeof(gamepad_data));
                idx = 0;
            }
        }
    }
}

void debug_task() {
    static uint32_t start_ms = 0;
    if (board_millis() - start_ms < 1000) return;
    start_ms += 1000;
    char buf[256];
    sprintf(buf, "Ready=%d, Mode=0x%02x, IMU=%d\r\n",
            pro_controller_state.is_ready, pro_controller_state.input_mode, pro_controller_state.imu_enabled);
    uart_puts(UART_ID, buf);
}

// TinyUSB HID Callbacks
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
  return 0; // Not used
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    if (report_type != HID_REPORT_TYPE_OUTPUT) return;

    uint8_t subcommand_id = 0;
    uint8_t arg_offset = 0;

    // Subcommand is at a different offset depending on the report ID
    if (report_id == SUBCOMMAND_REPORT_ID) { // 0x01
        subcommand_id = buffer[1];
        arg_offset = 2;
    } else if (report_id == RUMBLE_AND_SUBCOMMAND_REPORT_ID) { // 0x11
        subcommand_id = buffer[10];
        arg_offset = 11;
    } else if (report_id == RUMBLE_ONLY_REPORT_ID) { // 0x10
        // This is a rumble-only report, no subcommand to ACK
        return;
    } else {
        return; // Unknown report ID
    }

    switch_subcommand_response_t resp = {0};
    resp.report_id = SUBCOMMAND_ACK_REPORT_ID;
    resp.timer = buffer[0] & 0xFC;

    char debug_buf[128];
    sprintf(debug_buf, "Host sent Report ID 0x%02x, Subcommand 0x%02x\r\n", report_id, subcommand_id);
    uart_puts(UART_ID, debug_buf);

    switch (subcommand_id) {
        case SUBCOMMAND_REQUEST_DEVICE_INFO: {
            resp.ack = 0x82; // ACK with data for this subcommand
            resp.subcommand_id = subcommand_id;
            switch_device_info_t* info = (switch_device_info_t*)resp.payload;
            info->fw_version = 0x9104;
            info->controller_type = SWITCH_TYPE_PRO_CONTROLLER;
            info->mac_address[0] = 0xDE; info->mac_address[1] = 0xAD; info->mac_address[2] = 0xBE;
            info->mac_address[3] = 0xEF; info->mac_address[4] = 0xFE; info->mac_address[5] = 0xED;
            tud_hid_report(0, &resp, sizeof(resp));
            break;
        }
        case SUBCOMMAND_SET_INPUT_REPORT_MODE:
            pro_controller_state.input_mode = buffer[arg_offset];
            resp.ack = 0x80; // Standard ACK
            resp.subcommand_id = subcommand_id;
            tud_hid_report(0, &resp, 16); // Send a minimal ACK
            break;
        case SUBCOMMAND_ENABLE_IMU:
            pro_controller_state.imu_enabled = (buffer[arg_offset] == 0x01);
            resp.ack = 0x80;
            resp.subcommand_id = subcommand_id;
            tud_hid_report(0, &resp, 16);
            pro_controller_state.is_ready = true; // Assume ready after this command
            break;
        case SUBCOMMAND_SPI_FLASH_READ:
            resp.ack = 0x90; // ACK with data
            resp.subcommand_id = subcommand_id;
            memcpy(resp.payload, &buffer[arg_offset], 5);
            memset(resp.payload + 5, 0xFF, 32);
            tud_hid_report(0, &resp, sizeof(resp));
            break;
        default: {
            resp.ack = 0x80;
            resp.subcommand_id = subcommand_id;
            tud_hid_report(0, &resp, 16);
            break;
        }
    }
}

uint8_t dpad_to_switch_hat(uint8_t dpad_mask) {
    static const uint8_t hat_map[16] = {
        SWITCH_HAT_NOTHING, SWITCH_HAT_UP, SWITCH_HAT_DOWN, SWITCH_HAT_NOTHING,
        SWITCH_HAT_LEFT, SWITCH_HAT_UPLEFT, SWITCH_HAT_DOWNLEFT, SWITCH_HAT_NOTHING,
        SWITCH_HAT_RIGHT, SWITCH_HAT_UPRIGHT, SWITCH_HAT_DOWNRIGHT, SWITCH_HAT_NOTHING,
        SWITCH_HAT_NOTHING, SWITCH_HAT_NOTHING, SWITCH_HAT_NOTHING, SWITCH_HAT_NOTHING
    };
    return hat_map[dpad_mask & 0x0F];
}

void hid_task(void) {
    static uint32_t last_report_ms = 0;
    if (board_millis() - last_report_ms < 5) return; // Send reports slightly more often
    last_report_ms = board_millis();

    if (tud_suspended()) tud_remote_wakeup();

    if (pro_controller_state.is_ready) {
        switch_pro_report_t report = {0};
        report.report_id = STANDARD_INPUT_REPORT_ID;
        report.timer = report_counter++;

        // Button Mapping - Corrected based on switch_report.h masks
        // Input `gamepad_data.buttons` bits:
        // 0:B, 1:A, 2:Y, 3:X, 4:L, 5:R, 8:-, 9:+, 10:L3, 11:R3, 12:Home, 13:Capture
        // Triggers l2/r2 are separate.

        // Right-side buttons
        if (gamepad_data.buttons & (1 << 2)) report.inputs.buttons_right |= SWITCH_MASK_Y; // Y
        if (gamepad_data.buttons & (1 << 3)) report.inputs.buttons_right |= SWITCH_MASK_X; // X
        if (gamepad_data.buttons & (1 << 0)) report.inputs.buttons_right |= SWITCH_MASK_B; // B
        if (gamepad_data.buttons & (1 << 1)) report.inputs.buttons_right |= SWITCH_MASK_A; // A
        if (gamepad_data.buttons & (1 << 5)) report.inputs.buttons_right |= SWITCH_MASK_R; // R
        if (gamepad_data.r2 > 30)            report.inputs.buttons_right |= SWITCH_MASK_ZR;// ZR

        // Middle buttons
        if (gamepad_data.buttons & (1 << 8)) report.inputs.buttons_middle |= SWITCH_MASK_MINUS;
        if (gamepad_data.buttons & (1 << 9)) report.inputs.buttons_middle |= SWITCH_MASK_PLUS;
        if (gamepad_data.buttons & (1 << 11)) report.inputs.buttons_middle |= SWITCH_MASK_R3;
        if (gamepad_data.buttons & (1 << 10)) report.inputs.buttons_middle |= SWITCH_MASK_L3;
        if (gamepad_data.buttons & (1 << 12)) report.inputs.buttons_middle |= SWITCH_MASK_HOME;
        if (gamepad_data.buttons & (1 << 13)) report.inputs.buttons_middle |= SWITCH_MASK_CAPTURE;

        // Left-side buttons and D-pad
        uint8_t hat = dpad_to_switch_hat(gamepad_data.dpad);
        if (hat == SWITCH_HAT_UP || hat == SWITCH_HAT_UPLEFT || hat == SWITCH_HAT_UPRIGHT)       report.inputs.buttons_left |= (1 << 1);
        if (hat == SWITCH_HAT_DOWN || hat == SWITCH_HAT_DOWNLEFT || hat == SWITCH_HAT_DOWNRIGHT)     report.inputs.buttons_left |= (1 << 0);
        if (hat == SWITCH_HAT_LEFT || hat == SWITCH_HAT_UPLEFT || hat == SWITCH_HAT_DOWNLEFT)       report.inputs.buttons_left |= (1 << 3);
        if (hat == SWITCH_HAT_RIGHT || hat == SWITCH_HAT_UPRIGHT || hat == SWITCH_HAT_DOWNRIGHT)    report.inputs.buttons_left |= (1 << 2);

        if (gamepad_data.buttons & (1 << 4)) report.inputs.buttons_left |= SWITCH_MASK_L;
        if (gamepad_data.l2 > 30)            report.inputs.buttons_left |= SWITCH_MASK_ZL;

        // Analog Sticks (12-bit, centered at 2048)
        // Y axes are inverted for Switch
        uint16_t lx = (uint16_t)(((int16_t)gamepad_data.lx + 128) << 4);
        uint16_t ly = (uint16_t)(((int16_t)-gamepad_data.ly + 128) << 4);
        uint16_t rx = (uint16_t)(((int16_t)gamepad_data.rx + 128) << 4);
        uint16_t ry = (uint16_t)(((int16_t)-gamepad_data.ry + 128) << 4);

        uint8_t stick_buffer[6];
        pack_analog_stick(lx, ly, (uint8_t*)&report.inputs.left_stick);
        pack_analog_stick(rx, ry, (uint8_t*)&report.inputs.right_stick);

        // IMU
        if (pro_controller_state.imu_enabled) {
            int16_t* imu_payload = (int16_t*)report.imu_data;
            // The Switch report sends 3 samples of IMU data. We'll just repeat our one sample.
            for (int i = 0; i < 3; i++) {
                imu_payload[i*6 + 0] = gamepad_data.ax;
                imu_payload[i*6 + 1] = gamepad_data.ay;
                imu_payload[i*6 + 2] = gamepad_data.az;
                imu_payload[i*6 + 3] = gamepad_data.gx;
                imu_payload[i*6 + 4] = gamepad_data.gy;
                imu_payload[i*6 + 5] = gamepad_data.gz;
            }
        }

        tud_hid_report(0, &report, sizeof(report));
    }
}

// Main
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
