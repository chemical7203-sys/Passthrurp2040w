#pragma once

#include <stdint.h>
#include "tusb.h"

// Pro Controller specific report defines
#define PRO_SWITCH_REPORT_ID 0x30

// Hat values (same as standard Switch)
#define PRO_SWITCH_HAT_UP        0x00
#define PRO_SWITCH_HAT_UPRIGHT   0x01
#define PRO_SWITCH_HAT_RIGHT     0x02
#define PRO_SWITCH_HAT_DOWNRIGHT 0x03
#define PRO_SWITCH_HAT_DOWN      0x04
#define PRO_SWITCH_HAT_DOWNLEFT  0x05
#define PRO_SWITCH_HAT_LEFT      0x06
#define PRO_SWITCH_HAT_UPLEFT    0x07
#define PRO_SWITCH_HAT_NOTHING   0x08

// Pro Controller button masks
#define PRO_SWITCH_MASK_Y       (1U << 0)
#define PRO_SWITCH_MASK_B       (1U << 1)
#define PRO_SWITCH_MASK_A       (1U << 2)
#define PRO_SWITCH_MASK_X       (1U << 3)
#define PRO_SWITCH_MASK_L       (1U << 4)
#define PRO_SWITCH_MASK_R       (1U << 5)
#define PRO_SWITCH_MASK_ZL      (1U << 6)
#define PRO_SWITCH_MASK_ZR      (1U << 7)
#define PRO_SWITCH_MASK_MINUS   (1U << 8)
#define PRO_SWITCH_MASK_PLUS    (1U << 9)
#define PRO_SWITCH_MASK_L3      (1U << 10)
#define PRO_SWITCH_MASK_R3      (1U << 11)
#define PRO_SWITCH_MASK_HOME    (1U << 12)
#define PRO_SWITCH_MASK_CAPTURE (1U << 13)

#define PRO_SWITCH_JOYSTICK_MIN 0
#define PRO_SWITCH_JOYSTICK_MID 128
#define PRO_SWITCH_JOYSTICK_MAX 255

// Full Pro Controller USB Report
typedef struct __attribute((packed, aligned(1)))
{
  uint8_t  report_id;
  uint8_t  timer;
  uint8_t  buttons[3];
  uint8_t  hat;
  uint8_t  lx;
  uint8_t  ly;
  uint8_t  rx;
  uint8_t  ry;
  uint8_t  vibrator; // 0x08
  // IMU data
  uint8_t imu_data[36];
} hid_pro_switch_report_t;
