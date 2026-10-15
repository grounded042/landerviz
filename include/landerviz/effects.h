#pragma once

#include <stdint.h>

#include "landerviz/protocol.h"

lv_rgb_t lv_hsv_to_rgb(uint8_t hue, uint8_t saturation, uint8_t value);

// Return the unscaled color for a physical keyboard position in the idle
// aurora. x and y are layout coordinates, not LED indices.
lv_rgb_t lv_aurora_color(float x, float y, uint32_t now_ms);

// Return the unscaled color of one row in a bottom-to-top VU meter.
lv_rgb_t lv_vu_row_color(float level, int row_from_bottom, int row_count);

// Expand audio that has cleared the meter noise gate onto the more sensitive
// keyboard visualization curve.
float lv_keyboard_vu_level(float linear_level);

// Smooth a VU level at the fixed 60 fps render rate. Attack is deliberately
// faster than release.
float lv_vu_smooth_level(float displayed_level, float target_level);
