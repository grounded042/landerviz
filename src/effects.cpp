#include "landerviz/effects.h"

#include <algorithm>
#include <cmath>

#include "landerviz/audio_analysis.h"

namespace {

constexpr float spectral_row_phase = 0.11f;
constexpr float spectral_blur_offsets[5] = {-0.70f, -0.35f, 0.0f, 0.35f,
                                             0.70f};
constexpr float spectral_blur_weights[5] = {1.0f, 4.0f, 6.0f, 4.0f, 1.0f};
constexpr uint32_t spectral_flow_stale_ms = 2000;
constexpr uint32_t spectral_kick_refractory_ms = 180;
constexpr uint32_t spectral_event_release_ms = 66;
constexpr float spectral_lane_peak_release_ms = 3500.0f;
constexpr float spectral_lane_smoothing_ms = 100.0f;
constexpr float spectral_hue_slew_per_frame = 8.0f;
constexpr float spectral_value_attack_ms = 70.0f;
constexpr float spectral_value_release_ms = 160.0f;

constexpr float spectral_kick_hue = 8.0f;

float clamp_level(float level) {
    return std::clamp(level, 0.0f, 1.0f);
}

uint8_t color_channel(float value) {
    return (uint8_t)std::clamp(value + 0.5f, 0.0f, 255.0f);
}

float wrap_hue(float hue) {
    while (hue < 0.0f) hue += 256.0f;
    while (hue >= 256.0f) hue -= 256.0f;
    return hue;
}

float nearest_hue(float hue, float reference) {
    hue = wrap_hue(hue);
    const float reference_turn = reference - wrap_hue(reference);
    hue += reference_turn;
    if (hue - reference > 128.0f) hue -= 256.0f;
    if (reference - hue > 128.0f) hue += 256.0f;
    return hue;
}

bool rgb_mix_hue(const float channel[LV_SPECTRAL_COLOR_LANES],
                 float *hue) {
    const float red = channel[0];
    const float green = channel[1];
    const float blue = channel[2];
    const float maximum = std::max({red, green, blue});
    const float minimum = std::min({red, green, blue});
    const float chroma = maximum - minimum;
    // Near-gray mixtures have no stable hue. Retaining the previous hue is
    // preferable to magnifying tiny analyzer noise into a full color jump.
    if (chroma < 0.06f) return false;

    if (maximum == red) {
        *hue = 42.6666667f * (green - blue) / chroma;
    } else if (maximum == green) {
        *hue = 85.3333333f + 42.6666667f * (blue - red) / chroma;
    } else {
        *hue = 170.6666667f + 42.6666667f * (red - green) / chroma;
    }
    *hue = wrap_hue(*hue);
    return true;
}

float fast_distance_decay(float coordinate) {
    static constexpr float whole_decay[7] = {
        1.0f, 0.97f, 0.9409f, 0.912673f,
        0.88529281f, 0.858734026f, 0.832972005f,
    };
    const int whole = std::clamp((int)coordinate, 0, 6);
    const float fraction = coordinate - (float)whole;
    // Third-order expansion of 0.97^fraction. Keeping integer and fractional
    // factors separate preserves an exact 0.97 ratio one column later.
    const float fraction_decay =
        1.0f - 0.0304592075f * fraction +
        0.000463880f * fraction * fraction -
        0.000004710f * fraction * fraction * fraction;
    return whole_decay[whole] * fraction_decay;
}

void spectral_source_hsv(
    lv_spectral_flow_t *flow,
    const float band_level[LV_SPECTRAL_FLOW_BANDS], float bass_transient,
    const lv_kick_event_t *kick, bool reset_analysis,
    uint32_t elapsed_ms, uint32_t now_ms) {
    (void)bass_transient;
    if (reset_analysis) {
        flow->base_hue = 0.0f;
        flow->base_value = 0.0f;
        flow->source_hue = 0.0f;
        flow->source_value = 0.0f;
        flow->hue_initialized = false;
        for (int lane = 0; lane < LV_SPECTRAL_COLOR_LANES; ++lane) {
            flow->lane_peak[lane] = 0.0f;
            flow->lane_level[lane] = 0.0f;
        }
    }

    float level[LV_SPECTRAL_FLOW_BANDS] = {};
    float energy = 0.0f;
    for (int band = 0; band < LV_SPECTRAL_FLOW_BANDS; ++band) {
        level[band] = clamp_level(band_level[band]);
        energy = std::max(energy, level[band]);
    }
    // Scott Lawson's scroll visualization groups the spectrum into broad RGB
    // regions. Doing that here prevents neighboring mel filters from fighting
    // over the color while independent peak tracking lets each region remain
    // visible despite different absolute energy.
    float lane_input[LV_SPECTRAL_COLOR_LANES] = {};
    for (int lane = 0; lane < LV_SPECTRAL_COLOR_LANES; ++lane) {
        float power = 0.0f;
        for (int band = lane * 4; band < lane * 4 + 4; ++band) {
            power += level[band] * level[band];
        }
        lane_input[lane] = std::sqrt(power * 0.25f);
    }
    const float peak_release = elapsed_ms > 0
                                   ? std::exp(-(float)elapsed_ms /
                                              spectral_lane_peak_release_ms)
                                   : 1.0f;
    const float lane_response = elapsed_ms > 0
                                    ? 1.0f - std::exp(
                                          -(float)elapsed_ms /
                                          spectral_lane_smoothing_ms)
                                    : 1.0f;
    for (int lane = 0; lane < LV_SPECTRAL_COLOR_LANES; ++lane) {
        flow->lane_peak[lane] = std::max(
            lane_input[lane], flow->lane_peak[lane] * peak_release);
        const float normalized = lane_input[lane] /
                                 std::max(flow->lane_peak[lane], 0.035f);
        if (reset_analysis) flow->lane_level[lane] = normalized;
        else {
            flow->lane_level[lane] +=
                (normalized - flow->lane_level[lane]) * lane_response;
        }
    }

    float selected_hue = 0.0f;
    if (rgb_mix_hue(flow->lane_level, &selected_hue)) {
        if (flow->hue_initialized && elapsed_ms > 0) {
            const float target = nearest_hue(selected_hue, flow->base_hue);
            const float maximum_step = spectral_hue_slew_per_frame *
                                       (float)elapsed_ms /
                                       LV_SPECTRAL_FLOW_FRAME_MS;
            flow->base_hue += std::clamp(
                target - flow->base_hue, -maximum_step, maximum_step);
        } else {
            flow->base_hue = selected_hue;
        }
        flow->hue_initialized = true;
    }

    // FLOW represents the music through color and motion. Keeping its lit
    // ribbon at one brightness prevents ordinary level variation from
    // appearing as unrelated flashes inside a traveling color segment.
    const float target_value = energy > 0.0001f ? 255.0f : 0.0f;
    if (reset_analysis || elapsed_ms == 0) {
        flow->base_value = target_value;
    } else {
        const float time_constant = target_value > flow->base_value
                                        ? spectral_value_attack_ms
                                        : spectral_value_release_ms;
        const float response =
            1.0f - std::exp(-(float)elapsed_ms / time_constant);
        flow->base_value += (target_value - flow->base_value) * response;
    }
    flow->source_value = flow->base_value;

    if (kick != nullptr && kick->triggered) {
        const float reference = flow->hue_initialized
                                    ? flow->base_hue
                                    : flow->source_hue;
        flow->accent_hue = nearest_hue(spectral_kick_hue, reference);
        flow->accent_value = std::max(
            flow->base_value, 220.0f + 35.0f * clamp_level(kick->strength));
        flow->accent_until_ms = now_ms + LV_SPECTRAL_KICK_HOLD_MS;
        flow->accent_release_until_ms =
            flow->accent_until_ms + spectral_event_release_ms;
    }

    if (now_ms < flow->accent_until_ms) {
        flow->source_hue = flow->accent_hue;
        flow->source_value = flow->accent_value;
    } else if (now_ms < flow->accent_release_until_ms) {
        const float release =
            (float)(now_ms - flow->accent_until_ms) /
            (float)spectral_event_release_ms;
        const float destination = nearest_hue(flow->base_hue,
                                              flow->accent_hue);
        flow->source_hue = flow->accent_hue +
                           (destination - flow->accent_hue) * release;
        flow->source_value = flow->accent_value +
                             (flow->base_value - flow->accent_value) * release;
    } else {
        flow->source_hue = flow->base_hue;
        flow->source_value = flow->base_value;
    }
}

void append_ribbon_sample(lv_spectral_flow_t *flow, uint32_t now_ms) {
    if (flow->history_count > 0 &&
        flow->history[flow->history_head].time_ms == now_ms) {
        flow->history[flow->history_head] = {
            flow->source_hue, flow->source_value, now_ms};
        return;
    }

    flow->history_head =
        (uint16_t)((flow->history_head + 1) % LV_SPECTRAL_FLOW_HISTORY);
    flow->history[flow->history_head] = {
        flow->source_hue, flow->source_value, now_ms};
    if (flow->history_count < LV_SPECTRAL_FLOW_HISTORY) {
        ++flow->history_count;
    }
}

lv_spectral_flow_sample_t ribbon_sample_at_age(
    const lv_spectral_flow_t *flow, uint32_t now_ms, float desired_age_ms) {
    const auto &newest = flow->history[flow->history_head];
    if (flow->history_count <= 1 || desired_age_ms <= 0.0f) return newest;

    const float newest_age = (float)(now_ms - newest.time_ms);
    lv_spectral_flow_sample_t newer = newest;
    float newer_age = newest_age;
    if (desired_age_ms <= newer_age) return newer;

    auto sample_at_index = [flow](uint16_t age_index) {
        const uint16_t index = (uint16_t)(
            (flow->history_head + LV_SPECTRAL_FLOW_HISTORY - age_index) %
            LV_SPECTRAL_FLOW_HISTORY);
        return flow->history[index];
    };

    const auto oldest = sample_at_index(flow->history_count - 1);
    if ((float)(now_ms - oldest.time_ms) < desired_age_ms) return oldest;

    uint16_t low = 1;
    uint16_t high = flow->history_count - 1;
    while (low < high) {
        const uint16_t middle = (uint16_t)(low + (high - low) / 2);
        const auto sample = sample_at_index(middle);
        if ((float)(now_ms - sample.time_ms) >= desired_age_ms) high = middle;
        else low = (uint16_t)(middle + 1);
    }
    const auto older = sample_at_index(low);
    newer = sample_at_index((uint16_t)(low - 1));
    newer_age = (float)(now_ms - newer.time_ms);
    const float older_age = (float)(now_ms - older.time_ms);
    const float interval = older_age - newer_age;
    const float blend = interval > 0.0f
                            ? (desired_age_ms - newer_age) / interval
                            : 0.0f;
    return {
        newer.hue + (older.hue - newer.hue) * blend,
        newer.value + (older.value - newer.value) * blend,
        desired_age_ms < (float)now_ms
            ? now_ms - (uint32_t)desired_age_ms
            : 0,
    };
}

}  // namespace

lv_rgb_t lv_hsv_to_rgb(uint8_t hue, uint8_t saturation, uint8_t value) {
    const uint8_t region = hue / 43;
    const uint8_t remainder = (hue - region * 43) * 6;
    const uint8_t p = (value * (255 - saturation)) >> 8;
    const uint8_t q = (value * (255 - ((saturation * remainder) >> 8))) >> 8;
    const uint8_t t =
        (value * (255 - ((saturation * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0: return {value, t, p};
        case 1: return {q, value, p};
        case 2: return {p, value, t};
        case 3: return {p, q, value};
        case 4: return {t, p, value};
        default: return {value, p, q};
    }
}

lv_kick_event_t lv_kick_detector_update(lv_kick_detector_t *detector,
                                        float bass_level,
                                        uint32_t now_ms) {
    lv_kick_event_t kick{};
    bass_level = clamp_level(bass_level);
    const bool stale = detector->initialized &&
                       now_ms - detector->last_update_ms >=
                           spectral_flow_stale_ms;
    if (!detector->initialized || stale) {
        detector->fast_envelope = bass_level;
        detector->slow_envelope = bass_level;
        detector->previous_level = bass_level;
        detector->last_update_ms = now_ms;
        detector->initialized = true;
        detector->kick_seen = false;
        return kick;
    }

    const float rise = bass_level - detector->previous_level;
    const float fast_rate = bass_level > detector->fast_envelope
                                ? 0.70f
                                : 0.25f;
    detector->fast_envelope +=
        (bass_level - detector->fast_envelope) * fast_rate;
    detector->slow_envelope +=
        (bass_level - detector->slow_envelope) * 0.06f;

    const float minimum_rise =
        std::max(0.035f, detector->slow_envelope * 0.16f);
    const bool transient = rise > minimum_rise &&
                           bass_level > 0.06f &&
                           detector->fast_envelope >
                               detector->slow_envelope * 1.12f + 0.005f;
    const bool refractory = detector->kick_seen &&
                            now_ms - detector->last_kick_ms <
                                spectral_kick_refractory_ms;
    if (transient && !refractory) {
        kick.triggered = true;
        kick.strength = std::clamp(rise / 0.65f, 0.0f, 1.0f);
        detector->last_kick_ms = now_ms;
        detector->kick_seen = true;
    }

    detector->previous_level = bass_level;
    detector->last_update_ms = now_ms;
    return kick;
}

float lv_spectral_flow_coordinate(int half, float outward_distance, float row) {
    const float bounded_row = std::clamp(row, 0.0f, 4.0f);
    const float phased_row = half == 1 ? 4.0f - bounded_row : bounded_row;
    return std::max(0.0f, outward_distance + phased_row * spectral_row_phase);
}

void lv_spectral_flow_update(
    lv_spectral_flow_t *flow,
    const float band_level[LV_SPECTRAL_FLOW_BANDS], float bass_transient,
    uint32_t now_ms, const lv_kick_event_t *kick) {
    const bool reset_history =
        flow->initialized &&
        now_ms - flow->last_frame_ms >= spectral_flow_stale_ms;
    const uint32_t elapsed_ms =
        flow->initialized && !reset_history ? now_ms - flow->last_frame_ms : 0;
    spectral_source_hsv(flow, band_level, bass_transient, kick,
                        !flow->initialized || reset_history, elapsed_ms,
                        now_ms);

    if (!flow->initialized || reset_history) {
        flow->history_head = 0;
        flow->history_count = 1;
        flow->history[0] = {
            flow->source_hue, flow->source_value, now_ms};
        flow->initialized = true;
        flow->last_frame_ms = now_ms;
        return;
    }

    append_ribbon_sample(flow, now_ms);
    flow->last_frame_ms = now_ms;
}

lv_rgb_t lv_spectral_flow_color(const lv_spectral_flow_t *flow,
                                float coordinate, uint32_t now_ms) {
    if (!flow->initialized || coordinate < 0.0f ||
        coordinate >= LV_SPECTRAL_FLOW_COLUMNS) {
        return {0, 0, 0};
    }

    float hue = 0.0f;
    float value = 0.0f;
    float total_weight = 0.0f;
    const uint16_t travel_ms = flow->travel_ms > 0
                                   ? flow->travel_ms
                                   : (uint16_t)LV_SPECTRAL_FLOW_TRAVEL_MS;
    for (int tap = 0; tap < 5; ++tap) {
        const float sample_coordinate =
            std::max(0.0f, coordinate + spectral_blur_offsets[tap]);
        const auto sample = ribbon_sample_at_age(
            flow, now_ms, sample_coordinate * travel_ms);
        hue += sample.hue * spectral_blur_weights[tap];
        value += sample.value * spectral_blur_weights[tap];
        total_weight += spectral_blur_weights[tap];
    }
    const float distance_decay = fast_distance_decay(coordinate);
    return lv_hsv_to_rgb(color_channel(wrap_hue(hue / total_weight)), 255,
                         color_channel(value / total_weight * distance_decay));
}

lv_rgb_t lv_aurora_color(float x, float y, uint32_t now_ms) {
    const float phase = (float)now_ms * 0.00035f;
    const float curtain = 0.5f + 0.5f * std::sin(
        x * 0.62f + phase + std::sin(y * 0.9f - phase * 0.4f));
    const float ribbon = 0.5f + 0.5f * std::sin(
        y * 1.15f - phase * 0.8f + std::cos(x * 0.33f) * 0.9f);
    const float intensity = 0.65f * curtain * curtain + 0.35f * ribbon;
    const uint8_t value = (uint8_t)(28.0f + 92.0f * intensity);

    const float hue_wave = 128.0f + 42.0f * std::sin(x * 0.28f - phase * 0.65f) +
                           24.0f * std::sin(y * 0.8f + phase);
    const uint8_t hue = (uint8_t)std::clamp(hue_wave, 78.0f, 202.0f);
    return lv_hsv_to_rgb(hue, 215, value);
}

lv_rgb_t lv_vu_row_color(float level, int row_from_bottom, int row_count) {
    if (row_count <= 0 || row_from_bottom < 0 || row_from_bottom >= row_count) {
        return {0, 0, 0};
    }

    level = std::clamp(level, 0.0f, 1.0f);
    const float fill = std::clamp(
        level * (float)row_count - (float)row_from_bottom, 0.0f, 1.0f);
    const uint8_t value = (uint8_t)(12.0f + fill * 243.0f);
    const uint8_t hue = row_count == 1
                            ? 85
                            : (uint8_t)(85 - row_from_bottom * 85 / (row_count - 1));
    return lv_hsv_to_rgb(hue, 255, value);
}

float lv_keyboard_vu_level(float linear_level) {
    return lv_meter_level(linear_level) > 0.0f
               ? lv_perceptual_level(linear_level)
               : 0.0f;
}

float lv_vu_smooth_level(float displayed_level, float target_level) {
    displayed_level = std::clamp(displayed_level, 0.0f, 1.0f);
    target_level = std::clamp(target_level, 0.0f, 1.0f);
    const float response = target_level > displayed_level ? 0.5376f : 0.1351f;
    const float next = displayed_level + (target_level - displayed_level) * response;
    return target_level == 0.0f && next < 0.002f ? 0.0f : next;
}
