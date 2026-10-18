#pragma once

#include <stdint.h>

#include "landerviz/audio_analysis.h"
#include "landerviz/flow_v3.h"
#include "landerviz/flow_v3_attack.h"
#include "landerviz/flow_v3_combined.h"
#include "landerviz/flow_v3_energy.h"
#include "landerviz/flow_v3_mid.h"
#include "landerviz/flow_v3_texture.h"
#include "landerviz/protocol.h"

typedef struct {
    lv_flow_v3_t bass;
    lv_flow_v3_mid_t mids;
    lv_flow_v3_energy_t energy;
    lv_flow_v3_attack_t attack;
    lv_flow_v3_texture_t texture;
} lv_flow_v3_engine_t;

// Initialize the accepted FLOW v3 configuration. The caller must provide a
// zeroed engine or call this before any update/render operation.
void lv_flow_v3_engine_init(lv_flow_v3_engine_t *engine);

// Extract all accepted slice properties from one analyzed stereo window.
void lv_flow_v3_engine_update(
    lv_flow_v3_engine_t *engine,
    const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT],
    bool gate_open, uint32_t audio_time_ms);

// Render the accepted Bass + Mid + Energy + Attack + Texture compositor.
void lv_flow_v3_engine_render(const lv_flow_v3_engine_t *engine,
                              uint32_t now_ms,
                              lv_rgb_t frame[LV_KEY_COUNT],
                              lv_flow_v3_service_fn service = nullptr);
