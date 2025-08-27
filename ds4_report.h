/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 *
 * This file is a derivative of the GP2040-CE project's PS4Descriptors.h.
 */

#pragma once

#include <stdint.h>

// HAT report (4 bits)
#define DS4_HAT_UP        0x00
#define DS4_HAT_UPRIGHT   0x01
#define DS4_HAT_RIGHT     0x02
#define DS4_HAT_DOWNRIGHT 0x03
#define DS4_HAT_DOWN      0x04
#define DS4_HAT_DOWNLEFT  0x05
#define DS4_HAT_LEFT      0x06
#define DS4_HAT_UPLEFT    0x07
#define DS4_HAT_NOTHING   0x08

// PS4 analog sticks only report 8 bits
#define DS4_JOYSTICK_MIN 0x00
#define DS4_JOYSTICK_MID 0x80
#define DS4_JOYSTICK_MAX 0xFF

// touchpad resolution = 1920x943
#define DS4_TP_X_MIN 0
#define DS4_TP_X_MAX 1920
#define DS4_TP_Y_MIN 0
#define DS4_TP_Y_MAX 943

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
    // BEGIN HID-Compliant 3-byte Button/D-pad/Counter Block
    uint8_t dpad : 4;
    uint8_t square : 1;
    uint8_t cross : 1;
    uint8_t circle : 1;
    uint8_t triangle : 1;

    uint8_t l1 : 1;
    uint8_t r1 : 1;
    uint8_t l2 : 1;
    uint8_t r2 : 1;
    uint8_t share : 1;
    uint8_t options : 1;
    uint8_t l3 : 1;
    uint8_t r3 : 1;

    uint8_t ps : 1;
    uint8_t tpad : 1;
    uint8_t report_counter : 6;
    // END HID-Compliant 3-byte Button/D-pad/Counter Block
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


extern const uint8_t ds4_report_descriptor[];
