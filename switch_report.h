/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2021 Jason Skuby (mytechtoybox.com)
 * SPDX-FileCopyrightText: Copyright (c) 2023 Jules Blok
 */

#pragma once

#include <stdint.h>

#define SWITCH_PRO_ENDPOINT_SIZE 64

// HAT report (4 bits)
#define SWITCH_HAT_UP        0x00
#define SWITCH_HAT_UPRIGHT   0x01
#define SWITCH_HAT_RIGHT     0x02
#define SWITCH_HAT_DOWNRIGHT 0x03
#define SWITCH_HAT_DOWN      0x04
#define SWITCH_HAT_DOWNLEFT  0x05
#define SWITCH_HAT_LEFT      0x06
#define SWITCH_HAT_UPLEFT    0x07
#define SWITCH_HAT_NOTHING   0x08

// Button masks from GP2040-CE
#define SWITCH_MASK_Y       (1U << 0)
#define SWITCH_MASK_B       (1U << 1)
#define SWITCH_MASK_A       (1U << 2)
#define SWITCH_MASK_X       (1U << 3)
#define SWITCH_MASK_L       (1U << 6)
#define SWITCH_MASK_R       (1U << 6)
#define SWITCH_MASK_ZL      (1U << 7)
#define SWITCH_MASK_ZR      (1U << 7)
#define SWITCH_MASK_MINUS   (1U << 0)
#define SWITCH_MASK_PLUS    (1U << 1)
#define SWITCH_MASK_L3      (1U << 3)
#define SWITCH_MASK_R3      (1U << 2)
#define SWITCH_MASK_HOME    (1U << 4)
#define SWITCH_MASK_CAPTURE (1U << 5)

// Switch analog sticks are 12-bit values
#define SWITCH_JOYSTICK_MIN 0x000
#define SWITCH_JOYSTICK_MID 0x800
#define SWITCH_JOYSTICK_MAX 0xFFF

// Report IDs
typedef enum {
    // Output reports
    SUBCOMMAND_REPORT_ID = 0x01,
    RUMBLE_ONLY_REPORT_ID = 0x10,
    RUMBLE_AND_SUBCOMMAND_REPORT_ID = 0x11,
    // Input reports
    SUBCOMMAND_ACK_REPORT_ID = 0x21,
    STANDARD_INPUT_REPORT_ID = 0x30,
    // Handshake reports
    HANDSHAKE_REPORT_ID = 0x81,
} switch_report_id_t;

// Subcommand IDs sent from host
typedef enum {
    SUBCOMMAND_REQUEST_DEVICE_INFO = 0x02,
    SUBCOMMAND_SET_SHIPMENT = 0x08,
    SUBCOMMAND_SPI_FLASH_READ = 0x10,
    SUBCOMMAND_SET_INPUT_REPORT_MODE = 0x03,
    SUBCOMMAND_SET_PLAYER_LIGHTS = 0x30,
    SUBCOMMAND_ENABLE_IMU = 0x40,
    SUBCOMMAND_ENABLE_VIBRATION = 0x48,
} switch_subcommand_t;

typedef enum {
    SWITCH_TYPE_LEFT_JOYCON = 0x01,
    SWITCH_TYPE_RIGHT_JOYCON = 0x02,
    SWITCH_TYPE_PRO_CONTROLLER = 0x03,
} switch_controller_type_t;


typedef struct __attribute((packed, aligned(1)))
{
    // Left Stick
    uint8_t lx_msb;
    uint8_t l_lsb; // 4 bits lx, 4 bits ly
    uint8_t ly_msb;
    // Right Stick
    uint8_t rx_msb;
    uint8_t r_lsb; // 4 bits rx, 4 bits ry
    uint8_t ry_msb;
} switch_analog_sticks_t;


typedef struct __attribute((packed, aligned(1)))
{
    uint8_t buttons_right; // Y, X, B, A, SR, SL, R, ZR
    uint8_t buttons_middle; // -, +, R3, L3, Home, Capture, -, Charging Grip
    uint8_t buttons_left; // D-pad, SR, SL, L, ZL
    switch_analog_sticks_t sticks;
} switch_input_report_t;


typedef struct __attribute((packed, aligned(1)))
{
    uint8_t report_id;
    uint8_t timer;
    switch_input_report_t inputs;
    uint8_t rumble_report[8]; // GP2040 uses 1 byte, but pro controller has 8
    uint8_t imu_data[36];
} switch_pro_report_t;


// Response to subcommand 0x02
typedef struct __attribute((packed, aligned(1)))
{
    uint16_t fw_version; // 0x0348 for 3.89
    uint8_t controller_type; // 0x03 for Pro Controller
    uint8_t unknown_1;
    uint8_t mac_address[6];
    uint8_t unknown_2;
    uint8_t use_spi_colors;
} switch_device_info_t;

// Standard ACK response for subcommands
typedef struct __attribute((packed, aligned(1)))
{
    uint8_t report_id; // 0x21
    uint8_t timer;
    uint8_t buttons[3];
    switch_analog_sticks_t sticks;
    uint8_t sub_ack; // 0x80 | subcommand_id
    uint8_t subcommand_id;
    uint8_t payload[37]; // Response data
} switch_subcommand_response_t;
