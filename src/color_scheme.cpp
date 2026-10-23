#include "landerviz/color_scheme.h"

const char *lv_color_scheme_name(uint8_t scheme) {
    static constexpr const char *names[LV_COLOR_SCHEME_COUNT] = {
        "NATIVE", "ROTATE 1", "ROTATE 2"};
    return names[scheme % LV_COLOR_SCHEME_COUNT];
}

lv_rgb_t lv_color_scheme_apply(lv_rgb_t color, uint8_t scheme) {
    switch (scheme % LV_COLOR_SCHEME_COUNT) {
        case 1:
            return {color.b, color.r, color.g};
        case 2:
            return {color.g, color.b, color.r};
        default:
            return color;
    }
}

void lv_color_scheme_apply_frame(lv_rgb_t frame[LV_KEY_COUNT],
                                 uint8_t scheme) {
    if (scheme % LV_COLOR_SCHEME_COUNT == 0) return;
    for (int key = 0; key < LV_KEY_COUNT; ++key) {
        frame[key] = lv_color_scheme_apply(frame[key], scheme);
    }
}
