#include "landerviz/usb_bridge.h"

namespace {
constexpr uint32_t kStatusDwellMs = 1000;
constexpr uint8_t kBootStage = 0;
constexpr uint8_t kWaitStage = 1;
constexpr uint8_t kAttachStage = 2;
constexpr uint8_t kSpeedStage = 3;
constexpr uint8_t kDeviceStage = 4;
constexpr uint8_t kHidStage = 5;
constexpr uint8_t kReadyStage = 6;

bool stage_was_reached(uint8_t stage, uint8_t status) {
    switch (stage) {
        case kWaitStage:
            return status & LV_USB_HOST_STARTED;
        case kAttachStage:
            return status & LV_USB_ATTACH_EVENT;
        case kSpeedStage:
            return status & LV_USB_DEVICE_ATTACHED;
        case kDeviceStage:
            return status & LV_USB_DEVICE_MOUNTED;
        case kHidStage:
            return status & (LV_USB_KEYBOARD_MOUNTED | LV_USB_RAW_MOUNTED);
        case kReadyStage:
            return (status & (LV_USB_KEYBOARD_MOUNTED | LV_USB_RAW_MOUNTED)) ==
                   (LV_USB_KEYBOARD_MOUNTED | LV_USB_RAW_MOUNTED);
        default:
            return stage == kBootStage;
    }
}

uint8_t highest_reached_stage(uint8_t status) {
    for (int stage = kReadyStage; stage >= kWaitStage; --stage) {
        if (stage_was_reached((uint8_t)stage, status)) return (uint8_t)stage;
    }
    return kBootStage;
}

const char* display_stage_label(uint8_t stage, uint8_t status) {
    switch (stage) {
        case kWaitStage:
            return "WAIT";
        case kAttachStage:
            return "ATT";
        case kSpeedStage:
            return status & LV_USB_DEVICE_FULL_SPEED ? "FS" : "LS";
        case kDeviceStage:
            return "DEV";
        case kHidStage:
            return status & LV_USB_KEYBOARD_MOUNTED ? "KBD" : "RAW";
        case kReadyStage:
            return "OK";
        default:
            return "BOOT";
    }
}
}  // namespace

const char* lv_usb_host_status_label(uint8_t status) {
    if (status & LV_USB_HOST_ERROR) return "ERR";
    if ((status & (LV_USB_KEYBOARD_MOUNTED | LV_USB_RAW_MOUNTED)) ==
        (LV_USB_KEYBOARD_MOUNTED | LV_USB_RAW_MOUNTED)) {
        return "OK";
    }
    if (status & LV_USB_RAW_MOUNTED) return "RAW";
    if (status & LV_USB_KEYBOARD_MOUNTED) return "KBD";
    if (status & LV_USB_DEVICE_MOUNTED) return "DEV";
    if (status & LV_USB_DEVICE_FULL_SPEED) return "FS";
    if (status & LV_USB_DEVICE_ATTACHED) return "LS";
    if (status & LV_USB_ATTACH_EVENT) return "ATT";
    if (status & LV_USB_HOST_STARTED) return "WAIT";
    return "BOOT";
}

const char* lv_usb_status_display_update(lv_usb_status_display_t* display,
                                         uint8_t status_history,
                                         uint32_t now_ms) {
    if (status_history & LV_USB_HOST_ERROR) return "ERR";

    if (!display->initialized) {
        display->initialized = true;
        display->stage = stage_was_reached(kWaitStage, status_history)
                             ? kWaitStage
                             : kBootStage;
        display->changed_ms = now_ms;
        return display_stage_label(display->stage, status_history);
    }

    const uint8_t highest_stage = highest_reached_stage(status_history);
    if (highest_stage < display->stage) {
        display->stage = highest_stage;
        display->changed_ms = now_ms;
        return display_stage_label(display->stage, status_history);
    }

    if (now_ms - display->changed_ms >= kStatusDwellMs) {
        for (uint8_t stage = display->stage + 1; stage <= kReadyStage; ++stage) {
            if (stage_was_reached(stage, status_history)) {
                display->stage = stage;
                display->changed_ms = now_ms;
                break;
            }
        }
    }

    return display_stage_label(display->stage, status_history);
}
