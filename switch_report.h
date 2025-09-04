#pragma once

#include <stdint.h>

//
// Switch Pro Controller report defines and structs
// Based on GP2040-CE project
//

// HAT values (for dpad_to_switch_hat function if needed, though we map bits directly now)
#define SWITCH_HAT_UP        0x00
#define SWITCH_HAT_UPRIGHT   0x01
#define SWITCH_HAT_RIGHT     0x02
#define SWITCH_HAT_DOWNRIGHT 0x03
#define SWITCH_HAT_DOWN      0x04
#define SWITCH_HAT_DOWNLEFT  0x05
#define SWITCH_HAT_LEFT      0x06
#define SWITCH_HAT_UPLEFT    0x07
#define SWITCH_HAT_NOTHING   0x08

// Button masks for our `gamepad_data.buttons` (16-bit field from python client)
#define BUTTON_MASK_B       (1U << 0)
#define BUTTON_MASK_A       (1U << 1)
#define BUTTON_MASK_Y       (1U << 2)
#define BUTTON_MASK_X       (1U << 3)
#define BUTTON_MASK_L1      (1U << 4)
#define BUTTON_MASK_R1      (1U << 5)
#define BUTTON_MASK_L2_PRESS (1U << 6)
#define BUTTON_MASK_R2_PRESS (1U << 7)
#define BUTTON_MASK_SELECT  (1U << 8)
#define BUTTON_MASK_START   (1U << 9)
#define BUTTON_MASK_L3      (1U << 10)
#define BUTTON_MASK_R3      (1U << 11)
#define BUTTON_MASK_HOME    (1U << 12)
#define BUTTON_MASK_CAPTURE (1U << 13)

// DPAD masks for `gamepad_data.dpad` (8-bit field from python client)
#define DPAD_MASK_UP    (1U << 0)
#define DPAD_MASK_DOWN  (1U << 1)
#define DPAD_MASK_LEFT  (1U << 2)
#define DPAD_MASK_RIGHT (1U << 3)


// Switch analog sticks are 12-bit
#define SWITCH_JOYSTICK_MIN 0x000
#define SWITCH_JOYSTICK_MID 0x800
#define SWITCH_JOYSTICK_MAX 0xFFF

// Helper struct for packing analog stick data
typedef struct {
    uint8_t data[3];
} SwitchAnalog_t;

// Helper functions to pack 12-bit analog data into 3 bytes
static inline void set_switch_analog_x(SwitchAnalog_t* analog, uint16_t x) {
    analog->data[0] = x & 0xFF;
    analog->data[1] = (analog->data[1] & 0xF0) | ((x >> 8) & 0x0F);
}

static inline void set_switch_analog_y(SwitchAnalog_t* analog, uint16_t y) {
    analog->data[1] = (analog->data[1] & 0x0F) | ((y & 0x0F) << 4);
    analog->data[2] = (y >> 4) & 0xFF;
}

// Main input report structure. Safer to use byte arrays and masks.
typedef struct __attribute__((packed, aligned(1)))
{
    uint8_t buttons[3]; // 3 bytes for all buttons.
    SwitchAnalog_t left_stick;
    SwitchAnalog_t right_stick;
} SwitchInputReport_t;

// Full HID report structure sent to the Switch
typedef struct __attribute__((packed, aligned(1)))
{
    uint8_t report_id; // Should be 0x30 for standard input
    uint8_t timer;
    uint8_t connection_info_battery_level;
    SwitchInputReport_t inputs;
    uint8_t vibrator_report;
} hid_nintendo_report_t;

// Button bit masks for the 3-byte button field in the report
// The order is reversed from the GP2040-CE bitfield struct due to endianness.
// This layout matches what the Switch expects over USB.
// Byte 0: Y, X, B, A, RSR, RSL, R, ZR
#define PRO_CONTROLLER_MASK_Y_0 (1U << 0)
#define PRO_CONTROLLER_MASK_X_0 (1U << 1)
#define PRO_CONTROLLER_MASK_B_0 (1U << 2)
#define PRO_CONTROLLER_MASK_A_0 (1U << 3)
#define PRO_CONTROLLER_MASK_R_SR_0 (1U << 4)
#define PRO_CONTROLLER_MASK_R_SL_0 (1U << 5)
#define PRO_CONTROLLER_MASK_R_0 (1U << 6)
#define PRO_CONTROLLER_MASK_ZR_0 (1U << 7)

// Byte 1: -, +, R3, L3, Home, Capture
#define PRO_CONTROLLER_MASK_MINUS_1 (1U << 0)
#define PRO_CONTROLLER_MASK_PLUS_1 (1U << 1)
#define PRO_CONTROLLER_MASK_R3_1 (1U << 2)
#define PRO_CONTROLLER_MASK_L3_1 (1U << 3)
#define PRO_CONTROLLER_MASK_HOME_1 (1U << 4)
#define PRO_CONTROLLER_MASK_CAPTURE_1 (1U << 5)
// 2 bits unused

// Byte 2: Dpad, L, ZL
#define PRO_CONTROLLER_MASK_DPAD_DOWN_2 (1U << 0)
#define PRO_CONTROLLER_MASK_DPAD_UP_2 (1U << 1)
#define PRO_CONTROLLER_MASK_DPAD_RIGHT_2 (1U << 2)
#define PRO_CONTROLLER_MASK_DPAD_LEFT_2 (1U << 3)
#define PRO_CONTROLLER_MASK_L_SR_2 (1U << 4)
#define PRO_CONTROLLER_MASK_L_SL_2 (1U << 5)
#define PRO_CONTROLLER_MASK_L_2 (1U << 6)
#define PRO_CONTROLLER_MASK_ZL_2 (1U << 7)
