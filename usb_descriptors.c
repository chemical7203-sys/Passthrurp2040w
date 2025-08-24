#include "tusb.h"

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x2E8A, // Raspberry Pi
    .idProduct          = 0xC003, // New Product ID for full gamepad
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

uint8_t const * tud_descriptor_device_cb(void) {
  return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// HID Report Descriptor
//--------------------------------------------------------------------+
uint8_t const desc_hid_report[] =
{
    HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP     ),
    HID_USAGE      ( HID_USAGE_DESKTOP_GAMEPAD  ),
    HID_COLLECTION ( HID_COLLECTION_APPLICATION ),
        // Report ID
        HID_REPORT_ID(1),
        // 16 Buttons
        HID_USAGE_PAGE ( HID_USAGE_PAGE_BUTTON ),
        HID_USAGE_MIN    ( 1  ),
        HID_USAGE_MAX    ( 16 ),
        HID_LOGICAL_MIN  ( 0  ),
        HID_LOGICAL_MAX  ( 1  ),
        HID_REPORT_COUNT ( 16 ),
        HID_REPORT_SIZE  ( 1  ),
        HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),
        // D-Pad (Hat switch)
        HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP ),
        HID_USAGE      ( HID_USAGE_DESKTOP_HAT_SWITCH ),
        HID_LOGICAL_MIN  ( 0 ),
        HID_LOGICAL_MAX  ( 7 ), // 8 directions
        HID_PHYSICAL_MIN ( 0 ),
        HID_PHYSICAL_MAX_N ( 315, 2 ),
        HID_REPORT_COUNT ( 1 ),
        HID_REPORT_SIZE  ( 4 ),
        HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),
        // Padding for Hat switch to make it a full byte
        HID_REPORT_COUNT ( 1 ),
        HID_REPORT_SIZE  ( 4 ),
        HID_INPUT        ( HID_CONSTANT | HID_VARIABLE | HID_ABSOLUTE ),
        // 4 Analog Axes (LX, LY, RX, RY)
        HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP ),
        HID_USAGE      ( HID_USAGE_DESKTOP_X  ),
        HID_USAGE      ( HID_USAGE_DESKTOP_Y  ),
        HID_USAGE      ( HID_USAGE_DESKTOP_RX ),
        HID_USAGE      ( HID_USAGE_DESKTOP_RY ),
        HID_LOGICAL_MIN  ( -127 ),
        HID_LOGICAL_MAX  ( 127 ),
        HID_REPORT_COUNT ( 4 ),
        HID_REPORT_SIZE  ( 8 ),
        HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),
        // 2 Trigger Axes (L2, R2)
        HID_USAGE      ( HID_USAGE_DESKTOP_Z ),
        HID_USAGE      ( HID_USAGE_DESKTOP_RZ ),
        HID_LOGICAL_MIN  ( 0 ),
        HID_LOGICAL_MAX  ( 255 ),
        HID_REPORT_COUNT ( 2 ),
        HID_REPORT_SIZE  ( 8 ),
        HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),
    HID_COLLECTION_END
};

uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance) {
  (void) instance;
  return desc_hid_report;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
#define EPNUM_HID   0x81

uint8_t const desc_configuration[] =
{
  TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
  TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 10)
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
  (void) index;
  return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+
char const* string_desc_arr [] =
{
  (char[]){0x09, 0x04},
  "JulesCorp",
  "Pico Full Gamepad",
  "123456",
};

static uint16_t _desc_str[32];

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) langid;
  uint8_t chr_count;

  if ( index == 0) {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  } else {
    if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) return NULL;
    const char* str = string_desc_arr[index];
    chr_count = strlen(str);
    if ( chr_count > 31 ) chr_count = 31;
    for(uint8_t i=0; i<chr_count; i++) {
      _desc_str[1+i] = str[i];
    }
  }
  _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);
  return _desc_str;
}
