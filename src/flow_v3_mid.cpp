#include "landerviz/flow_v3_mid.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "landerviz/effects.h"
#include "landerviz/moonlander_map.h"

namespace {

constexpr int first_mid_band = 1;
constexpr int last_mid_band = 7;
constexpr float mid_presence_floor = 0.01f;
constexpr float mid_presence_attack_ms = 110.0f;
constexpr float mid_presence_release_ms = 240.0f;
constexpr float mid_hue_smoothing_ms = 120.0f;
constexpr float mid_baseline_smoothing_ms = 1800.0f;
constexpr float mid_contrast_floor = 0.08f;
constexpr float mid_contrast_full_scale = 0.60f;
constexpr float mid_contrast_max_influence = 0.55f;
constexpr float mid_contrast_max_palette_shift = 0.40f;
constexpr float mid_lightness = 0.62f;
constexpr float mid_low_hue = 20.0f;
constexpr float mid_high_hue = 320.0f;
constexpr float mid_palette_center_log2 = 9.731319031f;
constexpr float mid_palette_octave_span = 1.40f;
constexpr float mel_center_log2[LV_MEL_BAND_COUNT] = {
    7.731929925f, 8.627789206f, 9.266388339f, 9.780572574f,
    10.225496670f, 10.621566568f, 10.990940683f, 11.335870429f,
    11.663571446f, 11.979857439f, 12.283354637f, 12.570901657f,
};

float response(uint32_t elapsed_ms, float time_constant_ms) {
    return elapsed_ms > 0
               ? 1.0f - std::exp(-(float)elapsed_ms / time_constant_ms)
               : 1.0f;
}

float nearest_hue(float hue, float reference) {
    while (hue - reference > 180.0f) hue -= 360.0f;
    while (hue - reference < -180.0f) hue += 360.0f;
    return hue;
}

float smoothstep(float value) {
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

float base_palette_position(float log_frequency) {
    return std::clamp(
        0.5f + (log_frequency - mid_palette_center_log2) /
                   mid_palette_octave_span,
        0.0f, 1.0f);
}

float palette_hue(float position) {
    return mid_low_hue +
           (mid_high_hue - mid_low_hue) *
               std::clamp(position, 0.0f, 1.0f);
}

void set_key(lv_rgb_t frame[LV_KEY_COUNT], int key, lv_rgb_t color) {
    if (key >= 0 && key < LV_KEY_COUNT) frame[key] = color;
}

lv_rgb_t render_sample(const lv_flow_v3_slice_t &sample, float coordinate) {
    const float distance_fade = std::pow(0.985f, std::max(0.0f, coordinate));
    return lv_flow_v3_vivid_color(
        sample.hue,
        mid_lightness * std::clamp(sample.body, 0.0f, 1.0f) *
            distance_fade);
}

}  // namespace

void lv_flow_v3_mid_update(
    lv_flow_v3_mid_t *state,
    const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT],
    bool gate_open, uint32_t audio_time_ms) {
    const uint32_t elapsed_ms =
        state->initialized ? audio_time_ms - state->last_update_ms : 0;
    float level[LV_MEL_BAND_COUNT]{};
    float weighted_log_frequency = 0.0f;
    float total_power = 0.0f;
    for (int band = first_mid_band; band <= last_mid_band; ++band) {
        level[band] = std::max(audio[0].mel_level[band],
                               audio[1].mel_level[band]);
        const float power = level[band] * level[band];
        weighted_log_frequency += power * mel_center_log2[band];
        total_power += power;
    }
    constexpr int mid_band_count = last_mid_band - first_mid_band + 1;
    const float raw_level = std::sqrt(total_power / mid_band_count);
    const bool signal = gate_open && raw_level > mid_presence_floor;
    const float centroid_log_frequency = total_power > 0.000001f
                                             ? weighted_log_frequency /
                                                   total_power
                                             : 0.0f;
    const float centroid_hz = total_power > 0.000001f
                                  ? std::exp2(centroid_log_frequency)
                                  : 0.0f;

    float contrast_power = 0.0f;
    float contrast_weighted_log_frequency = 0.0f;
    if (signal && !state->baseline_initialized) {
        std::copy_n(level, LV_MEL_BAND_COUNT,
                    state->spectral_baseline);
        state->baseline_initialized = true;
    } else if (signal) {
        for (int band = first_mid_band; band <= last_mid_band; ++band) {
            const float rise = std::max(
                0.0f, level[band] - state->spectral_baseline[band]);
            const float power = rise * rise;
            contrast_power += power;
            contrast_weighted_log_frequency +=
                power * mel_center_log2[band];
        }
    }
    const float contrast_centroid_log_frequency =
        contrast_power > 0.000001f
            ? contrast_weighted_log_frequency / contrast_power
            : 0.0f;
    const float contrast_centroid_hz =
        contrast_power > 0.000001f
            ? std::exp2(contrast_centroid_log_frequency)
            : 0.0f;
    const float contrast_level = std::sqrt(contrast_power / mid_band_count);
    const float contrast_ratio = raw_level > mid_presence_floor
                                     ? contrast_level / raw_level
                                     : 0.0f;
    const float contrast_strength = mid_contrast_max_influence * smoothstep(
        (contrast_ratio - mid_contrast_floor) /
        (mid_contrast_full_scale - mid_contrast_floor));

    const float base_position = total_power > 0.000001f
                                    ? base_palette_position(
                                          centroid_log_frequency)
                                    : 0.0f;
    const float contrast_direction =
        contrast_power > 0.000001f && total_power > 0.000001f
            ? std::clamp(contrast_centroid_log_frequency -
                             centroid_log_frequency,
                         -1.0f, 1.0f)
            : 0.0f;
    const float target_position = std::clamp(
        base_position + contrast_direction * contrast_strength *
                            mid_contrast_max_palette_shift,
        0.0f, 1.0f);
    const float base_hue = palette_hue(base_position);
    const float target_hue = palette_hue(target_position);
    if (signal) {
        if (!state->hue_initialized) {
            state->smoothed_hue = target_hue;
            state->hue_initialized = true;
        } else {
            const float nearby = nearest_hue(target_hue,
                                             state->smoothed_hue);
            state->smoothed_hue +=
                (nearby - state->smoothed_hue) *
                response(elapsed_ms, mid_hue_smoothing_ms);
        }
    }

    if (state->baseline_initialized) {
        const float baseline_response = response(
            elapsed_ms, mid_baseline_smoothing_ms);
        for (int band = first_mid_band; band <= last_mid_band; ++band) {
            const float target = signal ? level[band] : 0.0f;
            state->spectral_baseline[band] +=
                (target - state->spectral_baseline[band]) *
                baseline_response;
        }
    }

    const float target_presence = signal ? 1.0f : 0.0f;
    if (!state->initialized) state->presence = target_presence;
    else {
        const float time_constant = target_presence > state->presence
                                        ? mid_presence_attack_ms
                                        : mid_presence_release_ms;
        state->presence +=
            (target_presence - state->presence) *
            response(elapsed_ms, time_constant);
    }
    const float hue = state->hue_initialized ? state->smoothed_hue
                                             : mid_low_hue;
    state->features = {
        raw_level,
        centroid_hz,
        contrast_centroid_hz,
        contrast_strength,
        base_hue,
        target_hue,
        hue,
        state->presence,
        state->presence > 0.001f ? mid_lightness : 0.0f,
        audio_time_ms,
        gate_open,
    };
    state->history[state->history_head] = {
        hue, 255.0f, raw_level, state->presence, 0.0f, 0.0f,
        audio_time_ms,
    };
    state->history_head =
        (uint16_t)((state->history_head + 1) % LV_FLOW_V3_HISTORY);
    state->history_count =
        std::min<uint16_t>(state->history_count + 1, LV_FLOW_V3_HISTORY);
    state->last_update_ms = audio_time_ms;
    state->initialized = true;
}

void lv_flow_v3_mid_set_travel_ms(lv_flow_v3_mid_t *state,
                                   uint16_t travel_ms) {
    state->travel_ms = travel_ms > 0
                           ? std::clamp<uint16_t>(travel_ms, 50, 1000)
                           : (uint16_t)LV_FLOW_V3_DEFAULT_TRAVEL_MS;
}

lv_flow_v3_slice_t lv_flow_v3_mid_sample(
    const lv_flow_v3_mid_t *state, float coordinate, uint32_t now_ms) {
    const uint16_t travel_ms = state->travel_ms > 0
                                   ? state->travel_ms
                                   : (uint16_t)LV_FLOW_V3_DEFAULT_TRAVEL_MS;
    return lv_flow_v3_history_sample(
        state->history, state->history_head, state->history_count,
        travel_ms, coordinate, now_ms);
}

void lv_flow_v3_mid_render(const lv_flow_v3_mid_t *state, uint32_t now_ms,
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
                        render_sample(lv_flow_v3_mid_sample(
                                          state, coordinate, now_ms),
                                      coordinate));
            }
        }
        for (int key = 0; key < 3; ++key) {
            const float coordinate = lv_spectral_flow_coordinate(
                half, 0.0f, (float)(key + 1));
            set_key(frame, lv_utility_key_at(half, key),
                    render_sample(lv_flow_v3_mid_sample(
                                      state, coordinate, now_ms),
                                  coordinate));
        }
        constexpr float thumb_distance[4] = {1.25f, 0.85f, 0.45f, 0.0f};
        for (int key = 0; key < 4; ++key) {
            const float coordinate = lv_spectral_flow_coordinate(
                half, thumb_distance[key], 3.2f + key * 0.2f);
            set_key(frame, lv_thumb_key_at(half, key),
                    render_sample(lv_flow_v3_mid_sample(
                                      state, coordinate, now_ms),
                                  coordinate));
        }
    }
}
