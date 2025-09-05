#pragma once

#include <stdint.h>

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

// Button report (24 bits)
#define SWITCH_MASK_Y       (1U << 0)
#define SWITCH_MASK_B       (1U << 1)
#define SWITCH_MASK_A       (1U << 2)
#define SWITCH_MASK_X       (1U << 3)
#define SWITCH_MASK_L       (1U << 4)
#define SWITCH_MASK_R       (1U << 5)
#define SWITCH_MASK_ZL      (1U << 6)
#define SWITCH_MASK_ZR      (1U << 7)
#define SWITCH_MASK_MINUS   (1U << 8)
#define SWITCH_MASK_PLUS    (1U << 9)
#define SWITCH_MASK_L3      (1U << 10)
#define SWITCH_MASK_R3      (1U << 11)
#define SWITCH_MASK_HOME    (1U << 12)
#define SWITCH_MASK_CAPTURE (1U << 13)

// Switch analog sticks only report 8 bits
#define SWITCH_JOYSTICK_MIN 0x00
#define SWITCH_JOYSTICK_MID 0x80
#define SWITCH_JOYSTICK_MAX 0xFF

// This is the feature report that contains the IMU data
typedef struct __attribute((packed, aligned(1)))
{
    uint8_t reportId; // Should be 0x21, 0x30, etc.
    // Accelerometer
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    // Gyroscope
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} switch_imu_report_t;


// This is the full input report that we send to the host
typedef struct __attribute((packed, aligned(1)))
{
    uint8_t report_id; // 0x30
    uint8_t timer;
    uint8_t connection_info; // battery level, connection type
    uint8_t buttons_right; // Y, B, A, X, R, L, ZR, ZL
    uint8_t buttons_mid; // Minus, Plus, R3, L3, Home, Capture, -, -
    uint8_t buttons_left; // Not used by this report, but part of the 3-byte button field
    uint8_t hat;
    uint8_t lx;
    uint8_t ly;
    uint8_t rx;
    uint8_t ry;
    uint8_t vibrator_report; // Echo of vibrator command
    switch_imu_report_t imu_reports[3];
} hid_nintendo_report_t;
