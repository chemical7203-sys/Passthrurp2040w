#ifndef USB_DESCRIPTORS_H_
#define USB_DESCRIPTORS_H_

#include "tusb.h"

// HID Gamepad Report.
// This structure defines the data format for the gamepad input report.
// It must match the HID report descriptor.
typedef struct TU_ATTR_PACKED
{
  uint8_t buttons; // 8 buttons, 1 bit for each.
  int8_t  x;       // Joystick X-axis: -127 to 127
  int8_t  y;       // Joystick Y-axis: -127 to 127
  // We can add more axes or buttons here later.
} hid_gamepad_report_t;


#endif /* USB_DESCRIPTORS_H_ */
