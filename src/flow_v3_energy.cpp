#include "landerviz/flow_v3_energy.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "landerviz/effects.h"
#include "landerviz/moonlander_map.h"

namespace {

constexpr float energy_attack_ms = 140.0f;
constexpr float energy_release_ms = 520.0f;
constexpr float energy_lab_hue = 195.0f;
constexpr float energy_distance_fade = 0.975f;

float response(uint32_t elapsed_ms, float time_constant_ms) {
    return elapsed_ms > 0
               ? 1.0f - std::exp(-(float)elapsed_ms / time_constant_ms)
               : 1.0f;
}

lv_flow_v3_slice_t blend(const lv_flow_v3_slice_t &first,
                         const lv_flow_v3_slice_t &second, float amount) {
    amount = std::clamp(amount, 0.0f, 1.0f);
    const auto mix = [amount](float a, float b) {
        return a + (b - a) * amount;
    };
    return {
        energy_lab_hue,
        255.0f,
        mix(first.energy, second.energy),
        mix(first.body, second.body),
        0.0f,
        0.0f,
        (uint32_t)std::lround(mix((float)first.audio_time_ms,
                                 (float)second.audio_time_ms)),
    };
}

lv_flow_v3_slice_t blurred_sample(const lv_flow_v3_energy_t *state,
                                  float coordinate, uint32_t now_ms) {
    constexpr float offset = 0.22f;
    const auto before = lv_flow_v3_energy_sample(
        state, std::max(0.0f, coordinate - offset), now_ms);
    const auto center = lv_flow_v3_energy_sample(state, coordinate, now_ms);
    const auto after = lv_flow_v3_energy_sample(
        state, coordinate + offset, now_ms);
    lv_flow_v3_slice_t result = center;
    result.energy = before.energy * 0.25f + center.energy * 0.50f +
                    after.energy * 0.25f;
    result.body = result.energy;
    return result;
}

lv_rgb_t render_sample(const lv_flow_v3_slice_t &sample, float coordinate) {
    const float energy = std::clamp(sample.energy, 0.0f, 1.0f);
    if (energy <= 0.001f) return {};
    const float distance_fade = std::pow(
        energy_distance_fade, std::max(0.0f, coordinate));
    const float value = (0.12f + 0.88f * energy) * distance_fade;
    return lv_hsv_to_rgb(
        (uint8_t)std::lround(energy_lab_hue * 255.0f / 360.0f), 255,
        (uint8_t)std::clamp(std::lround(value * 255.0f), 0l, 255l));
}

void set_key(lv_rgb_t frame[LV_KEY_COUNT], int key, lv_rgb_t color) {
    if (key >= 0 && key < LV_KEY_COUNT) frame[key] = color;
}

}  // namespace

float lv_flow_v3_energy_level(
    const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT]) {
    float level = 0.0f;
    for (int channel = 0; channel < LV_AUDIO_CHANNEL_COUNT; ++channel) {
        level = std::max(
            level, audio[channel].rms_level * 1.41421356f);
    }
    return level;
}

void lv_flow_v3_energy_update(
    lv_flow_v3_energy_t *state,
    const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT],
    bool gate_open, uint32_t audio_time_ms) {
    const uint32_t elapsed_ms =
        state->initialized ? audio_time_ms - state->last_update_ms : 0;
    const float raw_level = lv_flow_v3_energy_level(audio);
    const float target = gate_open ? lv_meter_level(raw_level) : 0.0f;
    if (!state->initialized) {
        state->envelope = target;
    } else {
        const float time_constant = target > state->envelope
                                        ? energy_attack_ms
                                        : energy_release_ms;
        state->envelope +=
            (target - state->envelope) *
            response(elapsed_ms, time_constant);
    }
    const float energy = state->envelope;
    state->features = {
        raw_level,
        target,
        energy,
        audio_time_ms,
        gate_open,
    };
    state->history[state->history_head] = {
        energy_lab_hue,
        255.0f,
        energy,
        energy,
        0.0f,
        0.0f,
        audio_time_ms,
    };
    state->history_head =
        (uint16_t)((state->history_head + 1) % LV_FLOW_V3_HISTORY);
    state->history_count =
        std::min<uint16_t>(state->history_count + 1, LV_FLOW_V3_HISTORY);
    state->last_update_ms = audio_time_ms;
    state->initialized = true;
}

void lv_flow_v3_energy_set_travel_ms(lv_flow_v3_energy_t *state,
                                     uint16_t travel_ms) {
    state->travel_ms = travel_ms > 0
                           ? std::clamp<uint16_t>(travel_ms, 50, 1000)
                           : (uint16_t)LV_FLOW_V3_DEFAULT_TRAVEL_MS;
}

lv_flow_v3_slice_t lv_flow_v3_energy_sample(
    const lv_flow_v3_energy_t *state, float coordinate, uint32_t now_ms) {
    if (state->history_count == 0) return {};
    const uint16_t travel_ms = state->travel_ms > 0
                                   ? state->travel_ms
                                   : LV_FLOW_V3_DEFAULT_TRAVEL_MS;
    const float target_time =
        (float)now_ms - std::max(0.0f, coordinate) * travel_ms;
    const uint16_t oldest = (uint16_t)(
        (state->history_head + LV_FLOW_V3_HISTORY - state->history_count) %
        LV_FLOW_V3_HISTORY);
    const auto &first = state->history[oldest];
    if (target_time < first.audio_time_ms) return {};

    lv_flow_v3_slice_t previous = first;
    for (uint16_t offset = 1; offset < state->history_count; ++offset) {
        const auto &next = state->history[
            (oldest + offset) % LV_FLOW_V3_HISTORY];
        if (target_time <= next.audio_time_ms) {
            const uint32_t interval = next.audio_time_ms -
                                      previous.audio_time_ms;
            const float amount = interval > 0
                                     ? (target_time -
                                        previous.audio_time_ms) /
                                           interval
                                     : 1.0f;
            return blend(previous, next, amount);
        }
        previous = next;
    }
    return previous;
}

void lv_flow_v3_energy_render(const lv_flow_v3_energy_t *state,
                              uint32_t now_ms,
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
                        render_sample(blurred_sample(
                                          state, coordinate, now_ms),
                                      coordinate));
            }
        }
        for (int key = 0; key < 3; ++key) {
            const float coordinate = lv_spectral_flow_coordinate(
                half, 0.0f, (float)(key + 1));
            set_key(frame, lv_utility_key_at(half, key),
                    render_sample(blurred_sample(
                                      state, coordinate, now_ms),
                                  coordinate));
        }
        constexpr float thumb_distance[4] = {1.25f, 0.85f, 0.45f, 0.0f};
        for (int key = 0; key < 4; ++key) {
            const float coordinate = lv_spectral_flow_coordinate(
                half, thumb_distance[key], 3.2f + key * 0.2f);
            set_key(frame, lv_thumb_key_at(half, key),
                    render_sample(blurred_sample(
                                      state, coordinate, now_ms),
                                  coordinate));
        }
    }
}
