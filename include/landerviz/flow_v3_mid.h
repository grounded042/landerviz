#pragma once

#ifndef LANDERVIZ_PREVIEW_FLOW_V3
#error "FLOW v3 is preview-only until its behavior is accepted"
#endif

#include <stdint.h>

#include "landerviz/audio_analysis.h"
#include "landerviz/flow_v3.h"
#include "landerviz/protocol.h"

typedef struct {
    float raw_level;
    float centroid_hz;
    float contrast_centroid_hz;
    float contrast_strength;
    float base_hue;
    float target_hue;
    float hue;
    float presence;
    float lightness;
    uint32_t audio_time_ms;
    bool gate_open;
} lv_flow_v3_mid_features_t;

typedef struct {
    lv_flow_v3_slice_t history[LV_FLOW_V3_HISTORY];
    lv_flow_v3_mid_features_t features;
    float spectral_baseline[LV_MEL_BAND_COUNT];
    float smoothed_hue;
    float presence;
    uint32_t last_update_ms;
    uint16_t history_head;
    uint16_t history_count;
    uint16_t travel_ms;
    bool initialized;
    bool hue_initialized;
    bool baseline_initialized;
} lv_flow_v3_mid_t;

void lv_flow_v3_mid_update(
    lv_flow_v3_mid_t *state,
    const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT],
    bool gate_open, uint32_t audio_time_ms);

void lv_flow_v3_mid_set_travel_ms(lv_flow_v3_mid_t *state,
                                   uint16_t travel_ms);

lv_flow_v3_slice_t lv_flow_v3_mid_sample(
    const lv_flow_v3_mid_t *state, float coordinate, uint32_t now_ms);

void lv_flow_v3_mid_render(const lv_flow_v3_mid_t *state, uint32_t now_ms,
                           lv_rgb_t frame[LV_KEY_COUNT]);

// Convert an OKLCH hue and perceptual lightness to the most vivid in-gamut
// sRGB color. OKLCH is Oklab lightness, chroma, and hue; sRGB is the standard
// red/green/blue color space used by the preview and keyboard protocol.
lv_rgb_t lv_flow_v3_vivid_color(float hue_degrees, float lightness);

// Recover Oklab perceptual lightness from an 8-bit sRGB color for regression
// tests and palette diagnostics.
float lv_flow_v3_color_lightness(lv_rgb_t color);
