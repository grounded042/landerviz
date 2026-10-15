#include "landerviz/effects.h"

#include <algorithm>
#include <cmath>

#include "landerviz/audio_analysis.h"

lv_rgb_t lv_hsv_to_rgb(uint8_t hue, uint8_t saturation, uint8_t value) {
    const uint8_t region = hue / 43;
    const uint8_t remainder = (hue - region * 43) * 6;
    const uint8_t p = (value * (255 - saturation)) >> 8;
    const uint8_t q = (value * (255 - ((saturation * remainder) >> 8))) >> 8;
    const uint8_t t =
        (value * (255 - ((saturation * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0: return {value, t, p};
        case 1: return {q, value, p};
        case 2: return {p, value, t};
        case 3: return {p, q, value};
        case 4: return {t, p, value};
        default: return {value, p, q};
    }
}

lv_rgb_t lv_aurora_color(float x, float y, uint32_t now_ms) {
    const float phase = (float)now_ms * 0.00035f;
    const float curtain = 0.5f + 0.5f * std::sin(
        x * 0.62f + phase + std::sin(y * 0.9f - phase * 0.4f));
    const float ribbon = 0.5f + 0.5f * std::sin(
        y * 1.15f - phase * 0.8f + std::cos(x * 0.33f) * 0.9f);
    const float intensity = 0.65f * curtain * curtain + 0.35f * ribbon;
    const uint8_t value = (uint8_t)(28.0f + 92.0f * intensity);

    const float hue_wave = 128.0f + 42.0f * std::sin(x * 0.28f - phase * 0.65f) +
                           24.0f * std::sin(y * 0.8f + phase);
    const uint8_t hue = (uint8_t)std::clamp(hue_wave, 78.0f, 202.0f);
    return lv_hsv_to_rgb(hue, 215, value);
}

lv_rgb_t lv_vu_row_color(float level, int row_from_bottom, int row_count) {
    if (row_count <= 0 || row_from_bottom < 0 || row_from_bottom >= row_count) {
        return {0, 0, 0};
    }

    level = std::clamp(level, 0.0f, 1.0f);
    const float fill = std::clamp(
        level * (float)row_count - (float)row_from_bottom, 0.0f, 1.0f);
    const uint8_t value = (uint8_t)(12.0f + fill * 243.0f);
    const uint8_t hue = row_count == 1
                            ? 85
                            : (uint8_t)(85 - row_from_bottom * 85 / (row_count - 1));
    return lv_hsv_to_rgb(hue, 255, value);
}

float lv_keyboard_vu_level(float linear_level) {
    return lv_meter_level(linear_level) > 0.0f
               ? lv_perceptual_level(linear_level)
               : 0.0f;
}

float lv_vu_smooth_level(float displayed_level, float target_level) {
    displayed_level = std::clamp(displayed_level, 0.0f, 1.0f);
    target_level = std::clamp(target_level, 0.0f, 1.0f);
    const float response = target_level > displayed_level ? 0.32f : 0.07f;
    const float next = displayed_level + (target_level - displayed_level) * response;
    return target_level == 0.0f && next < 0.002f ? 0.0f : next;
}
