#include "landerviz/flow_v3_texture.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "landerviz/effects.h"
#include "landerviz/flow_v3_mid.h"
#include "landerviz/moonlander_map.h"

namespace {

constexpr int texture_first_band = 8;
constexpr float texture_share_scale = 1.25f;
constexpr float texture_attack_ms = 70.0f;
constexpr float texture_release_ms = 220.0f;
constexpr float texture_lab_hue = 330.0f;
constexpr float texture_lab_hue_span = 240.0f;
constexpr float texture_distance_fade = 0.975f;

float response(uint32_t elapsed_ms, float time_constant_ms) {
    return elapsed_ms > 0
               ? 1.0f - std::exp(-(float)elapsed_ms / time_constant_ms)
               : 1.0f;
}

float envelope(float current, float target, uint32_t elapsed_ms,
               float attack_ms, float release_ms) {
    const float time_constant = target > current ? attack_ms : release_ms;
    return current + (target - current) *
                         response(elapsed_ms, time_constant);
}

void set_key(lv_rgb_t frame[LV_KEY_COUNT], int key, lv_rgb_t color) {
    if (key >= 0 && key < LV_KEY_COUNT) frame[key] = color;
}

}  // namespace

float lv_flow_v3_texture_level(
    const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT]) {
    float total_power = 0.0f;
    float high_power = 0.0f;
    for (int band = 0; band < LV_MEL_BAND_COUNT; ++band) {
        const float level = std::max(audio[0].mel_level[band],
                                     audio[1].mel_level[band]);
        const float power = level * level;
        total_power += power;
        if (band >= texture_first_band) high_power += power;
    }
    if (total_power <= 0.000001f) return 0.0f;
    return std::clamp(high_power / total_power, 0.0f, 1.0f);
}

void lv_flow_v3_texture_update(
    lv_flow_v3_texture_t *state,
    const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT],
    bool gate_open, uint32_t audio_time_ms) {
    const uint32_t elapsed_ms =
        state->initialized ? audio_time_ms - state->last_update_ms : 0;
    const float raw_share = gate_open ? lv_flow_v3_texture_level(audio) : 0.0f;
    const float target = std::clamp(
        std::sqrt(raw_share) * texture_share_scale, 0.0f, 1.0f);
    if (!state->initialized) {
        state->texture = target;
        state->presence = gate_open ? 1.0f : 0.0f;
    } else {
        state->texture = envelope(
            state->texture, target, elapsed_ms,
            texture_attack_ms, texture_release_ms);
        state->presence = envelope(
            state->presence, gate_open ? 1.0f : 0.0f, elapsed_ms,
            texture_attack_ms, texture_release_ms);
    }
    state->features = {
        raw_share,
        target,
        state->texture,
        state->presence,
        audio_time_ms,
        gate_open,
    };
    state->history[state->history_head] = {
        texture_lab_hue,
        255.0f,
        state->presence,
        state->presence,
        0.0f,
        state->texture,
        audio_time_ms,
    };
    state->history_head =
        (uint16_t)((state->history_head + 1) % LV_FLOW_V3_HISTORY);
    state->history_count =
        std::min<uint16_t>(state->history_count + 1, LV_FLOW_V3_HISTORY);
    state->last_update_ms = audio_time_ms;
    state->initialized = true;
}

void lv_flow_v3_texture_set_travel_ms(lv_flow_v3_texture_t *state,
                                      uint16_t travel_ms) {
    state->travel_ms = travel_ms > 0
                           ? std::clamp<uint16_t>(travel_ms, 50, 1000)
                           : (uint16_t)LV_FLOW_V3_DEFAULT_TRAVEL_MS;
}

lv_flow_v3_slice_t lv_flow_v3_texture_sample(
    const lv_flow_v3_texture_t *state, float coordinate,
    uint32_t now_ms) {
    const uint16_t travel_ms = state->travel_ms > 0
                                   ? state->travel_ms
                                   : (uint16_t)LV_FLOW_V3_DEFAULT_TRAVEL_MS;
    return lv_flow_v3_history_sample(
        state->history, state->history_head, state->history_count,
        travel_ms, coordinate, now_ms);
}

lv_rgb_t lv_flow_v3_texture_color(lv_flow_v3_slice_t sample,
                                  float coordinate) {
    const float body = std::clamp(sample.body, 0.0f, 1.0f);
    if (body <= 0.001f) return {};
    const float texture = std::clamp(sample.texture, 0.0f, 1.0f);
    const float hue = texture_lab_hue + texture_lab_hue_span * texture;
    const float distance_fade = std::pow(
        texture_distance_fade, std::max(0.0f, coordinate));
    const float lightness = (0.16f + 0.46f * body) * distance_fade;
    return lv_flow_v3_vivid_color(hue, lightness);
}

void lv_flow_v3_texture_render(const lv_flow_v3_texture_t *state,
                               uint32_t now_ms,
                               lv_rgb_t frame[LV_KEY_COUNT]) {
    std::memset(frame, 0, sizeof(lv_rgb_t) * LV_KEY_COUNT);
    for (int half = 0; half < LV_AUDIO_CHANNEL_COUNT; ++half) {
        for (int col = 0; col < LV_SPECTRAL_FLOW_COLUMNS; ++col) {
            const int distance = half == 0
                                     ? LV_SPECTRAL_FLOW_COLUMNS - 1 - col
                                     : col;
            const float coordinate = (float)distance;
            const auto color = lv_flow_v3_texture_color(
                lv_flow_v3_texture_sample(state, coordinate, now_ms),
                coordinate);
            for (int row = 0; row < 5; ++row) {
                set_key(frame, lv_key_at(half, col, row),
                        color);
            }
        }
        for (int key = 0; key < 3; ++key) {
            const float coordinate = 0.0f;
            set_key(frame, lv_utility_key_at(half, key),
                    lv_flow_v3_texture_color(
                        lv_flow_v3_texture_sample(
                            state, coordinate, now_ms),
                        coordinate));
        }
        constexpr float thumb_distance[4] = {1.25f, 0.85f, 0.45f, 0.0f};
        for (int key = 0; key < 4; ++key) {
            const float coordinate = thumb_distance[key];
            set_key(frame, lv_thumb_key_at(half, key),
                    lv_flow_v3_texture_color(
                        lv_flow_v3_texture_sample(
                            state, coordinate, now_ms),
                        coordinate));
        }
    }
}
