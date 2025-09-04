/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 *
 * This file is a derivative of the GP2040-CE project's PS4Descriptors.h.
 */

#ifndef _DS4_REPORT_H_
#define _DS4_REPORT_H_

#include <stdint.h>

#define PS4_ENDPOINT_SIZE 64

// DS4 Vendor/Product IDs
#define DS4_VENDOR_ID         0x054C
#define DS4_PRODUCT_ID        0x09CC

// Third-party Vendor/Product IDs for compatibility
#define PS4_RZR_VENDOR_ID     0x1532
#define PS4_RZR_PRODUCT_ID    0x0401


/**************************************************************************
 *
 *  Endpoint Buffer Configuration
 *
 **************************************************************************/

#define ENDPOINT0_SIZE  64

#define PS4_FEATURES_SIZE 32

#define GAMEPAD_INTERFACE       0
#define GAMEPAD_ENDPOINT        1
#define GAMEPAD_SIZE            64

#define LSB(n) (n & 255)
#define MSB(n) ((n >> 8) & 255)

// HAT report (4 bits)
#define PS4_HAT_UP        0x00
#define PS4_HAT_UPRIGHT   0x01
#define PS4_HAT_RIGHT     0x02
#define PS4_HAT_DOWNRIGHT 0x03
#define PS4_HAT_DOWN      0x04
#define PS4_HAT_DOWNLEFT  0x05
#define PS4_HAT_LEFT      0x06
#define PS4_HAT_UPLEFT    0x07
#define PS4_HAT_NOTHING   0x08

// Button report (16 bits)
#define PS4_MASK_SQUARE   (1U <<  4)
#define PS4_MASK_CROSS    (1U <<  5)
#define PS4_MASK_CIRCLE   (1U <<  6)
#define PS4_MASK_TRIANGLE (1U <<  7)
#define PS4_MASK_L1       (1U <<  8)
#define PS4_MASK_R1       (1U <<  9)
#define PS4_MASK_L2       (1U << 10)
#define PS4_MASK_R2       (1U << 11)
#define PS4_MASK_SELECT   (1U << 12)
#define PS4_MASK_START    (1U << 13)
#define PS4_MASK_L3       (1U << 14)
#define PS4_MASK_R3       (1U << 15)
#define PS4_MASK_PS       (1U << 16)
#define PS4_MASK_TP       (1U << 17)

// PS4 analog sticks only report 8 bits
#define PS4_JOYSTICK_MIN 0x00
#define PS4_JOYSTICK_MID 0x80
#define PS4_JOYSTICK_MAX 0xFF

// touchpad resolution = 1920x943
#define PS4_TP_X_MIN 0
#define PS4_TP_X_MAX 1920
#define PS4_TP_Y_MIN 0
#define PS4_TP_Y_MAX 943

typedef struct __attribute__((packed)) {
    uint8_t counter : 7;
    uint8_t unpressed : 1;
    uint8_t data[3]; // 12 bit X, followed by 12 bit Y
} TouchpadXY;

typedef struct __attribute__((packed)) {
    TouchpadXY p1;
    TouchpadXY p2;
} TouchpadData;

typedef struct __attribute__((packed)) {
    int16_t x;
    int16_t y;
    int16_t z;
} PSSensor;

typedef struct __attribute__((packed)) {
    uint16_t timestamp;
    uint8_t unknown0;
    PSSensor gyro;
    PSSensor accel;
    uint8_t unknown1[5];
    uint8_t unknown2;
} PSSensorData;

typedef struct __attribute__((packed)) {
    uint8_t report_id;
    uint8_t left_stick_x;
    uint8_t left_stick_y;
    uint8_t right_stick_x;
    uint8_t right_stick_y;

    // dpad + buttons are a single 16 bit field
    uint16_t dpad : 4;
    uint16_t square : 1;
    uint16_t cross : 1;
    uint16_t circle : 1;
    uint16_t triangle : 1;
    uint16_t l1 : 1;
    uint16_t r1 : 1;
    uint16_t l2 : 1;
    uint16_t r2 : 1;
    uint16_t share : 1;
    uint16_t options : 1;
    uint16_t l3 : 1;
    uint16_t r3 : 1;

    uint8_t ps : 1;
    uint8_t tpad : 1;
    uint8_t report_counter : 6;

    uint8_t l2_trigger;
    uint8_t r2_trigger;

    uint16_t timestamp;
    uint8_t battery;

    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;

    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;

    uint8_t unknown[5];
    uint8_t battery_level : 4;
    uint8_t usb_charging : 1;
    uint8_t unknown2 : 3;
    uint8_t unknown3[2];
    uint8_t touch_event;
    uint8_t unknown4;
    TouchpadData touchpad;
} hid_ds4_report_t;

// Renaming for consistency with GP2040-CE from which this is derived
typedef hid_ds4_report_t PS4Report;

#endif // _DS4_REPORT_H_
