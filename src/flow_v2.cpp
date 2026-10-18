#include "landerviz/flow_v2.h"

#include <algorithm>
#include <cmath>

#include "landerviz/visualizer.h"

namespace {

constexpr float flow_gain_target = 0.35f;
constexpr float flow_gain_min = 0.50f;
constexpr float flow_gain_max = 8.0f;
constexpr float flow_gain_reduce_ms = 250.0f;
constexpr float flow_gain_recover_ms = 4000.0f;
constexpr float flow_gain_silence_ms = 10000.0f;
constexpr float flow_value_floor = 51.0f;
constexpr float flow_value_attack_ms = 20.0f;
constexpr float flow_value_release_ms = 220.0f;
constexpr float flow_centroid_smoothing_ms = 140.0f;
constexpr float flow_v2_distance_decay = 0.99f;
constexpr float flow_palette_center_hz = 500.0f;
// Calibrated against the centroid distributions of three different pop/rock
// tracks. This fills most of the palette without pinning sustained passages
// to an endpoint and losing their color motion.
constexpr float flow_palette_contrast = 2.4f;
constexpr int flow_onset_band_count = 3;
constexpr int flow_onset_bands_per_group =
    LV_MEL_BAND_COUNT / flow_onset_band_count;
constexpr float flow_onset_log_scale = 15.0f;
constexpr float flow_onset_adaptation_ms = 1400.0f;
constexpr float flow_onset_deviation_weight = 2.4f;
constexpr float flow_onset_floor[flow_onset_band_count] = {
    0.060f, 0.052f, 0.045f};
constexpr uint32_t flow_onset_refractory_ms[flow_onset_band_count] = {
    80, 70, 50};
constexpr uint32_t flow_onset_shared_refractory_ms = 80;

// Centers of the twelve triangular mel filters in audio_analysis.cpp.
// Keeping these in Hz makes traces intelligible even though smoothing and
// palette lookup happen in log2-frequency space.
constexpr float mel_center_hz[LV_MEL_BAND_COUNT] = {
    212.59f, 395.57f, 615.83f, 879.52f,
    1197.24f, 1575.47f, 2035.18f, 2584.86f,
    3244.03f, 4039.21f, 4984.92f, 6084.41f,
};

// The hues originate from nine high-chroma OKLCH stops. User testing showed
// that preserving their sRGB lightness made the already-capped keyboard output
// muddy, so v2 deliberately drives every stop at nearly full saturation and
// full value. The hues stay unwrapped so interpolation crosses red in one
// direction. tools/generate_flow_palette.py documents the source colors.
constexpr lv_flow_v2_palette_color_t flow_palette[] = {
    {250.50f, 245.0f, 255.0f},
    {274.35f, 245.0f, 255.0f},
    {287.01f, 245.0f, 255.0f},
    {321.44f, 245.0f, 255.0f},
    {378.00f, 245.0f, 255.0f},
    {392.32f, 245.0f, 255.0f},
    {410.65f, 245.0f, 255.0f},
    {444.70f, 245.0f, 255.0f},
    {474.91f, 245.0f, 255.0f},
};

float smoothing_response(uint32_t elapsed_ms, float time_constant_ms) {
    return elapsed_ms > 0
               ? 1.0f - std::exp(-(float)elapsed_ms / time_constant_ms)
               : 1.0f;
}

float target_packet_value(float linear_peak_level) {
    const float perceptual = lv_perceptual_level(linear_peak_level);
    return perceptual > 0.0f
               ? flow_value_floor +
                     (255.0f - flow_value_floor) * perceptual
               : 0.0f;
}

void spectrum_features(const float bands[LV_MEL_BAND_COUNT],
                       float *centroid_log2, float *dominance) {
    float weighted_log_frequency = 0.0f;
    float total_power = 0.0f;
    float maximum_power = 0.0f;
    for (int band = 0; band < LV_MEL_BAND_COUNT; ++band) {
        const float level = std::clamp(bands[band], 0.0f, 1.0f);
        const float power = level * level;
        weighted_log_frequency += power * std::log2(mel_center_hz[band]);
        total_power += power;
        maximum_power = std::max(maximum_power, power);
    }

    if (total_power <= 0.000001f) {
        *centroid_log2 = 0.0f;
        *dominance = 0.0f;
        return;
    }

    *centroid_log2 = weighted_log_frequency / total_power;
    const float raw_dominance = maximum_power / total_power;
    constexpr float balanced = 1.0f / LV_MEL_BAND_COUNT;
    *dominance = std::clamp(
        (raw_dominance - balanced) / (1.0f - balanced), 0.0f, 1.0f);
}

float log_spectral_level(float level) {
    return std::log1p(flow_onset_log_scale *
                      std::clamp(level, 0.0f, 1.0f)) /
           std::log1p(flow_onset_log_scale);
}

float onset_detector_update(lv_flow_v2_onset_detector_t *detector,
                            float flux, bool gate_open, uint32_t now_ms,
                            int onset_band, float *threshold,
                            bool *triggered) {
    if (!detector->initialized) {
        detector->last_update_ms = now_ms;
        detector->initialized = true;
    }
    const uint32_t elapsed_ms = now_ms - detector->last_update_ms;
    *threshold = std::max(
        flow_onset_floor[onset_band],
        detector->mean +
            flow_onset_deviation_weight * detector->deviation);
    const bool refractory =
        detector->onset_seen &&
        now_ms - detector->last_onset_ms <
            flow_onset_refractory_ms[onset_band];
    *triggered = gate_open && !refractory && flux > *threshold;

    float strength = 0.0f;
    if (*triggered) {
        const float excess = flux - *threshold;
        strength = std::clamp(
            excess / std::max(0.12f, *threshold * 2.5f), 0.0f, 1.0f);
    }
    // Learn ordinary musical motion as well as quiet frames, but cap an
    // outlier's contribution so a strong beat cannot suppress its successors.
    const float training_flux = std::min(flux, *threshold + 0.10f);
    const float response =
        smoothing_response(elapsed_ms, flow_onset_adaptation_ms);
    detector->mean += (training_flux - detector->mean) * response;
    const float distance = std::fabs(training_flux - detector->mean);
    detector->deviation +=
        (distance - detector->deviation) * response;
    detector->last_update_ms = now_ms;
    return strength;
}

void onset_features(
    lv_flow_v2_onset_state_t *state,
    const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT],
    bool gate_open, uint32_t now_ms, lv_flow_v2_features_t *features) {
    float current_log_level[LV_MEL_BAND_COUNT]{};
    for (int band = 0; band < LV_MEL_BAND_COUNT; ++band) {
        // A maximum-energy merge behaves like mono event analysis without
        // summing waveforms, so phase cancellation cannot hide a panned hit.
        const float shared_level = std::max(audio[0].mel_level[band],
                                            audio[1].mel_level[band]);
        current_log_level[band] = log_spectral_level(shared_level);
    }

    float strength[flow_onset_band_count]{};
    for (int onset_band = 0; onset_band < flow_onset_band_count;
         ++onset_band) {
        float squared_flux = 0.0f;
        for (int offset = 0; offset < flow_onset_bands_per_group; ++offset) {
            const int band = onset_band * flow_onset_bands_per_group + offset;
            const float increase =
                state->initialized
                    ? std::max(0.0f, current_log_level[band] -
                                        state->previous_log_level[band])
                    : 0.0f;
            squared_flux += increase * increase;
        }
        const float flux = std::sqrt(
            squared_flux / (float)flow_onset_bands_per_group);
        features->onset_flux[onset_band] = flux;
        strength[onset_band] = onset_detector_update(
            &state->detector[onset_band], flux,
            gate_open && state->initialized, now_ms, onset_band,
            &features->onset_threshold[onset_band],
            &features->onset_triggered[onset_band]);
    }

    uint8_t triggered_mask = 0;
    for (int onset_band = 0; onset_band < flow_onset_band_count;
         ++onset_band) {
        if (features->onset_triggered[onset_band]) {
            triggered_mask |= (uint8_t)(1u << onset_band);
        }
    }
    const bool any_triggered = triggered_mask != 0;
    const bool shared_refractory =
        state->shared_onset_seen &&
        now_ms - state->last_shared_onset_ms <
            flow_onset_shared_refractory_ms;
    // A snare or high-frequency transient must not consume the time slot of a
    // kick that follows it. Bass may preempt a recent non-bass event, while a
    // recent bass event continues to suppress less important visual chatter.
    const bool bass_preempts_non_bass =
        (triggered_mask & 0x01u) != 0 &&
        (state->last_shared_onset_mask & 0x01u) == 0;
    if (any_triggered && shared_refractory && !bass_preempts_non_bass) {
        for (int onset_band = 0; onset_band < flow_onset_band_count;
             ++onset_band) {
            features->onset_triggered[onset_band] = false;
            strength[onset_band] = 0.0f;
        }
    } else if (any_triggered) {
        state->last_shared_onset_ms = now_ms;
        state->last_shared_onset_mask = triggered_mask;
        state->shared_onset_seen = true;
        for (int onset_band = 0; onset_band < flow_onset_band_count;
             ++onset_band) {
            if (!features->onset_triggered[onset_band]) continue;
            state->detector[onset_band].last_onset_ms = now_ms;
            state->detector[onset_band].onset_seen = true;
        }
    }

    std::copy_n(current_log_level, LV_MEL_BAND_COUNT,
                state->previous_log_level);
    state->initialized = true;
    features->onset_low = strength[0];
    features->onset_mid = strength[1];
    features->onset_high = strength[2];
}

}  // namespace

float lv_flow_v2_gain_update(lv_flow_v2_gain_t *state, float linear_level,
                             bool gate_open, uint32_t now_ms) {
    linear_level = std::max(0.0f, linear_level);
    if (!state->initialized) {
        state->gain = 1.0f;
        state->last_update_ms = now_ms;
        state->initialized = true;
    }
    const uint32_t elapsed_ms = now_ms - state->last_update_ms;

    if (!gate_open) {
        // Silence cannot wind gain upward. A very slow return toward unity
        // prevents an old track's gain from persisting forever.
        const float response =
            smoothing_response(elapsed_ms, flow_gain_silence_ms);
        state->gain += (1.0f - state->gain) * response;
        state->last_update_ms = now_ms;
        return state->gain;
    }

    const float target = std::clamp(
        flow_gain_target / std::max(linear_level, 0.0001f),
        flow_gain_min, flow_gain_max);
    if (!state->signal_seen) {
        state->gain = target;
        state->signal_seen = true;
    } else {
        const float time_constant = target < state->gain
                                        ? flow_gain_reduce_ms
                                        : flow_gain_recover_ms;
        const float response =
            smoothing_response(elapsed_ms, time_constant);
        state->gain += (target - state->gain) * response;
    }
    state->gain = std::clamp(state->gain, flow_gain_min, flow_gain_max);
    state->last_update_ms = now_ms;
    return state->gain;
}

lv_flow_v2_palette_color_t lv_flow_v2_palette(float centroid_hz,
                                              float dominance) {
    constexpr int stop_count =
        (int)(sizeof(flow_palette) / sizeof(flow_palette[0]));
    const float position =
        lv_flow_v2_palette_position(centroid_hz) * (stop_count - 1);
    const int first = std::min((int)position, stop_count - 1);
    const int second = std::min(first + 1, stop_count - 1);
    const float blend = position - first;

    lv_flow_v2_palette_color_t color{
        flow_palette[first].hue +
            (flow_palette[second].hue - flow_palette[first].hue) * blend,
        flow_palette[first].saturation +
            (flow_palette[second].saturation -
             flow_palette[first].saturation) * blend,
        flow_palette[first].value_scale +
            (flow_palette[second].value_scale -
             flow_palette[first].value_scale) * blend,
    };
    dominance = std::clamp(dominance, 0.0f, 1.0f);
    const float saturation_sharpen = 0.25f + 0.75f * dominance;
    color.saturation +=
        (255.0f - color.saturation) * saturation_sharpen;
    return color;
}

float lv_flow_v2_palette_position(float centroid_hz) {
    const float low_log = std::log2(mel_center_hz[0]);
    const float high_log = std::log2(mel_center_hz[LV_MEL_BAND_COUNT - 1]);
    const float centered =
        (std::log2(std::max(centroid_hz, mel_center_hz[0])) -
         std::log2(flow_palette_center_hz)) /
        (high_log - low_log);
    return std::clamp(
        0.5f + centered * flow_palette_contrast, 0.0f, 1.0f);
}

void lv_flow_v2_update(
    lv_flow_v2_t *state,
    const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT],
    bool gate_open, uint32_t audio_time_ms) {
    const uint32_t elapsed_ms =
        state->initialized ? audio_time_ms - state->last_update_ms : 0;
    float shared_peak_level = 0.0f;
    for (int channel = 0; channel < LV_AUDIO_CHANNEL_COUNT; ++channel) {
        shared_peak_level = std::max(
            shared_peak_level, audio[channel].rms_level * 1.41421356f);
    }
    const float gain = lv_flow_v2_gain_update(
        &state->gain, shared_peak_level, gate_open, audio_time_ms);

    state->features = {};
    state->features.gain = gain;
    state->features.audio_time_ms = audio_time_ms;
    state->features.gate_open = gate_open;
    onset_features(&state->onset, audio, gate_open, audio_time_ms,
                   &state->features);

    for (int channel = 0; channel < LV_AUDIO_CHANNEL_COUNT; ++channel) {
        auto &channel_state = state->channel[channel];
        auto &features = state->features.channel[channel];
        features.raw_level = audio[channel].rms_level * 1.41421356f;

        const float gained_level = features.raw_level * gain;
        const float target_value =
            gate_open ? target_packet_value(gained_level) : 0.0f;
        if (!state->initialized) {
            channel_state.value = target_value;
        } else {
            const float time_constant = target_value > channel_state.value
                                            ? flow_value_attack_ms
                                            : flow_value_release_ms;
            channel_state.value +=
                (target_value - channel_state.value) *
                smoothing_response(elapsed_ms, time_constant);
        }

        float centroid_log2 = 0.0f;
        float dominance = 0.0f;
        spectrum_features(audio[channel].mel_level, &centroid_log2,
                          &dominance);
        if (centroid_log2 > 0.0f) {
            features.raw_centroid_hz = std::exp2(centroid_log2);
            if (!channel_state.centroid_initialized) {
                channel_state.centroid_log2 = centroid_log2;
                channel_state.centroid_initialized = true;
            } else {
                channel_state.centroid_log2 +=
                    (centroid_log2 - channel_state.centroid_log2) *
                    smoothing_response(elapsed_ms,
                                       flow_centroid_smoothing_ms);
            }
        }

        features.centroid_hz = channel_state.centroid_initialized
                                   ? std::exp2(channel_state.centroid_log2)
                                   : mel_center_hz[0];
        if (features.raw_centroid_hz == 0.0f) {
            features.raw_centroid_hz = features.centroid_hz;
        }
        features.palette_position =
            lv_flow_v2_palette_position(features.centroid_hz);
        features.dominance = dominance;
        const auto palette =
            lv_flow_v2_palette(features.centroid_hz, dominance);
        features.hue = palette.hue;
        features.saturation = palette.saturation;
        features.value = channel_state.value * palette.value_scale / 255.0f;

        if (state->ribbon[channel].distance_decay == 0.0f) {
            lv_spectral_flow_set_distance_decay(
                &state->ribbon[channel], flow_v2_distance_decay);
        }
        lv_spectral_flow_update_packet(
            &state->ribbon[channel], palette.hue, palette.saturation,
            features.value,
            std::max({std::sqrt(state->features.onset_low),
                      std::sqrt(state->features.onset_mid) * 0.55f,
                      std::sqrt(state->features.onset_high) * 0.32f}),
            audio_time_ms);
    }

    state->last_update_ms = audio_time_ms;
    state->initialized = true;
}

void lv_flow_v2_set_travel_ms(lv_flow_v2_t *state, uint16_t travel_ms) {
    if (travel_ms > 0) {
        travel_ms = std::clamp<uint16_t>(travel_ms, 50, 1000);
    }
    for (auto &ribbon : state->ribbon) {
        ribbon.travel_ms = travel_ms;
        lv_spectral_flow_set_distance_decay(&ribbon,
                                            flow_v2_distance_decay);
    }
}

void lv_flow_v2_render(const lv_flow_v2_t *state, uint32_t now_ms,
                       lv_rgb_t frame[LV_KEY_COUNT]) {
    lv_visualizer_render_flow_ribbons(state->ribbon, now_ms, frame);
}
