/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 *
 * This file is a derivative of the GP2040-CE project's PS4Descriptors.h,
 * and is updated with structs from the passinglink project.
 */

#pragma once

#include <stdint.h>

// HAT report values
#define DS4_HAT_UP        0x00
#define DS4_HAT_UPRIGHT   0x01
#define DS4_HAT_RIGHT     0x02
#define DS4_HAT_DOWNRIGHT 0x03
#define DS4_HAT_DOWN      0x04
#define DS4_HAT_DOWNLEFT  0x05
#define DS4_HAT_LEFT      0x06
#define DS4_HAT_UPLEFT    0x07
#define DS4_HAT_NEUTRAL   0x08

// Joystick values
#define DS4_JOYSTICK_MIN 0x00
#define DS4_JOYSTICK_MID 0x80
#define DS4_JOYSTICK_MAX 0xFF

// Touchpad structs from passinglink project
// These are used in the main DS4 report
typedef struct __attribute__((packed)) {
  uint8_t counter : 7;
  uint8_t unpressed : 1;
  uint8_t data[3]; // 12 bit X, followed by 12 bit Y
} TouchpadXY;

typedef struct __attribute__((packed)) {
  TouchpadXY p1;
  TouchpadXY p2;
} TouchpadData;

// Main DS4 report struct, based on passinglink's `OutputReport`
// This struct MUST match the HID descriptor in usb_descriptors.c
typedef struct __attribute__((packed)) {
  uint8_t report_id;
  uint8_t left_stick_x;
  uint8_t left_stick_y;
  uint8_t right_stick_x;
  uint8_t right_stick_y;

  // D-pad, buttons, and counter are packed into the next 3 bytes
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
  uint8_t tpad_click : 1;
  uint8_t report_counter : 6;

  uint8_t l2_trigger;
  uint8_t r2_trigger;

  // The rest of the report is mostly for sensors and other features.
  // We must include them so the struct has the correct total size (64 bytes),
  // even if we don't populate them with real data yet.
  uint16_t timestamp;
  uint8_t  battery_level;
  int16_t  gyro_x;
  int16_t  gyro_y;
  int16_t  gyro_z;
  int16_t  accel_x;
  int16_t  accel_y;
  int16_t  accel_z;
  uint8_t  reserved[5];
  uint8_t  extended_data;
  uint8_t  reserved2[2];
  TouchpadData touchpad;
  uint8_t  reserved3[23]; // Padded from 20 to 23 to make total size 64
} hid_ds4_report_t;
