#include <cstring>

#include "tusb.h"

namespace {
constexpr uint8_t kHidInterface = 0;
constexpr uint8_t kInterfaceCount = 1;
constexpr uint8_t kEndpointHid = 0x81;

const tusb_desc_device_t device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0xCAFE,
    .idProduct = 0x4010,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(1)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(2)),
    // Match QMK's 16-bit System Control report rather than TinyUSB's compact
    // three-value template, so the bridge can forward reports byte-for-byte.
    HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),
    HID_USAGE(HID_USAGE_DESKTOP_SYSTEM_CONTROL),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
    HID_REPORT_ID(3)
    HID_USAGE_MIN_N(0x0001, 2),
    HID_USAGE_MAX_N(0x008F, 2),
    HID_LOGICAL_MIN_N(0x0001, 2),
    HID_LOGICAL_MAX_N(0x008F, 2),
    HID_REPORT_COUNT(1),
    HID_REPORT_SIZE(16),
    HID_INPUT(HID_DATA | HID_ARRAY | HID_ABSOLUTE),
    HID_COLLECTION_END,
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(4)),

    // QMK firmware 25 enables 240-key N-key rollover. Report ID 6 contains
    // one modifier byte followed by a 30-byte key bitmap.
    HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),
    HID_USAGE(HID_USAGE_DESKTOP_KEYBOARD),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
    HID_REPORT_ID(6)
    HID_USAGE_PAGE(HID_USAGE_PAGE_KEYBOARD),
    HID_USAGE_MIN(0xE0),
    HID_USAGE_MAX(0xE7),
    HID_LOGICAL_MIN(0),
    HID_LOGICAL_MAX(1),
    HID_REPORT_COUNT(8),
    HID_REPORT_SIZE(1),
    HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),
    HID_USAGE_PAGE(HID_USAGE_PAGE_KEYBOARD),
    HID_USAGE_MIN(0),
    HID_USAGE_MAX(239),
    HID_LOGICAL_MIN(0),
    HID_LOGICAL_MAX(1),
    HID_REPORT_COUNT(240),
    HID_REPORT_SIZE(1),
    HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),
    HID_USAGE_PAGE(HID_USAGE_PAGE_LED),
    HID_USAGE_MIN(1),
    HID_USAGE_MAX(5),
    HID_REPORT_COUNT(5),
    HID_REPORT_SIZE(1),
    HID_OUTPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),
    HID_REPORT_COUNT(1),
    HID_REPORT_SIZE(3),
    HID_OUTPUT(HID_CONSTANT),
    HID_COLLECTION_END,
};

constexpr uint16_t kConfigLength = TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN;
const uint8_t configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, kInterfaceCount, 0, kConfigLength,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500),
    TUD_HID_DESCRIPTOR(kHidInterface, 0, HID_ITF_PROTOCOL_NONE,
                       sizeof(hid_report_descriptor), kEndpointHid, CFG_TUD_HID_EP_BUFSIZE, 1),
};

const char *string_descriptors[] = {
    nullptr,
    "LanderViz",
    "Moonlander RGB Bridge",
    "0001",
};

uint16_t string_buffer[32];
}  // namespace

extern "C" const uint8_t *tud_descriptor_device_cb() {
    return reinterpret_cast<const uint8_t *>(&device_descriptor);
}

extern "C" const uint8_t *tud_hid_descriptor_report_cb(uint8_t) {
    return hid_report_descriptor;
}

extern "C" const uint8_t *tud_descriptor_configuration_cb(uint8_t) {
    return configuration_descriptor;
}

extern "C" const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t) {
    if (index == 0) {
        string_buffer[1] = 0x0409;
        string_buffer[0] = (uint16_t)((TUSB_DESC_STRING << 8) | 4);
        return string_buffer;
    }
    if (index >= sizeof(string_descriptors) / sizeof(string_descriptors[0])) return nullptr;

    const char *source = string_descriptors[index];
    size_t length = std::strlen(source);
    if (length > 31) length = 31;
    for (size_t i = 0; i < length; ++i) string_buffer[1 + i] = source[i];
    string_buffer[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * length + 2));
    return string_buffer;
}
