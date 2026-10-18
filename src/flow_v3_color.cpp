#include "landerviz/flow_v3_color.h"

#include <algorithm>
#include <cmath>

namespace {

#include "flow_v3_color_lut.inc"

float srgb_to_linear(float value) {
    return value <= 0.04045f
               ? value / 12.92f
               : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

uint8_t interpolate_channel(uint8_t first, uint8_t second, float amount) {
    const float value = first + ((float)second - first) * amount;
    return (uint8_t)std::clamp(value + 0.5f, 0.0f, 255.0f);
}

lv_rgb_t interpolate_color(lv_rgb_t first, lv_rgb_t second, float amount) {
    return {
        interpolate_channel(first.r, second.r, amount),
        interpolate_channel(first.g, second.g, amount),
        interpolate_channel(first.b, second.b, amount),
    };
}

lv_rgb_t table_color(int lightness, int hue) {
    return {
        kFlowV3VividLut[lightness][hue][0],
        kFlowV3VividLut[lightness][hue][1],
        kFlowV3VividLut[lightness][hue][2],
    };
}

}  // namespace

lv_rgb_t lv_flow_v3_vivid_color(float hue_degrees, float lightness) {
    lightness = std::clamp(lightness, 0.0f, 1.0f);
    if (lightness <= 0.001f) return {};

    while (hue_degrees >= 360.0f) hue_degrees -= 360.0f;
    while (hue_degrees < 0.0f) hue_degrees += 360.0f;
    const float hue_position =
        hue_degrees * (float)kFlowV3HueSteps / 360.0f;
    const int hue_first = (int)hue_position;
    const int hue_second = (hue_first + 1) % kFlowV3HueSteps;
    const float hue_blend = hue_position - hue_first;

    const float lightness_position =
        lightness * (float)kFlowV3LightnessSteps;
    const int lightness_first = std::min(
        (int)lightness_position, kFlowV3LightnessSteps);
    const int lightness_second = std::min(
        lightness_first + 1, kFlowV3LightnessSteps);
    const float lightness_blend = lightness_position - lightness_first;

    const auto low = interpolate_color(
        table_color(lightness_first, hue_first),
        table_color(lightness_first, hue_second), hue_blend);
    const auto high = interpolate_color(
        table_color(lightness_second, hue_first),
        table_color(lightness_second, hue_second), hue_blend);
    return interpolate_color(low, high, lightness_blend);
}

float lv_flow_v3_color_lightness(lv_rgb_t color) {
    const float red = srgb_to_linear((float)color.r / 255.0f);
    const float green = srgb_to_linear((float)color.g / 255.0f);
    const float blue = srgb_to_linear((float)color.b / 255.0f);
    const float l_value = std::cbrt(
        0.4122214708f * red + 0.5363325363f * green +
        0.0514459929f * blue);
    const float m_value = std::cbrt(
        0.2119034982f * red + 0.6806995451f * green +
        0.1073969566f * blue);
    const float s_value = std::cbrt(
        0.0883024619f * red + 0.2817188376f * green +
        0.6299787005f * blue);
    return 0.2104542553f * l_value + 0.7936177850f * m_value -
           0.0040720468f * s_value;
}

float lv_flow_v3_bass_reference_lightness(uint8_t value) {
    return kFlowV3BassLightness[value];
}
