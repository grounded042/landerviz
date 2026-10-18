#pragma once

#include <stdint.h>

#include "landerviz/protocol.h"

// Look up the most vivid in-gamut sRGB color for an OKLCH hue and perceptual
// lightness. The generated table keeps trigonometry and gamut searches out of
// the RP2040 render loop; interpolation is shared by firmware and preview.
lv_rgb_t lv_flow_v3_vivid_color(float hue_degrees, float lightness);

// Recover Oklab perceptual lightness from an 8-bit sRGB color. This reference
// conversion is used by tests and diagnostics, not the firmware render loop.
float lv_flow_v3_color_lightness(lv_rgb_t color);

// Return the precomputed Oklab lightness of the accepted cyan Bass renderer at
// an 8-bit HSV value. This avoids an RGB-to-Oklab conversion for every key.
float lv_flow_v3_bass_reference_lightness(uint8_t value);
