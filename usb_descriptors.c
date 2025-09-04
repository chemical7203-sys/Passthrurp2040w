#include "tusb.h"
#include "class/hid/hid_device.h"

#if CFG_TUD_HID_SONY
#include "ds4_report.h"
#elif CFG_TUD_HID_NINTENDO
#include "switch_report.h"
#endif


//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
#if CFG_TUD_HID_NINTENDO
// Switch Device Descriptor (as HORI Pokken Controller)
tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE, .bcdUSB = 0x0200,
    .bDeviceClass = 0x00, .bDeviceSubClass = 0x00, .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x0F0D, .idProduct = 0x0092, .bcdDevice = 0x0100,
    .iManufacturer = 0x01, .iProduct = 0x02, .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};
#elif CFG_TUD_HID_SONY
// DS4 Device Descriptor (Razer Panthera)
tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE, .bcdUSB = 0x0200,
    .bDeviceClass = 0x00, .bDeviceSubClass = 0x00, .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x1532, .idProduct = 0x0401, .bcdDevice = 0x0100,
    .iManufacturer = 0x01, .iProduct = 0x02, .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};
#else
// Generic Device Descriptor
tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE, .bcdUSB = 0x0200,
    .bDeviceClass = 0x00, .bDeviceSubClass = 0x00, .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x2E8A, .idProduct = 0xC003, .bcdDevice = 0x0100,
    .iManufacturer = 0x01, .iProduct = 0x02, .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};
#endif

uint8_t const * tud_descriptor_device_cb(void) { return (uint8_t const *) &desc_device; }

//--------------------------------------------------------------------+
// HID Report Descriptors
//--------------------------------------------------------------------+
#if CFG_TUD_HID_NINTENDO
uint8_t const desc_hid_report[] =
{
        0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
        0x09, 0x05,        // Usage (Game Pad)
        0xA1, 0x01,        // Collection (Application)
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x01,        //   Logical Maximum (1)
        0x35, 0x00,        //   Physical Minimum (0)
        0x45, 0x01,        //   Physical Maximum (1)
        0x75, 0x01,        //   Report Size (1)
        0x95, 0x10,        //   Report Count (16)
        0x05, 0x09,        //   Usage Page (Button)
        0x19, 0x01,        //   Usage Minimum (0x01)
        0x29, 0x10,        //   Usage Maximum (0x10)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
        0x25, 0x07,        //   Logical Maximum (7)
        0x46, 0x3B, 0x01,  //   Physical Maximum (315)
        0x75, 0x04,        //   Report Size (4)
        0x95, 0x01,        //   Report Count (1)
        0x65, 0x14,        //   Unit (System: English Rotation, Length: Centimeter)
        0x09, 0x39,        //   Usage (Hat switch)
        0x81, 0x42,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
        0x65, 0x00,        //   Unit (None)
        0x95, 0x01,        //   Report Count (1)
        0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x26, 0xFF, 0x00,  //   Logical Maximum (255)
        0x46, 0xFF, 0x00,  //   Physical Maximum (255)
        0x09, 0x30,        //   Usage (X)
        0x09, 0x31,        //   Usage (Y)
        0x09, 0x32,        //   Usage (Z)
        0x09, 0x33,        //   Usage (Rx)
        0x75, 0x08,        //   Report Size (8)
        0x95, 0x04,        //   Report Count (4)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
        0x09, 0x20,        //   Usage (0x20)
        0x95, 0x01,        //   Report Count (1)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x0A, 0x21, 0x26,  //   Usage (0x2621)
        0x95, 0x08,        //   Report Count (8)
        0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
        0xC0,              // End Collection
};
#elif CFG_TUD_HID_SONY
// From GP2040-CE
const uint8_t ds4_report_descriptor[] =
{
        0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
        0x09, 0x05,        // Usage (Game Pad)
        0xA1, 0x01,        // Collection (Application)
        0x85, 0x01,        //   Report ID (1)
        0x09, 0x30,        //   Usage (X)
        0x09, 0x31,        //   Usage (Y)
        0x09, 0x32,        //   Usage (Z)
        0x09, 0x35,        //   Usage (Rz)
        0x15, 0x00,        //   Logical Minimum (0)
        0x26, 0xFF, 0x00,  //   Logical Maximum (255)
        0x75, 0x08,        //   Report Size (8)
        0x95, 0x04,        //   Report Count (4)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x09, 0x39,        //   Usage (Hat switch)
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x07,        //   Logical Maximum (7)
        0x35, 0x00,        //   Physical Minimum (0)
        0x46, 0x3B, 0x01,  //   Physical Maximum (315)
        0x65, 0x14,        //   Unit (System: English Rotation, Length: Centimeter)
        0x75, 0x04,        //   Report Size (4)
        0x95, 0x01,        //   Report Count (1)
        0x81, 0x42,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
        0x65, 0x00,        //   Unit (None)
        0x05, 0x09,        //   Usage Page (Button)
        0x19, 0x01,        //   Usage Minimum (0x01)
        0x29, 0x0E,        //   Usage Maximum (0x0E)
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x01,        //   Logical Maximum (1)
        0x75, 0x01,        //   Report Size (1)
        0x95, 0x0E,        //   Report Count (14)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
        0x09, 0x20,        //   Usage (0x20)
        0x75, 0x06,        //   Report Size (6)
        0x95, 0x01,        //   Report Count (1)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
        0x09, 0x33,        //   Usage (Rx)
        0x09, 0x34,        //   Usage (Ry)
        0x15, 0x00,        //   Logical Minimum (0)
        0x26, 0xFF, 0x00,  //   Logical Maximum (255)
        0x75, 0x08,        //   Report Size (8)
        0x95, 0x02,        //   Report Count (2)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
        0x09, 0x21,        //   Usage (0x21)
        0x95, 0x36,        //   Report Count (54)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x85, 0x05,        //   Report ID (5)
        0x09, 0x22,        //   Usage (0x22)
        0x95, 0x1F,        //   Report Count (31)
        0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
        0x85, 0x03,        //   Report ID (3)
        0x0A, 0x21, 0x27,  //   Usage (0x2721)
        0x95, 0x2F,        //   Report Count (47)
        0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
        0xC0,              // End Collection
};
#else // GENERIC
uint8_t const desc_hid_report[] = { TUD_HID_REPORT_DESC_GAMEPAD(HID_REPORT_ID(1)) };
#endif

uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance)
{
  (void) instance;
#if CFG_TUD_HID_NINTENDO
  return desc_hid_report;
#elif CFG_TUD_HID_SONY
  return ds4_report_descriptor;
#else
  return desc_hid_report;
#endif
}

//--------------------------------------------------------------------+
// Configuration Descriptors
//--------------------------------------------------------------------+
#if CFG_TUD_HID_SONY
#define LSB(n) (n & 255)
#define MSB(n) ((n >> 8) & 255)
#define GAMEPAD_INTERFACE 0
#define GAMEPAD_ENDPOINT 1
#define GAMEPAD_SIZE 64
#define PS4_CONFIG1_DESC_SIZE (9+9+9+7+7)

uint8_t const desc_configuration[] = {
    // configuration descriptor, USB spec 9.6.3, page 264-266, Table 9-10
    9,                                                     // bLength;
    2,                                                     // bDescriptorType;
    LSB(PS4_CONFIG1_DESC_SIZE),    // wTotalLength
    MSB(PS4_CONFIG1_DESC_SIZE),
    1,                                 // bNumInterfaces
    1,                                 // bConfigurationValue
    0,                                 // iConfiguration
    0x80,                          // bmAttributes
    50,                                // bMaxPower
            // interface descriptor, USB spec 9.6.5, page 267-269, Table 9-12
    9,                                             // bLength
    4,                                             // bDescriptorType
    GAMEPAD_INTERFACE,             // bInterfaceNumber
    0,                                             // bAlternateSetting
    2,                                             // bNumEndpoints
    0x03,                                  // bInterfaceClass (0x03 = HID)
    0x00,                                  // bInterfaceSubClass (0x00 = No Boot)
    0x00,                                  // bInterfaceProtocol (0x00 = No Protocol)
    0,                                             // iInterface
            // HID interface descriptor, HID 1.11 spec, section 6.2.1
    9,                                                         // bLength
    0x21,                                              // bDescriptorType
    0x11, 0x01,                                        // bcdHID
    0,                                                         // bCountryCode
    1,                                                         // bNumDescriptors
    0x22,                                              // bDescriptorType
    LSB(sizeof(ds4_report_descriptor)), // wDescriptorLength
    MSB(sizeof(ds4_report_descriptor)),
            // endpoint descriptor, USB spec 9.6.6, page 269-271, Table 9-13
    7,                                                         // bLength
    5,                                                     // bDescriptorType
    GAMEPAD_ENDPOINT | 0x80,       // bEndpointAddress
    0x03,                                          // bmAttributes (0x03=intr)
    GAMEPAD_SIZE, 0,                       // wMaxPacketSize
    1,                                                     // bInterval (1 ms)
    0x07,                          // bLength
    0x05,                          // bDescriptorType (Endpoint)
    0x03,                          // bEndpointAddress (OUT/H2D)
    0x03,                          // bmAttributes (Interrupt)
    0x40, 0x00,                    // wMaxPacketSize 64
    0x01,                          // bInterval 1 (unit depends on device speed)
};
#else
#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
#define EPNUM_HID   0x81
uint8_t const desc_configuration[] = {
  TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
  TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 10)
};
#endif

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) { (void) index; return desc_configuration; }

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+
const char* string_desc_arr [] = {
  (char[]){0x09, 0x04},
#if CFG_TUD_HID_NINTENDO
  "HORI CO.,LTD.", "POKKEN CONTROLLER", "1.0",
#elif CFG_TUD_HID_SONY
  "Open Stick Community", "GP2040-CE (PS4)", "1.0",
#else // GENERIC
  "JulesCorp", "Pico Gamepad", "123456",
#endif
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
    for(uint8_t i=0; i<chr_count; i++) { _desc_str[1+i] = str[i]; }
  }
  _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);
  return _desc_str;
}
