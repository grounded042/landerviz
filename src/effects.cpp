#include "landerviz/effects.h"

#include <algorithm>
#include <cmath>

#include "landerviz/audio_analysis.h"

namespace {

constexpr float spectral_flux_floor = 0.12f;
constexpr float spectral_hue_time_constant_ms = 180.0f;
constexpr float spectral_value_attack_ms = 70.0f;
constexpr float spectral_value_release_ms = 180.0f;
constexpr float spectral_row_phase = 0.11f;
constexpr float spectral_blur_offsets[5] = {-0.70f, -0.35f, 0.0f, 0.35f,
                                             0.70f};
constexpr float spectral_blur_weights[5] = {1.0f, 4.0f, 6.0f, 4.0f, 1.0f};
constexpr float spectral_carrier_wavelength = 2.5f;
constexpr uint32_t spectral_flow_stale_ms = 2000;

struct spectral_region_t {
    int first_band;
    int last_band;
    float event_weight;
};

constexpr spectral_region_t spectral_regions[3] = {
    {0, 2, 1.25f},  // bass and kick body
    {3, 7, 1.10f},  // guitar and melodic body
    {8, 11, 0.72f}, // presence, cymbals, and upper harmonics
};

// The body palette crosses the red end of the hue wheel instead of taking the
// long route through green and blue. Negative values are intentional aliases
// for hues just below 256.
constexpr float spectral_band_hues[LV_SPECTRAL_FLOW_BANDS] = {
    0.0f, 14.0f, 28.0f,
    42.0f, 10.0f, -20.0f, -55.0f, -80.0f,
    126.0f, 148.0f, 174.0f, 204.0f,
};

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

float fast_carrier_cos(float cycles) {
    // Linear interpolation through one 32-sample cosine cycle avoids dozens
    // of software floating-point cosine calls on every RP2040 frame.
    static constexpr int8_t cosine[32] = {
        127, 125, 117, 106, 90, 71, 49, 25,
        0, -25, -49, -71, -90, -106, -117, -125,
        -127, -125, -117, -106, -90, -71, -49, -25,
        0, 25, 49, 71, 90, 106, 117, 125,
    };
    const float scaled = cycles * 32.0f;
    int base = (int)scaled;
    if (scaled < (float)base) --base;
    const float fraction = scaled - (float)base;
    int index = base % 32;
    if (index < 0) index += 32;
    const int next = (index + 1) & 31;
    return ((float)cosine[index] +
            ((float)cosine[next] - cosine[index]) * fraction) /
           127.0f;
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
    bool reset_analysis, uint32_t elapsed_ms) {
    if (reset_analysis) {
        flow->hue_initialized = false;
        flow->gain_peak = 0.0f;
        flow->source_value = 0.0f;
        flow->previous_bass_transient = 0.0f;
        flow->bass_impulse = 0.0f;
        for (int band = 0; band < LV_SPECTRAL_FLOW_BANDS; ++band) {
            flow->previous_band_level[band] = 0.0f;
        }
    }

    const float bass_level = clamp_level(bass_transient);
    if (!reset_analysis && elapsed_ms > 0) {
        flow->bass_impulse *=
            std::exp(-(float)elapsed_ms / 90.0f);
    }
    if (bass_level > flow->previous_bass_transient + 0.03f) {
        flow->bass_impulse = std::max(flow->bass_impulse, bass_level);
    }
    flow->previous_bass_transient = bass_level;

    float energy = 0.0f;
    float region_flux[3] = {};
    float region_peak[3] = {};
    int region_dominant_band[3] = {0, 3, 8};
    for (int band = 0; band < LV_SPECTRAL_FLOW_BANDS; ++band) {
        const float level = clamp_level(band_level[band]);
        energy = std::max(energy, level);

        // Spectral flux measures only newly arriving energy. Dividing the rise
        // by the band's previous level lets a quieter cymbal change compete
        // with a much louder but steady bass line.
        const float rise = std::max(0.0f,
                                    level - flow->previous_band_level[band]);
        const float relative_flux =
            rise / std::max(flow->previous_band_level[band], 0.10f);
        int region = band <= 2 ? 0 : (band <= 7 ? 1 : 2);
        region_flux[region] += relative_flux;
        if (relative_flux > region_peak[region]) {
            region_dominant_band[region] = band;
        }
        region_peak[region] = std::max(region_peak[region], relative_flux);
        flow->previous_band_level[band] = level;
    }

    int winning_region = 0;
    float winning_score = 0.0f;
    for (int region = 0; region < 3; ++region) {
        const auto &definition = spectral_regions[region];
        const float band_count =
            (float)(definition.last_band - definition.first_band + 1);
        const float mean = region_flux[region] / band_count;
        const float score =
            (0.65f * region_peak[region] + 0.35f * mean) *
            definition.event_weight;
        if (score > winning_score) {
            winning_score = score;
            winning_region = region;
        }
    }

    if (winning_score >= spectral_flux_floor) {
        const float selected_hue =
            spectral_band_hues[region_dominant_band[winning_region]];
        flow->target_hue = flow->hue_initialized
                               ? nearest_hue(selected_hue, flow->hue)
                               : selected_hue;
        if (!flow->hue_initialized) {
            flow->hue = flow->target_hue;
            flow->hue_initialized = true;
        }
    } else if (!flow->hue_initialized && energy > 0.0001f) {
        float weighted_hue = 0.0f;
        float weight = 0.0f;
        for (int band = 0; band < LV_SPECTRAL_FLOW_BANDS; ++band) {
            weighted_hue += clamp_level(band_level[band]) *
                            spectral_band_hues[band];
            weight += clamp_level(band_level[band]);
        }
        flow->target_hue = weight > 0.0f ? weighted_hue / weight : 0.0f;
        flow->hue = flow->target_hue;
        flow->hue_initialized = true;
    }

    if (flow->hue_initialized && elapsed_ms > 0) {
        const float blend =
            1.0f - std::exp(-(float)elapsed_ms / spectral_hue_time_constant_ms);
        flow->hue += (flow->target_hue - flow->hue) * blend;
    }

    // One shared peak controls packet brightness without independently
    // normalizing all 12 bands. Rise is immediate; the peak falls slowly
    // enough to retain musical dynamics.
    if (energy > 0.0001f &&
        (reset_analysis || flow->gain_peak <= 0.0001f)) {
        flow->gain_peak = energy;
    } else if (energy > 0.0001f) {
        const float release = elapsed_ms > 0
                                  ? std::pow(0.992f,
                                             (float)elapsed_ms /
                                                 (1000.0f / 60.0f))
                                  : 1.0f;
        flow->gain_peak = std::max(energy, flow->gain_peak * release);
    }

    const float gain = std::max(flow->gain_peak, 0.0001f);
    const float normalized_energy = clamp_level(energy / gain);
    // Preserve dynamics without letting one loud transient make subsequent
    // packets disappear. The gate still controls true silence; an audible
    // packet gets at least 55% source brightness before the global safety cap.
    const float target_value =
        energy > 0.0001f
            ? std::min(255.0f,
                       (0.55f + 0.45f * std::sqrt(normalized_energy)) *
                               255.0f +
                           flow->bass_impulse * 24.0f)
            : 0.0f;
    if (reset_analysis || elapsed_ms == 0) {
        flow->source_value = target_value;
    } else {
        const float time_constant = target_value > flow->source_value
                                        ? spectral_value_attack_ms
                                        : spectral_value_release_ms;
        const float blend =
            1.0f - std::exp(-(float)elapsed_ms / time_constant);
        flow->source_value += (target_value - flow->source_value) * blend;
    }
}

void append_ribbon_sample(lv_spectral_flow_t *flow, uint32_t now_ms) {
    if (flow->history_count > 0 &&
        flow->history[flow->history_head].time_ms == now_ms) {
        flow->history[flow->history_head] = {
            flow->hue, flow->source_value, now_ms};
        return;
    }

    flow->history_head =
        (uint16_t)((flow->history_head + 1) % LV_SPECTRAL_FLOW_HISTORY);
    flow->history[flow->history_head] = {
        flow->hue, flow->source_value, now_ms};
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

float lv_spectral_flow_coordinate(int half, float outward_distance, float row) {
    const float bounded_row = std::clamp(row, 0.0f, 4.0f);
    const float phased_row = half == 1 ? 4.0f - bounded_row : bounded_row;
    return std::max(0.0f, outward_distance + phased_row * spectral_row_phase);
}

void lv_spectral_flow_update(
    lv_spectral_flow_t *flow,
    const float band_level[LV_SPECTRAL_FLOW_BANDS], float bass_transient,
    uint32_t now_ms) {
    const bool reset_history =
        flow->initialized &&
        now_ms - flow->last_frame_ms >= spectral_flow_stale_ms;
    const uint32_t elapsed_ms =
        flow->initialized && !reset_history ? now_ms - flow->last_frame_ms : 0;
    spectral_source_hsv(flow, band_level, bass_transient,
                        !flow->initialized || reset_history, elapsed_ms);

    if (!flow->initialized || reset_history) {
        flow->history_head = 0;
        flow->history_count = 1;
        flow->history[0] = {flow->hue, flow->source_value, now_ms};
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
    hue /= total_weight;
    value /= total_weight;

    const float carrier_period_ms = travel_ms * spectral_carrier_wavelength;
    const float carrier_cycles = coordinate / spectral_carrier_wavelength -
                                 (float)now_ms / carrier_period_ms;
    const float carrier = 0.91f + 0.09f * fast_carrier_cos(carrier_cycles);
    const float distance_decay = fast_distance_decay(coordinate);
    return lv_hsv_to_rgb(color_channel(wrap_hue(hue)), 255,
                         color_channel(value * carrier * distance_decay));
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
