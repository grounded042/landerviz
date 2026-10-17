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
constexpr float mid_palette_center_hz = 850.0f;
constexpr float mid_palette_octave_span = 1.40f;
constexpr float mel_center_hz[LV_MEL_BAND_COUNT] = {
    212.59f, 395.57f, 615.83f, 879.52f,
    1197.24f, 1575.47f, 2035.18f, 2584.86f,
    3244.03f, 4039.21f, 4984.92f, 6084.41f,
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

float base_palette_position(float frequency_hz) {
    if (frequency_hz <= 0.0f) return 0.0f;
    return std::clamp(
        0.5f + std::log2(frequency_hz / mid_palette_center_hz) /
                   mid_palette_octave_span,
        0.0f, 1.0f);
}

float palette_hue(float position) {
    return mid_low_hue +
           (mid_high_hue - mid_low_hue) *
               std::clamp(position, 0.0f, 1.0f);
}

float linear_to_srgb(float value) {
    return value <= 0.0031308f
               ? 12.92f * value
               : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
}

float srgb_to_linear(float value) {
    return value <= 0.04045f
               ? value / 12.92f
               : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

struct rgb_float_t {
    float red;
    float green;
    float blue;
};

rgb_float_t oklch_to_linear_srgb(float lightness, float chroma,
                                 float hue_degrees) {
    constexpr float pi = 3.14159265358979323846f;
    const float hue = hue_degrees * pi / 180.0f;
    const float a = chroma * std::cos(hue);
    const float b = chroma * std::sin(hue);
    const float l = std::pow(
        lightness + 0.3963377774f * a + 0.2158037573f * b, 3.0f);
    const float m = std::pow(
        lightness - 0.1055613458f * a - 0.0638541728f * b, 3.0f);
    const float s = std::pow(
        lightness - 0.0894841775f * a - 1.2914855480f * b, 3.0f);
    return {
        4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
        -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
        -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s,
    };
}

bool in_gamut(const rgb_float_t &color) {
    return color.red >= 0.0f && color.red <= 1.0f &&
           color.green >= 0.0f && color.green <= 1.0f &&
           color.blue >= 0.0f && color.blue <= 1.0f;
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
        255.0f,
        mix(first.energy, second.energy),
        mix(first.body, second.body),
        0.0f,
        0.0f,
        (uint32_t)std::lround(mix((float)first.audio_time_ms,
                                 (float)second.audio_time_ms)),
    };
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

lv_rgb_t lv_flow_v3_vivid_color(float hue_degrees, float lightness) {
    lightness = std::clamp(lightness, 0.0f, 1.0f);
    if (lightness <= 0.001f) return {};
    float low = 0.0f;
    float high = 0.4f;
    for (int iteration = 0; iteration < 14; ++iteration) {
        const float candidate = (low + high) * 0.5f;
        if (in_gamut(oklch_to_linear_srgb(lightness, candidate,
                                          hue_degrees))) {
            low = candidate;
        } else {
            high = candidate;
        }
    }
    const auto linear = oklch_to_linear_srgb(
        lightness, low * 0.99f, hue_degrees);
    const auto channel = [](float value) {
        return (uint8_t)std::clamp(
            std::lround(linear_to_srgb(value) * 255.0f), 0l, 255l);
    };
    return {channel(linear.red), channel(linear.green),
            channel(linear.blue)};
}

float lv_flow_v3_color_lightness(lv_rgb_t color) {
    const float red = srgb_to_linear((float)color.r / 255.0f);
    const float green = srgb_to_linear((float)color.g / 255.0f);
    const float blue = srgb_to_linear((float)color.b / 255.0f);
    const float l = std::cbrt(
        0.4122214708f * red + 0.5363325363f * green +
        0.0514459929f * blue);
    const float m = std::cbrt(
        0.2119034982f * red + 0.6806995451f * green +
        0.1073969566f * blue);
    const float s = std::cbrt(
        0.0883024619f * red + 0.2817188376f * green +
        0.6299787005f * blue);
    return 0.2104542553f * l + 0.7936177850f * m -
           0.0040720468f * s;
}

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
        weighted_log_frequency += power * std::log2(mel_center_hz[band]);
        total_power += power;
    }
    constexpr int mid_band_count = last_mid_band - first_mid_band + 1;
    const float raw_level = std::sqrt(total_power / mid_band_count);
    const bool signal = gate_open && raw_level > mid_presence_floor;
    const float centroid_hz = total_power > 0.000001f
                                  ? std::exp2(weighted_log_frequency /
                                              total_power)
                                  : 0.0f;

    float contrast_power = 0.0f;
    float contrast_weighted_log_frequency = 0.0f;
    if (signal && !state->baseline_initialized) {
        std::copy(std::begin(level), std::end(level),
                  std::begin(state->spectral_baseline));
        state->baseline_initialized = true;
    } else if (signal) {
        for (int band = first_mid_band; band <= last_mid_band; ++band) {
            const float rise = std::max(
                0.0f, level[band] - state->spectral_baseline[band]);
            const float power = rise * rise;
            contrast_power += power;
            contrast_weighted_log_frequency +=
                power * std::log2(mel_center_hz[band]);
        }
    }
    const float contrast_centroid_hz =
        contrast_power > 0.000001f
            ? std::exp2(contrast_weighted_log_frequency / contrast_power)
            : 0.0f;
    const float contrast_level = std::sqrt(contrast_power / mid_band_count);
    const float contrast_ratio = raw_level > mid_presence_floor
                                     ? contrast_level / raw_level
                                     : 0.0f;
    const float contrast_strength = mid_contrast_max_influence * smoothstep(
        (contrast_ratio - mid_contrast_floor) /
        (mid_contrast_full_scale - mid_contrast_floor));

    const float base_position = base_palette_position(centroid_hz);
    const float contrast_direction =
        contrast_centroid_hz > 0.0f && centroid_hz > 0.0f
            ? std::clamp(std::log2(contrast_centroid_hz / centroid_hz),
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
                                     ? (target_time - previous.audio_time_ms) /
                                           interval
                                     : 1.0f;
            return blend(previous, next, amount);
        }
        previous = next;
    }
    return previous;
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
