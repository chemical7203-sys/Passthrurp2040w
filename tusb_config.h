#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------
// This file is included by `tusb_option.h`, which defines the `OPT_*` macros.
// We cannot use them here. The Pico SDK build system will set the MCU, OS, and
// endpoint 0 size correctly. We only need to override the class drivers.

//--------------------------------------------------------------------
// CLASS DRIVER CONFIG
//--------------------------------------------------------------------
#define CFG_TUD_CDC             0 // Disable CDC
#define CFG_TUD_MSC             0 // Disable Mass Storage
#define CFG_TUD_HID             1 // Enable HID
#define CFG_TUD_MIDI            0 // Disable MIDI
#define CFG_TUD_VENDOR          0 // Disable Vendor

//------------- HID -------------//
#define CFG_TUD_HID_RX_BUFSIZE   64
#define CFG_TUD_HID_TX_BUFSIZE   64

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_H_ */
