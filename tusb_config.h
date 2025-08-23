#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

// We are using the RP2040 MCU
#ifndef CFG_TUSB_MCU
  #define CFG_TUSB_MCU OPT_MCU_RP2040
#endif

// We are using the Pico SDK OS
#ifndef CFG_TUSB_OS
  #define CFG_TUSB_OS OPT_OS_PICO
#endif

// Enable Device stack
#define CFG_TUD_ENABLED       1

// Disable Host stack
#define CFG_TUH_ENABLED       0

// Board specific settings
#define BOARD_TUD_RHPORT      0
#define BOARD_TUD_MAX_SPEED   OPT_MODE_FULL_SPEED

// RHPort 0 is device mode
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | BOARD_TUD_MAX_SPEED)

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE    64
#endif

//--------------------------------------------------------------------
// DEVICE CLASS CONFIGURATION
//--------------------------------------------------------------------
#define CFG_TUD_CDC              0 // Disable CDC
#define CFG_TUD_MSC              0 // Disable MSC
#define CFG_TUD_HID              1 // Enable 1 HID interface
#define CFG_TUD_MIDI             0 // Disable MIDI
#define CFG_TUD_VENDOR           0 // Disable Vendor

//------------- HID -------------//
#define CFG_TUD_HID_RX_BUFSIZE   64
#define CFG_TUD_HID_TX_BUFSIZE   64

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_H_ */
