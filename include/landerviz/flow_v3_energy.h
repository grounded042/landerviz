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
    float target;
    float energy;
    uint32_t audio_time_ms;
    bool gate_open;
} lv_flow_v3_energy_features_t;

typedef struct {
    lv_flow_v3_slice_t history[LV_FLOW_V3_HISTORY];
    lv_flow_v3_energy_features_t features;
    float envelope;
    uint32_t last_update_ms;
    uint16_t history_head;
    uint16_t history_count;
    uint16_t travel_ms;
    bool initialized;
} lv_flow_v3_energy_t;

// Return a fixed, phase-safe mono level from the full stereo spectrum.
float lv_flow_v3_energy_level(
    const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT]);

void lv_flow_v3_energy_update(
    lv_flow_v3_energy_t *state,
    const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT],
    bool gate_open, uint32_t audio_time_ms);

void lv_flow_v3_energy_set_travel_ms(lv_flow_v3_energy_t *state,
                                     uint16_t travel_ms);

lv_flow_v3_slice_t lv_flow_v3_energy_sample(
    const lv_flow_v3_energy_t *state, float coordinate, uint32_t now_ms);

void lv_flow_v3_energy_render(const lv_flow_v3_energy_t *state,
                              uint32_t now_ms,
                              lv_rgb_t frame[LV_KEY_COUNT]);
