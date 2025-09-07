/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2021-2023, The GP2040-CE Project Team
 * SPDX-FileCopyrightText: Copyright (c) 2023, Jules Blok
 */

#pragma once

#include <stdint.h>

// From GP2040-CE
//--------------------------------------------------------------------

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

// Button masks
#define SWITCH_MASK_Y       (1U << 0)
#define SWITCH_MASK_B       (1U << 1)
#define SWITCH_MASK_A       (1U << 2)
#define SWITCH_MASK_X       (1U << 3)
#define SWITCH_MASK_L       (1U << 6)
#define SWITCH_MASK_R       (1U << 6) // Note: L and R have same bit in different bytes
#define SWITCH_MASK_ZL      (1U << 7)
#define SWITCH_MASK_ZR      (1U << 7) // Note: ZL and ZR have same bit in different bytes
#define SWITCH_MASK_MINUS   (1U << 0)
#define SWITCH_MASK_PLUS    (1U << 1)
#define SWITCH_MASK_L3      (1U << 3)
#define SWITCH_MASK_R3      (1U << 2)
#define SWITCH_MASK_HOME    (1U << 4)
#define SWITCH_MASK_CAPTURE (1U << 5)

// Report IDs
typedef enum {
    SUBCOMMAND_REPORT_ID = 0x01,
    RUMBLE_ONLY_REPORT_ID = 0x10,
    RUMBLE_AND_SUBCOMMAND_REPORT_ID = 0x11,
    SUBCOMMAND_ACK_REPORT_ID = 0x21,
    STANDARD_INPUT_REPORT_ID = 0x30,
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

// Controller Type Enum that was missing
typedef enum {
    SWITCH_TYPE_LEFT_JOYCON = 0x01,
    SWITCH_TYPE_RIGHT_JOYCON = 0x02,
    SWITCH_TYPE_PRO_CONTROLLER = 0x03,
} switch_controller_type_t;

// Structs for report building
typedef struct __attribute__((packed, aligned(1)))
{
    uint8_t data[3];
} switch_analog_stick_t;

typedef struct __attribute__((packed, aligned(1)))
{
    uint8_t buttons_right;
    uint8_t buttons_middle;
    uint8_t buttons_left;
    switch_analog_stick_t left_stick;
    switch_analog_stick_t right_stick;
} switch_input_report_t;

// This is the main 64-byte report sent to the host
typedef struct __attribute__((packed, aligned(1)))
{
    uint8_t report_id;
    uint8_t timer;
    switch_input_report_t inputs;
    uint8_t rumble;
    uint8_t imu_data[36];
    uint8_t padding[15];
} switch_pro_report_t;

// For responding to 0x02 subcommand
typedef struct __attribute__((packed, aligned(1)))
{
    uint16_t fw_version;
    uint8_t controller_type;
    uint8_t unknown_1; // 0x02
    uint8_t mac_address[6];
    uint8_t unknown_2; // 0x01
    uint8_t use_spi_colors; // 0x01
} switch_device_info_t;

// For ACKing subcommands
typedef struct __attribute__((packed, aligned(1)))
{
    uint8_t report_id; // 0x21
    uint8_t timer;
    // Followed by standard input report data...
    uint8_t buttons_right;
    uint8_t buttons_middle;
    uint8_t buttons_left;
    uint8_t sticks[6];
    // Followed by ACK payload
    uint8_t ack;
    uint8_t subcommand_id;
    uint8_t payload[37];
} switch_subcommand_response_t;
