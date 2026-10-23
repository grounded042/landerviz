#pragma once

#include <cstdint>

#include "landerviz/protocol.h"

constexpr uint8_t LV_COLOR_SCHEME_COUNT = 3;

const char *lv_color_scheme_name(uint8_t scheme);

// Rotate RGB channels in 120-degree steps. This preserves the maximum channel,
// saturation, and total LED load while moving the complete visualization to a
// different vivid part of the color wheel.
lv_rgb_t lv_color_scheme_apply(lv_rgb_t color, uint8_t scheme);
void lv_color_scheme_apply_frame(lv_rgb_t frame[LV_KEY_COUNT],
                                 uint8_t scheme);
