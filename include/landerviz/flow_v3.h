#pragma once

#ifndef LANDERVIZ_PREVIEW_FLOW_V3
#error "FLOW v3 is preview-only until its behavior is accepted"
#endif

#include <stdint.h>

#include "landerviz/audio_analysis.h"
#include "landerviz/protocol.h"

enum {
    LV_FLOW_V3_HISTORY = 128,
    LV_FLOW_V3_DEFAULT_TRAVEL_MS = 110,
};

typedef struct {
    float hue;
    float saturation;
    float energy;
    float body;
    float attack;
    float texture;
    uint32_t audio_time_ms;
} lv_flow_v3_slice_t;

typedef struct {
    float raw_bass;
    float body;
    float attack;
    float hue;
    uint32_t audio_time_ms;
    bool gate_open;
} lv_flow_v3_features_t;

typedef struct {
    lv_flow_v3_slice_t history[LV_FLOW_V3_HISTORY];
    lv_flow_v3_features_t features;
    float fast_bass;
    float slow_bass;
    float previous_body;
    float smoothed_hue;
    uint32_t last_update_ms;
    uint16_t history_head;
    uint16_t history_count;
    uint16_t travel_ms;
    bool bass_contrast_enabled;
    bool initialized;
    bool hue_initialized;
} lv_flow_v3_t;

// Merge the three lowest FFT regions (roughly 61-244 Hz) into one mono bass
// level. A maximum merge preserves panned hits without allowing opposite
// waveform phase to cancel.
float lv_flow_v3_bass_level(
    const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT]);

// Extract one cohesive timestamped music slice and append it to the ribbon.
void lv_flow_v3_update(
    lv_flow_v3_t *state,
    const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT],
    bool gate_open, uint32_t audio_time_ms);

void lv_flow_v3_set_travel_ms(lv_flow_v3_t *state, uint16_t travel_ms);

// Enable Bass Lab B's bounded slow-bed subtraction. Lab A is the default and
// remains bit-for-bit unchanged when this is false.
void lv_flow_v3_set_bass_contrast(lv_flow_v3_t *state, bool enabled);

// Sample the continuous ribbon at an outward keyboard coordinate. Adjacent
// audio slices are interpolated rather than selected as discrete key states.
lv_flow_v3_slice_t lv_flow_v3_sample(const lv_flow_v3_t *state,
                                     float coordinate, uint32_t now_ms);

void lv_flow_v3_render(const lv_flow_v3_t *state, uint32_t now_ms,
                       lv_rgb_t frame[LV_KEY_COUNT]);
