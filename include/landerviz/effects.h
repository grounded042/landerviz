#pragma once

#include <stdint.h>

#include "landerviz/protocol.h"

enum {
    LV_SPECTRAL_FLOW_BANDS = 12,
    LV_SPECTRAL_FLOW_COLUMNS = 6,
    LV_SPECTRAL_FLOW_FRAME_MS = 33,
    LV_SPECTRAL_FLOW_HISTORY = 96,
    LV_SPECTRAL_FLOW_TRAVEL_MS = 110,
};

typedef struct {
    float hue;
    float value;
    uint32_t time_ms;
} lv_spectral_flow_sample_t;

typedef struct {
    lv_spectral_flow_sample_t history[LV_SPECTRAL_FLOW_HISTORY];
    float previous_band_level[LV_SPECTRAL_FLOW_BANDS];
    float gain_peak;
    float hue;
    float target_hue;
    float source_value;
    float previous_bass_transient;
    float bass_impulse;
    uint32_t last_frame_ms;
    uint16_t history_head;
    uint16_t history_count;
    uint16_t travel_ms;
    bool initialized;
    bool hue_initialized;
} lv_spectral_flow_t;

lv_rgb_t lv_hsv_to_rgb(uint8_t hue, uint8_t saturation, uint8_t value);

// Let the strongest bass, body, or presence event select a saturated hue,
// then append that continuously evolving color to the ribbon history.
void lv_spectral_flow_update(
    lv_spectral_flow_t *flow,
    const float band_level[LV_SPECTRAL_FLOW_BANDS], float bass_transient,
    uint32_t now_ms);

// Convert a key's outward distance and vertical row into a continuous field
// coordinate. Opposite row phase on each half prevents whole columns from
// changing in lockstep.
float lv_spectral_flow_coordinate(int half, float outward_distance, float row);

// Render one key from a blurred neighborhood of progressively older ribbon
// history. The coordinate comes from lv_spectral_flow_coordinate().
lv_rgb_t lv_spectral_flow_color(const lv_spectral_flow_t *flow,
                                float coordinate, uint32_t now_ms);

// Return the unscaled color for a physical keyboard position in the idle
// aurora. x and y are layout coordinates, not LED indices.
lv_rgb_t lv_aurora_color(float x, float y, uint32_t now_ms);

// Return the unscaled color of one row in a bottom-to-top VU meter.
lv_rgb_t lv_vu_row_color(float level, int row_from_bottom, int row_count);

// Expand audio that has cleared the meter noise gate onto the more sensitive
// keyboard visualization curve.
float lv_keyboard_vu_level(float linear_level);

// Smooth a VU level at the fixed 30 fps render rate. Attack is deliberately
// faster than release.
float lv_vu_smooth_level(float displayed_level, float target_level);
