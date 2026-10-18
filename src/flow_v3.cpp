#include "landerviz/flow_v3.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "landerviz/effects.h"
#include "landerviz/moonlander_map.h"

namespace {

constexpr float bass_attack_ms = 28.0f;
constexpr float bass_release_ms = 115.0f;
constexpr float bass_micro_attack_ms = 160.0f;
constexpr float bass_micro_release_ms = 260.0f;
constexpr float bass_full_speed_delta = 0.16f;
constexpr float bass_bed_attack_ms = 900.0f;
constexpr float bass_bed_release_ms = 1800.0f;
constexpr float bass_bed_subtraction = 0.48f;
constexpr float bass_body_minimum = 0.45f;
constexpr float bass_noise_floor = 0.015f;
constexpr float bass_reference_ceiling = 0.75f;
constexpr float bass_curve = 0.82f;
constexpr float bass_lab_hue = 195.0f;
constexpr float bass_lab_saturation = 255.0f;

float response(uint32_t elapsed_ms, float time_constant_ms) {
    return elapsed_ms > 0
               ? 1.0f - std::exp(-(float)elapsed_ms / time_constant_ms)
               : 1.0f;
}

float envelope(float current, float target, uint32_t elapsed_ms,
               float attack_ms, float release_ms) {
    const float time_constant = target > current ? attack_ms : release_ms;
    return current + (target - current) * response(elapsed_ms, time_constant);
}

float bass_body_envelope(float current, float target, uint32_t elapsed_ms,
                         bool contrast_enabled) {
    if (!contrast_enabled) {
        return envelope(current, target, elapsed_ms,
                        bass_attack_ms, bass_release_ms);
    }
    float motion = std::clamp(
        std::fabs(target - current) / bass_full_speed_delta, 0.0f, 1.0f);
    motion = motion * motion * (3.0f - 2.0f * motion);
    const float attack_ms = bass_micro_attack_ms +
                            (bass_attack_ms - bass_micro_attack_ms) * motion;
    const float release_ms = bass_micro_release_ms +
                             (bass_release_ms - bass_micro_release_ms) * motion;
    return envelope(current, target, elapsed_ms, attack_ms, release_ms);
}

float nearest_hue(float hue, float reference) {
    while (hue - reference > 180.0f) hue -= 360.0f;
    while (hue - reference < -180.0f) hue += 360.0f;
    return hue;
}

lv_flow_v3_slice_t blend(const lv_flow_v3_slice_t &first,
                         const lv_flow_v3_slice_t &second, float amount) {
    amount = std::clamp(amount, 0.0f, 1.0f);
    const auto mix = [amount](float a, float b) {
        return a + (b - a) * amount;
    };
    const float second_hue = nearest_hue(second.hue, first.hue);
    return {
        mix(first.hue, second_hue),
        mix(first.saturation, second.saturation),
        mix(first.energy, second.energy),
        mix(first.body, second.body),
        mix(first.attack, second.attack),
        mix(first.texture, second.texture),
        (uint32_t)(mix((float)first.audio_time_ms,
                      (float)second.audio_time_ms) + 0.5f),
    };
}

void set_key(lv_rgb_t frame[LV_KEY_COUNT], int key, lv_rgb_t color) {
    if (key >= 0 && key < LV_KEY_COUNT) frame[key] = color;
}

lv_rgb_t render_sample(const lv_flow_v3_slice_t &sample, float coordinate) {
    const float distance_fade = std::pow(0.975f, std::max(0.0f, coordinate));
    const float presence = std::clamp(sample.body, 0.0f, 1.0f);
    if (presence <= 0.001f) return {};
    const float value = (0.12f + 0.88f * presence) * distance_fade;
    float hue = std::fmod(sample.hue, 360.0f);
    if (hue < 0.0f) hue += 360.0f;
    return lv_hsv_to_rgb(
        (uint8_t)std::lround(hue * 255.0f / 360.0f),
        (uint8_t)std::clamp(std::lround(sample.saturation), 0l, 255l),
        (uint8_t)std::clamp(std::lround(value * 255.0f), 0l, 255l));
}

lv_flow_v3_slice_t blurred_sample(const lv_flow_v3_t *state,
                                  float coordinate, uint32_t now_ms) {
    constexpr float offset = 0.22f;
    const auto before = lv_flow_v3_sample(
        state, std::max(0.0f, coordinate - offset), now_ms);
    const auto center = lv_flow_v3_sample(state, coordinate, now_ms);
    const auto after = lv_flow_v3_sample(state, coordinate + offset, now_ms);
    lv_flow_v3_slice_t result = center;
    result.energy = before.energy * 0.25f + center.energy * 0.50f +
                    after.energy * 0.25f;
    result.body = before.body * 0.25f + center.body * 0.50f +
                  after.body * 0.25f;
    result.attack = 0.0f;
    return result;
}

}  // namespace

float lv_flow_v3_bass_level(
    const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT]) {
    float bass = 0.0f;
    for (int channel = 0; channel < LV_AUDIO_CHANNEL_COUNT; ++channel) {
        const float channel_bass = std::max(
            {audio[channel].band_level[0], audio[channel].band_level[1],
             audio[channel].band_level[2] * 0.85f});
        bass = std::max(bass, channel_bass);
    }
    return bass;
}

void lv_flow_v3_update(
    lv_flow_v3_t *state,
    const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT],
    bool gate_open, uint32_t audio_time_ms) {
    const uint32_t elapsed_ms =
        state->initialized ? audio_time_ms - state->last_update_ms : 0;
    const float raw_bass = gate_open ? lv_flow_v3_bass_level(audio) : 0.0f;
    const float normalized = std::clamp(
        (raw_bass - bass_noise_floor) /
            (bass_reference_ceiling - bass_noise_floor),
        0.0f, 1.0f);
    const float raw_target_body = gate_open
                                      ? std::pow(normalized, bass_curve)
                                      : 0.0f;

    if (!state->initialized) {
        state->slow_bass = raw_target_body;
    } else {
        state->slow_bass = envelope(
            state->slow_bass, raw_target_body, elapsed_ms,
            bass_bed_attack_ms, bass_bed_release_ms);
    }
    const float bed_reduction =
        state->slow_bass * bass_bed_subtraction;
    const float contrasted_body = std::clamp(
        (raw_target_body - bed_reduction) /
            std::max(0.01f, 1.0f - bed_reduction),
        0.0f, 1.0f);
    const float target_body = state->bass_contrast_enabled
                                  ? std::max(raw_target_body *
                                                 bass_body_minimum,
                                             contrasted_body)
                                  : raw_target_body;
    if (!state->initialized) state->fast_bass = target_body;
    else {
        state->fast_bass = bass_body_envelope(
            state->fast_bass, target_body, elapsed_ms,
            state->bass_contrast_enabled);
    }
    const float body = state->fast_bass;
    state->smoothed_hue = bass_lab_hue;
    state->hue_initialized = true;

    state->features = {
        raw_bass, body, 0.0f, state->smoothed_hue,
        audio_time_ms, gate_open,
    };
    state->history[state->history_head] = {
        state->smoothed_hue,
        bass_lab_saturation,
        body,
        body,
        0.0f,
        0.0f,
        audio_time_ms,
    };
    state->history_head =
        (uint16_t)((state->history_head + 1) % LV_FLOW_V3_HISTORY);
    state->history_count =
        std::min<uint16_t>(state->history_count + 1, LV_FLOW_V3_HISTORY);
    state->previous_body = body;
    state->last_update_ms = audio_time_ms;
    state->initialized = true;
}

void lv_flow_v3_set_travel_ms(lv_flow_v3_t *state, uint16_t travel_ms) {
    state->travel_ms = travel_ms > 0
                           ? std::clamp<uint16_t>(travel_ms, 50, 1000)
                           : (uint16_t)LV_FLOW_V3_DEFAULT_TRAVEL_MS;
}

void lv_flow_v3_set_bass_contrast(lv_flow_v3_t *state, bool enabled) {
    state->bass_contrast_enabled = enabled;
}

lv_flow_v3_slice_t lv_flow_v3_sample(const lv_flow_v3_t *state,
                                     float coordinate, uint32_t now_ms) {
    const uint16_t travel_ms = state->travel_ms > 0
                                   ? state->travel_ms
                                   : (uint16_t)LV_FLOW_V3_DEFAULT_TRAVEL_MS;
    return lv_flow_v3_history_sample(
        state->history, state->history_head, state->history_count,
        travel_ms, coordinate, now_ms);
}

lv_flow_v3_slice_t lv_flow_v3_history_sample(
    const lv_flow_v3_slice_t history[LV_FLOW_V3_HISTORY],
    uint16_t history_head, uint16_t history_count, uint16_t travel_ms,
    float coordinate, uint32_t now_ms) {
    if (history_count == 0) return {};
    const float target_time =
        (float)now_ms - std::max(0.0f, coordinate) * travel_ms;
    uint16_t newer_index = (uint16_t)(
        (history_head + LV_FLOW_V3_HISTORY - 1) % LV_FLOW_V3_HISTORY);
    const auto &newest = history[newer_index];
    if (target_time >= newest.audio_time_ms) return newest;

    for (uint16_t offset = 1; offset < history_count; ++offset) {
        const uint16_t older_index = (uint16_t)(
            (history_head + LV_FLOW_V3_HISTORY - 1 - offset) %
            LV_FLOW_V3_HISTORY);
        const auto &older = history[older_index];
        if (target_time >= older.audio_time_ms) {
            const auto &newer = history[newer_index];
            const uint32_t interval = newer.audio_time_ms -
                                      older.audio_time_ms;
            const float amount = interval > 0
                                     ? (target_time - older.audio_time_ms) /
                                           interval
                                     : 1.0f;
            return blend(older, newer, amount);
        }
        newer_index = older_index;
    }
    return {};
}

void lv_flow_v3_render(const lv_flow_v3_t *state, uint32_t now_ms,
                       lv_rgb_t frame[LV_KEY_COUNT]) {
    std::memset(frame, 0, sizeof(lv_rgb_t) * LV_KEY_COUNT);
    for (int half = 0; half < LV_AUDIO_CHANNEL_COUNT; ++half) {
        for (int col = 0; col < LV_SPECTRAL_FLOW_COLUMNS; ++col) {
            const int distance = half == 0
                                     ? LV_SPECTRAL_FLOW_COLUMNS - 1 - col
                                     : col;
            for (int row = 0; row < 5; ++row) {
                const float coordinate = lv_spectral_flow_coordinate(
                    half, (float)distance, (float)row);
                set_key(frame, lv_key_at(half, col, row),
                        render_sample(blurred_sample(state, coordinate,
                                                    now_ms),
                                      coordinate));
            }
        }
        for (int key = 0; key < 3; ++key) {
            const float coordinate = lv_spectral_flow_coordinate(
                half, 0.0f, (float)(key + 1));
            set_key(frame, lv_utility_key_at(half, key),
                    render_sample(blurred_sample(state, coordinate,
                                                now_ms),
                                  coordinate));
        }
        constexpr float thumb_distance[4] = {1.25f, 0.85f, 0.45f, 0.0f};
        for (int key = 0; key < 4; ++key) {
            const float coordinate = lv_spectral_flow_coordinate(
                half, thumb_distance[key], 3.2f + key * 0.2f);
            set_key(frame, lv_thumb_key_at(half, key),
                    render_sample(blurred_sample(state, coordinate,
                                                now_ms),
                                  coordinate));
        }
    }
}
