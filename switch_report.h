#pragma once

#include <stdint.h>
#include "tusb.h"

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

// Button masks for Pro Controller
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

// Switch analog sticks are 12-bit values
#define SWITCH_JOYSTICK_MIN 0
#define SWITCH_JOYSTICK_MID 2048
#define SWITCH_JOYSTICK_MAX 4095

// Struct for the standard input report 0x30
typedef struct __attribute((packed, aligned(1)))
{
    uint8_t report_id; // 0x30
    uint8_t timer;
    uint8_t buttons[3];
    uint8_t sticks[6]; // 4x 12-bit values for sticks
    uint8_t imu_data[36];
} pro_controller_report_t;

// For backwards compatibility with the old hid_task, we can keep the old struct name
// but point it to the new, larger struct.
typedef pro_controller_report_t hid_nintendo_report_t;


// Nintendo-specific subcommands sent via HID Set_Report
typedef enum {
    SUBCOMMAND_REQUEST_DEVICE_INFO = 0x02,
    SUBCOMMAND_SET_INPUT_REPORT_MODE = 0x03,
    SUBCOMMAND_SET_PLAYER_LIGHTS = 0x30,
    SUBCOMMAND_ENABLE_IMU = 0x40,
    SUBCOMMAND_SET_IMU_SENSITIVITY = 0x41,
    SUBCOMMAND_ENABLE_VIBRATION = 0x48,
} nintendo_subcommand_t;
