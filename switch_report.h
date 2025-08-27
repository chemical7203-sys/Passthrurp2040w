#pragma once

#include <stdint.h>
#include "tusb.h"

// Switch HAT report (8 bits)
#define SWITCH_HAT_UP        0
#define SWITCH_HAT_UPRIGHT   1
#define SWITCH_HAT_RIGHT     2
#define SWITCH_HAT_DOWNRIGHT 3
#define SWITCH_HAT_DOWN      4
#define SWITCH_HAT_DOWNLEFT  5
#define SWITCH_HAT_LEFT      6
#define SWITCH_HAT_UPLEFT    7
#define SWITCH_HAT_NOTHING   8

// Switch analog sticks report 16 bits
#define SWITCH_JOYSTICK_MIN 0
#define SWITCH_JOYSTICK_MID 32767
#define SWITCH_JOYSTICK_MAX 65535

/* Switch button masks */
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

// Nintendo Switch Controller
typedef struct TU_ATTR_PACKED
{
  uint16_t buttons;
  uint8_t  hat;
  uint16_t lx;
  uint16_t ly;
  uint16_t rx;
  uint16_t ry;
  uint8_t  vendor_specific;
} hid_nintendo_report_t;
