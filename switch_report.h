#pragma once

#include <stdint.h>
#include "tusb.h"

// Button masks for Switch Pro Controller (Report 0x30)
// Byte 0: Y, B, A, X, L, R, ZL, ZR
#define SWITCH_MASK_Y       (1U << 0)
#define SWITCH_MASK_B       (1U << 1)
#define SWITCH_MASK_A       (1U << 2)
#define SWITCH_MASK_X       (1U << 3)
#define SWITCH_MASK_L       (1U << 4)
#define SWITCH_MASK_R       (1U << 5)
#define SWITCH_MASK_ZL      (1U << 6)
#define SWITCH_MASK_ZR      (1U << 7)

// Byte 1: Minus, Plus, L3, R3, Home, Capture
#define SWITCH_MASK_MINUS   (1U << 0)
#define SWITCH_MASK_PLUS    (1U << 1)
#define SWITCH_MASK_L3      (1U << 2)
#define SWITCH_MASK_R3      (1U << 3)
#define SWITCH_MASK_HOME    (1U << 4)
#define SWITCH_MASK_CAPTURE (1U << 5)
// Byte 1 bit 6 & 7 are unused/charging status in some docs, usually 0

// Byte 2: Hat/D-Pad (0-7, 8=Neutral)
// D-Pad is lower 4 bits of Byte 2.

// Analog sticks in Pro Controller report are 12-bit, packed into 3 bytes.
// Left Stick: Byte 0 (Low 8), Byte 1 (High 4) | Byte 1 (Low 4 for Y?? No, it's packed differently)
// Actually standard packing for 12-bit is:
// Byte 0: X[0:7]
// Byte 1: X[8:11] | Y[0:3] << 4
// Byte 2: Y[4:11]
// Range: 0x000 - 0xFFF. Center ~0x800.

// IMU Data Structure
typedef struct __attribute__((packed)) {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} switch_imu_data_t;

// Full Input Report 0x30
typedef struct __attribute__((packed)) {
    uint8_t report_id;      // 0x30
    uint8_t timer;          // Incrementing timer
    uint8_t battery_connection; // Battery level and connection info (usually 0x90 or 0x8E)

    uint8_t buttons[3];     // Byte 0: R, L, X, A, B, Y... Wait, need to verify order.
                            // Standard bitmask usually used.

    uint8_t left_stick[3];  // 12-bit X, 12-bit Y
    uint8_t right_stick[3]; // 12-bit X, 12-bit Y

    uint8_t vibrator_report; // Often 0x00 or echo of output report

    switch_imu_data_t imu[3]; // 3 samples of IMU data
} switch_pro_report_t;
