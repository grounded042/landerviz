#pragma once

#ifndef LANDERVIZ_PREVIEW_FLOW_V2
#error "FLOW v2 is preview-only until its behavior is accepted"
#endif

#include <stdint.h>

#include "landerviz/audio_analysis.h"
#include "landerviz/effects.h"
#include "landerviz/protocol.h"

typedef struct {
    float gain;
    uint32_t last_update_ms;
    bool initialized;
    bool signal_seen;
} lv_flow_v2_gain_t;

typedef struct {
    float raw_level;
    float value;
    float raw_centroid_hz;
    float centroid_hz;
    float palette_position;
    float dominance;
    float hue;
    float saturation;
} lv_flow_v2_channel_features_t;

typedef struct {
    lv_flow_v2_channel_features_t channel[LV_AUDIO_CHANNEL_COUNT];
    float gain;
    float onset_low;
    float onset_mid;
    float onset_high;
    float onset_flux[3];
    float onset_threshold[3];
    bool onset_triggered[3];
    uint32_t audio_time_ms;
    bool gate_open;
} lv_flow_v2_features_t;

typedef struct {
    float value;
    float centroid_log2;
    bool centroid_initialized;
} lv_flow_v2_channel_state_t;

typedef struct {
    float mean;
    float deviation;
    uint32_t last_update_ms;
    uint32_t last_onset_ms;
    bool initialized;
    bool onset_seen;
} lv_flow_v2_onset_detector_t;

typedef struct {
    lv_flow_v2_onset_detector_t detector[3];
    float previous_log_level[LV_MEL_BAND_COUNT];
    uint32_t last_shared_onset_ms;
    uint8_t last_shared_onset_mask;
    bool initialized;
    bool shared_onset_seen;
} lv_flow_v2_onset_state_t;

typedef struct {
    lv_flow_v2_gain_t gain;
    lv_flow_v2_onset_state_t onset;
    lv_flow_v2_channel_state_t channel[LV_AUDIO_CHANNEL_COUNT];
    lv_spectral_flow_t ribbon[LV_AUDIO_CHANNEL_COUNT];
    lv_flow_v2_features_t features;
    uint32_t last_update_ms;
    bool initialized;
} lv_flow_v2_t;

typedef struct {
    float hue;
    float saturation;
    float value_scale;
} lv_flow_v2_palette_color_t;

// Update one shared automatic gain from a pre-gain, linear stereo level.
// A closed gate can never increase gain.
float lv_flow_v2_gain_update(lv_flow_v2_gain_t *state, float linear_level,
                             bool gate_open, uint32_t now_ms);

// Return the curated palette color for a mel-spectrum centroid and dominance.
lv_flow_v2_palette_color_t lv_flow_v2_palette(float centroid_hz,
                                              float dominance);

// Expand the centroid range common in mastered music onto the whole palette.
// The mapping remains absolute and ordered; it does not normalize per song.
float lv_flow_v2_palette_position(float centroid_hz);

// Analyze one already-FFT'd stereo frame and append its hue/value packets to
// the two timestamped ribbons. gate_open must be computed before AGC.
void lv_flow_v2_update(
    lv_flow_v2_t *state,
    const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT],
    bool gate_open, uint32_t audio_time_ms);

void lv_flow_v2_set_travel_ms(lv_flow_v2_t *state, uint16_t travel_ms);

// Render the current v2 ribbons onto the physical Moonlander layout.
void lv_flow_v2_render(const lv_flow_v2_t *state, uint32_t now_ms,
                       lv_rgb_t frame[LV_KEY_COUNT]);
