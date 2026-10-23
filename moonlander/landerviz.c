#include QMK_KEYBOARD_H

#include "landerviz.h"
#include "protocol.h"
#include "raw_hid.h"

#ifndef LANDERVIZ_BRIGHTNESS_CAP
#    define LANDERVIZ_BRIGHTNESS_CAP 175
#endif
#define LANDERVIZ_TIMEOUT_MS 500

static lv_decoder_t decoder;
static uint32_t last_complete_frame_ms;

static lv_rgb_t enforce_brightness_cap(lv_rgb_t color) {
    uint8_t peak = color.r;
    if (color.g > peak) peak = color.g;
    if (color.b > peak) peak = color.b;
    if (peak <= LANDERVIZ_BRIGHTNESS_CAP) return color;

    color.r = (uint8_t)(((uint16_t)color.r * LANDERVIZ_BRIGHTNESS_CAP) / peak);
    color.g = (uint8_t)(((uint16_t)color.g * LANDERVIZ_BRIGHTNESS_CAP) / peak);
    color.b = (uint8_t)(((uint16_t)color.b * LANDERVIZ_BRIGHTNESS_CAP) / peak);
    return color;
}

void raw_hid_receive(uint8_t *data, uint8_t length) {
    if (length != LV_PACKET_SIZE) return;

    const lv_packet_result_t result = lv_decode_packet(&decoder, data);
    if (result == LV_FRAME_READY) {
        last_complete_frame_ms = timer_read32();
    }
}

bool landerviz_render(void) {
    if (!decoder.active_valid) return false;
    if (timer_elapsed32(last_complete_frame_ms) > LANDERVIZ_TIMEOUT_MS) {
        decoder.active_valid = false;
        return false;
    }

    for (uint8_t led = 0; led < LV_KEY_COUNT; ++led) {
        const lv_rgb_t color = enforce_brightness_cap(decoder.active[led]);
        rgb_matrix_set_color(led, color.r, color.g, color.b);
    }
    return true;
}
