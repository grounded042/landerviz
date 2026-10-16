#pragma once

#include <cstdint>

#include "landerviz/protocol.h"

enum lv_usb_host_status_flag : uint8_t {
    LV_USB_HOST_STARTED = 1u << 0,
    LV_USB_DEVICE_MOUNTED = 1u << 1,
    LV_USB_KEYBOARD_MOUNTED = 1u << 2,
    LV_USB_RAW_MOUNTED = 1u << 3,
    LV_USB_DEVICE_ATTACHED = 1u << 4,
    LV_USB_DEVICE_FULL_SPEED = 1u << 5,
    LV_USB_ATTACH_EVENT = 1u << 6,
    LV_USB_HOST_ERROR = 1u << 7,
};

struct lv_usb_status_display_t {
    uint8_t stage;
    uint32_t changed_ms;
    bool initialized;
};

void usb_bridge_init();
void usb_bridge_device_task();
void usb_bridge_submit_frame(const lv_rgb_t frame[LV_KEY_COUNT]);
uint32_t usb_bridge_completed_frame_count();
uint8_t usb_bridge_host_status();
uint8_t usb_bridge_host_status_history();
const char* lv_usb_host_status_label(uint8_t status);
const char* lv_usb_status_display_update(lv_usb_status_display_t* display,
                                         uint8_t status_history,
                                         uint32_t now_ms);
