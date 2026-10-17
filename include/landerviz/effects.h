#pragma once

#include <stdint.h>

#include "landerviz/protocol.h"

enum {
    LV_SPECTRAL_FLOW_BANDS = 12,
    LV_SPECTRAL_COLOR_LANES = 3,
    LV_SPECTRAL_FLOW_COLUMNS = 6,
    LV_SPECTRAL_FLOW_FRAME_MS = 33,
    LV_SPECTRAL_FLOW_HISTORY = 96,
    LV_SPECTRAL_FLOW_TRAVEL_MS = 110,
    LV_SPECTRAL_KICK_HOLD_MS = 120,
};

typedef struct {
    bool triggered;
    float strength;
} lv_kick_event_t;

typedef struct {
    float fast_envelope;
    float slow_envelope;
    float previous_level;
    uint32_t last_update_ms;
    uint32_t last_kick_ms;
    bool initialized;
    bool kick_seen;
} lv_kick_detector_t;

typedef struct {
    float hue;
    float value;
    uint32_t time_ms;
} lv_spectral_flow_sample_t;

typedef struct {
    lv_spectral_flow_sample_t history[LV_SPECTRAL_FLOW_HISTORY];
    float lane_peak[LV_SPECTRAL_COLOR_LANES];
    float lane_level[LV_SPECTRAL_COLOR_LANES];
    float base_hue;
    float base_value;
    float source_hue;
    float source_value;
    float accent_hue;
    float accent_value;
    uint32_t last_frame_ms;
    uint32_t accent_until_ms;
    uint32_t accent_release_until_ms;
    uint16_t history_head;
    uint16_t history_count;
    uint16_t travel_ms;
    bool initialized;
    bool hue_initialized;
} lv_spectral_flow_t;

lv_rgb_t lv_hsv_to_rgb(uint8_t hue, uint8_t saturation, uint8_t value);

// Detect a kick from a sharp rise in the shared low-frequency envelope. The
// fast and slow envelopes distinguish a transient from a sustained bass note.
lv_kick_event_t lv_kick_detector_update(lv_kick_detector_t *detector,
                                        float bass_level,
                                        uint32_t now_ms);

// Map the complete mel spectrum to one saturated hue/value packet, then append
// that packet to the outward-moving ribbon history.
void lv_spectral_flow_update(
    lv_spectral_flow_t *flow,
    const float band_level[LV_SPECTRAL_FLOW_BANDS], float bass_transient,
    uint32_t now_ms, const lv_kick_event_t *kick = nullptr);

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
