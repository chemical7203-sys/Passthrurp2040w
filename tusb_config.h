#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------
#ifndef CFG_TUD_MCU
#define CFG_TUD_MCU           OPT_MCU_RP2040
#endif

#define CFG_TUD_OS              OPT_OS_PICO
#define CFG_TUD_ENDPOINT0_SIZE  64

//--------------------------------------------------------------------
// CLASS DRIVER CONFIG
//--------------------------------------------------------------------

// How many Host specific class should be supported
#ifndef CFG_TUH_HUB
#define CFG_TUH_HUB           0
#endif

#ifndef CFG_TUH_CDC
#define CFG_TUH_CDC           0
#endif

#ifndef CFG_TUH_HID
#define CFG_TUH_HID           0
#endif

#ifndef CFG_TUH_MSC
#define CFG_TUH_MSC           0
#endif

#ifndef CFG_TUH_VENDOR
#define CFG_TUH_VENDOR        0
#endif

// How many Device specific class should be supported
#ifndef CFG_TUD_CDC
#define CFG_TUD_CDC           1
#endif

#ifndef CFG_TUD_MSC
#define CFG_TUD_MSC           0
#endif

#ifndef CFG_TUD_HID
#define CFG_TUD_HID           1 // <-- Enable HID class
#endif

#ifndef CFG_TUD_MIDI
#define CFG_TUD_MIDI          0
#endif

#ifndef CFG_TUD_VENDOR
#define CFG_TUD_VENDOR        0
#endif

//------------- CDC -------------//
#define CFG_TUD_CDC_RX_BUFSIZE  256
#define CFG_TUD_CDC_TX_BUFSIZE  256

//------------- HID -------------//
#define CFG_TUD_HID_RX_BUFSIZE  64
#define CFG_TUD_HID_TX_BUFSIZE  64


#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_H_ */
