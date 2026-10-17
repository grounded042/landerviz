#pragma once

#ifndef LANDERVIZ_PREVIEW_FLOW_V3
#error "FLOW v3 is preview-only until its behavior is accepted"
#endif

#include <stdint.h>

#include "landerviz/audio_analysis.h"
#include "landerviz/flow_v3.h"
#include "landerviz/protocol.h"

typedef struct {
    float raw_attack;
    float attack;
    uint32_t audio_time_ms;
    bool gate_open;
} lv_flow_v3_attack_features_t;

typedef struct {
    lv_flow_v3_slice_t history[LV_FLOW_V3_HISTORY];
    lv_flow_v3_attack_features_t features;
    float fast_level[LV_MEL_BAND_COUNT];
    float slow_level[LV_MEL_BAND_COUNT];
    uint32_t last_update_ms;
    uint16_t history_head;
    uint16_t history_count;
    uint16_t travel_ms;
    bool initialized;
} lv_flow_v3_attack_t;

void lv_flow_v3_attack_update(
    lv_flow_v3_attack_t *state,
    const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT],
    bool gate_open, uint32_t audio_time_ms);

void lv_flow_v3_attack_set_travel_ms(lv_flow_v3_attack_t *state,
                                     uint16_t travel_ms);

lv_flow_v3_slice_t lv_flow_v3_attack_sample(
    const lv_flow_v3_attack_t *state, float coordinate, uint32_t now_ms);

void lv_flow_v3_attack_render(const lv_flow_v3_attack_t *state,
                              uint32_t now_ms,
                              lv_rgb_t frame[LV_KEY_COUNT]);
