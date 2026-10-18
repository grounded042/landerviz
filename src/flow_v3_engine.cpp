#include "landerviz/flow_v3_engine.h"

#include <cstring>

#include "landerviz/flow_v3_combined.h"

void lv_flow_v3_engine_init(lv_flow_v3_engine_t *engine) {
    std::memset(engine, 0, sizeof(*engine));
    lv_flow_v3_set_bass_contrast(&engine->bass, true);
    lv_flow_v3_set_travel_ms(&engine->bass,
                             LV_FLOW_V3_DEFAULT_TRAVEL_MS);
    lv_flow_v3_mid_set_travel_ms(&engine->mids,
                                 LV_FLOW_V3_DEFAULT_TRAVEL_MS);
    lv_flow_v3_energy_set_travel_ms(&engine->energy,
                                    LV_FLOW_V3_DEFAULT_TRAVEL_MS);
    lv_flow_v3_attack_set_travel_ms(&engine->attack,
                                    LV_FLOW_V3_DEFAULT_TRAVEL_MS);
    lv_flow_v3_texture_set_travel_ms(&engine->texture,
                                     LV_FLOW_V3_DEFAULT_TRAVEL_MS);
}

void lv_flow_v3_engine_update(
    lv_flow_v3_engine_t *engine,
    const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT],
    bool gate_open, uint32_t audio_time_ms) {
    lv_flow_v3_update(&engine->bass, audio, gate_open, audio_time_ms);
    lv_flow_v3_mid_update(&engine->mids, audio, gate_open,
                          audio_time_ms);
    lv_flow_v3_energy_update(&engine->energy, audio, gate_open,
                             audio_time_ms);
    lv_flow_v3_attack_update(&engine->attack, audio, gate_open,
                             audio_time_ms);
    lv_flow_v3_texture_update(&engine->texture, audio, gate_open,
                              audio_time_ms);
}

void lv_flow_v3_engine_render(const lv_flow_v3_engine_t *engine,
                              uint32_t now_ms,
                              lv_rgb_t frame[LV_KEY_COUNT],
                              lv_flow_v3_service_fn service) {
    lv_flow_v3_combined_energy_attack_texture_render(
        &engine->bass, &engine->mids, &engine->energy,
        &engine->attack, &engine->texture, now_ms, frame, service);
}
