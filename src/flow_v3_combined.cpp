#include "landerviz/flow_v3_combined.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "landerviz/effects.h"
#include "landerviz/moonlander_map.h"

namespace {

constexpr float bass_lab_hue = 195.0f;
constexpr float bass_distance_fade = 0.975f;
constexpr float energy_floor_body_cap = 0.22f;
constexpr float attack_blur_tightening = 0.75f;
constexpr float texture_hue_influence = 32.0f;

float nearest_hue(float hue, float reference) {
    while (hue - reference > 180.0f) hue -= 360.0f;
    while (hue - reference < -180.0f) hue += 360.0f;
    return hue;
}

lv_flow_v3_slice_t blurred_bass_sample(
    const lv_flow_v3_t *bass, float coordinate, uint32_t now_ms) {
    constexpr float offset = 0.22f;
    const auto before = lv_flow_v3_sample(
        bass, std::max(0.0f, coordinate - offset), now_ms);
    const auto center = lv_flow_v3_sample(bass, coordinate, now_ms);
    const auto after = lv_flow_v3_sample(
        bass, coordinate + offset, now_ms);
    lv_flow_v3_slice_t result = center;
    result.energy = before.energy * 0.25f + center.energy * 0.50f +
                    after.energy * 0.25f;
    result.body = before.body * 0.25f + center.body * 0.50f +
                  after.body * 0.25f;
    result.attack = 0.0f;
    return result;
}

void set_key(lv_rgb_t frame[LV_KEY_COUNT], int key, lv_rgb_t color) {
    if (key >= 0 && key < LV_KEY_COUNT) frame[key] = color;
}

lv_rgb_t color_at(const lv_flow_v3_t *bass,
                  const lv_flow_v3_mid_t *mids, float coordinate,
                  uint32_t now_ms) {
    return lv_flow_v3_combined_color(
        lv_flow_v3_combined_sample(bass, mids, coordinate, now_ms),
        coordinate);
}

lv_rgb_t energy_color_at(const lv_flow_v3_t *bass,
                         const lv_flow_v3_mid_t *mids,
                         const lv_flow_v3_energy_t *energy,
                         float coordinate, uint32_t now_ms) {
    return lv_flow_v3_combined_energy_color(
        lv_flow_v3_combined_energy_sample(
            bass, mids, energy, coordinate, now_ms),
        coordinate);
}

lv_rgb_t energy_attack_color_at(
    const lv_flow_v3_t *bass, const lv_flow_v3_mid_t *mids,
    const lv_flow_v3_energy_t *energy,
    const lv_flow_v3_attack_t *attack, float coordinate,
    uint32_t now_ms) {
    return lv_flow_v3_combined_energy_color(
        lv_flow_v3_combined_energy_attack_sample(
            bass, mids, energy, attack, coordinate, now_ms),
        coordinate);
}

lv_rgb_t energy_attack_texture_color_at(
    const lv_flow_v3_t *bass, const lv_flow_v3_mid_t *mids,
    const lv_flow_v3_energy_t *energy,
    const lv_flow_v3_attack_t *attack,
    const lv_flow_v3_texture_t *texture, float coordinate,
    uint32_t now_ms) {
    return lv_flow_v3_combined_energy_color(
        lv_flow_v3_combined_energy_attack_texture_sample(
            bass, mids, energy, attack, texture, coordinate, now_ms),
        coordinate);
}

}  // namespace

lv_flow_v3_slice_t lv_flow_v3_combined_sample(
    const lv_flow_v3_t *bass, const lv_flow_v3_mid_t *mids,
    float coordinate, uint32_t now_ms) {
    auto combined = blurred_bass_sample(bass, coordinate, now_ms);
    const auto mid = lv_flow_v3_mid_sample(mids, coordinate, now_ms);
    const float mid_presence = std::clamp(mid.body, 0.0f, 1.0f);
    const float nearby_mid_hue = nearest_hue(mid.hue, combined.hue);
    combined.hue += (nearby_mid_hue - combined.hue) * mid_presence;
    combined.saturation = 255.0f;
    combined.energy = combined.body;
    combined.attack = 0.0f;
    combined.texture = 0.0f;
    return combined;
}

lv_rgb_t lv_flow_v3_combined_color(lv_flow_v3_slice_t sample,
                                   float coordinate) {
    const float body = std::clamp(sample.body, 0.0f, 1.0f);
    if (body <= 0.001f) return {};
    const float distance_fade = std::pow(
        bass_distance_fade, std::max(0.0f, coordinate));
    const float value = (0.12f + 0.88f * body) * distance_fade;
    const auto bass_reference = lv_hsv_to_rgb(
        (uint8_t)std::lround(bass_lab_hue * 255.0f / 360.0f), 255,
        (uint8_t)std::clamp(std::lround(value * 255.0f), 0l, 255l));
    const float lightness = lv_flow_v3_color_lightness(bass_reference);
    return lv_flow_v3_vivid_color(sample.hue, lightness);
}

void lv_flow_v3_combined_render(
    const lv_flow_v3_t *bass, const lv_flow_v3_mid_t *mids,
    uint32_t now_ms, lv_rgb_t frame[LV_KEY_COUNT]) {
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
                        color_at(bass, mids, coordinate, now_ms));
            }
        }
        for (int key = 0; key < 3; ++key) {
            const float coordinate = lv_spectral_flow_coordinate(
                half, 0.0f, (float)(key + 1));
            set_key(frame, lv_utility_key_at(half, key),
                    color_at(bass, mids, coordinate, now_ms));
        }
        constexpr float thumb_distance[4] = {1.25f, 0.85f, 0.45f, 0.0f};
        for (int key = 0; key < 4; ++key) {
            const float coordinate = lv_spectral_flow_coordinate(
                half, thumb_distance[key], 3.2f + key * 0.2f);
            set_key(frame, lv_thumb_key_at(half, key),
                    color_at(bass, mids, coordinate, now_ms));
        }
    }
}

float lv_flow_v3_energy_floor_body(float energy) {
    return energy_floor_body_cap * std::clamp(energy, 0.0f, 1.0f);
}

lv_flow_v3_slice_t lv_flow_v3_combined_energy_sample(
    const lv_flow_v3_t *bass, const lv_flow_v3_mid_t *mids,
    const lv_flow_v3_energy_t *energy, float coordinate,
    uint32_t now_ms) {
    auto combined = lv_flow_v3_combined_sample(
        bass, mids, coordinate, now_ms);
    combined.energy = lv_flow_v3_energy_sample(
        energy, coordinate, now_ms).energy;
    return combined;
}

lv_rgb_t lv_flow_v3_combined_energy_color(lv_flow_v3_slice_t sample,
                                          float coordinate) {
    sample.body = std::max(
        sample.body, lv_flow_v3_energy_floor_body(sample.energy));
    return lv_flow_v3_combined_color(sample, coordinate);
}

void lv_flow_v3_combined_energy_render(
    const lv_flow_v3_t *bass, const lv_flow_v3_mid_t *mids,
    const lv_flow_v3_energy_t *energy, uint32_t now_ms,
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
                        energy_color_at(bass, mids, energy, coordinate,
                                        now_ms));
            }
        }
        for (int key = 0; key < 3; ++key) {
            const float coordinate = lv_spectral_flow_coordinate(
                half, 0.0f, (float)(key + 1));
            set_key(frame, lv_utility_key_at(half, key),
                    energy_color_at(bass, mids, energy, coordinate,
                                    now_ms));
        }
        constexpr float thumb_distance[4] = {1.25f, 0.85f, 0.45f, 0.0f};
        for (int key = 0; key < 4; ++key) {
            const float coordinate = lv_spectral_flow_coordinate(
                half, thumb_distance[key], 3.2f + key * 0.2f);
            set_key(frame, lv_thumb_key_at(half, key),
                    energy_color_at(bass, mids, energy, coordinate,
                                    now_ms));
        }
    }
}

lv_flow_v3_slice_t lv_flow_v3_combined_energy_attack_sample(
    const lv_flow_v3_t *bass, const lv_flow_v3_mid_t *mids,
    const lv_flow_v3_energy_t *energy,
    const lv_flow_v3_attack_t *attack, float coordinate,
    uint32_t now_ms) {
    auto combined = lv_flow_v3_combined_energy_sample(
        bass, mids, energy, coordinate, now_ms);
    const auto articulation = lv_flow_v3_attack_sample(
        attack, coordinate, now_ms);
    combined.attack = std::clamp(articulation.attack, 0.0f, 1.0f);
    const float tightening = attack_blur_tightening * combined.attack;
    const float unblurred_body = std::clamp(
        lv_flow_v3_sample(bass, coordinate, now_ms).body, 0.0f, 1.0f);
    combined.body += (unblurred_body - combined.body) * tightening;
    return combined;
}

void lv_flow_v3_combined_energy_attack_render(
    const lv_flow_v3_t *bass, const lv_flow_v3_mid_t *mids,
    const lv_flow_v3_energy_t *energy,
    const lv_flow_v3_attack_t *attack, uint32_t now_ms,
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
                        energy_attack_color_at(
                            bass, mids, energy, attack, coordinate,
                            now_ms));
            }
        }
        for (int key = 0; key < 3; ++key) {
            const float coordinate = lv_spectral_flow_coordinate(
                half, 0.0f, (float)(key + 1));
            set_key(frame, lv_utility_key_at(half, key),
                    energy_attack_color_at(
                        bass, mids, energy, attack, coordinate, now_ms));
        }
        constexpr float thumb_distance[4] = {1.25f, 0.85f, 0.45f, 0.0f};
        for (int key = 0; key < 4; ++key) {
            const float coordinate = lv_spectral_flow_coordinate(
                half, thumb_distance[key], 3.2f + key * 0.2f);
            set_key(frame, lv_thumb_key_at(half, key),
                    energy_attack_color_at(
                        bass, mids, energy, attack, coordinate, now_ms));
        }
    }
}

lv_flow_v3_slice_t lv_flow_v3_combined_energy_attack_texture_sample(
    const lv_flow_v3_t *bass, const lv_flow_v3_mid_t *mids,
    const lv_flow_v3_energy_t *energy,
    const lv_flow_v3_attack_t *attack,
    const lv_flow_v3_texture_t *texture, float coordinate,
    uint32_t now_ms) {
    auto combined = lv_flow_v3_combined_energy_attack_sample(
        bass, mids, energy, attack, coordinate, now_ms);
    combined.texture = std::clamp(
        lv_flow_v3_texture_sample(texture, coordinate, now_ms).texture,
        0.0f, 1.0f);
    combined.hue += texture_hue_influence * combined.texture;
    return combined;
}

void lv_flow_v3_combined_energy_attack_texture_render(
    const lv_flow_v3_t *bass, const lv_flow_v3_mid_t *mids,
    const lv_flow_v3_energy_t *energy,
    const lv_flow_v3_attack_t *attack,
    const lv_flow_v3_texture_t *texture, uint32_t now_ms,
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
                        energy_attack_texture_color_at(
                            bass, mids, energy, attack, texture,
                            coordinate, now_ms));
            }
        }
        for (int key = 0; key < 3; ++key) {
            const float coordinate = lv_spectral_flow_coordinate(
                half, 0.0f, (float)(key + 1));
            set_key(frame, lv_utility_key_at(half, key),
                    energy_attack_texture_color_at(
                        bass, mids, energy, attack, texture,
                        coordinate, now_ms));
        }
        constexpr float thumb_distance[4] = {1.25f, 0.85f, 0.45f, 0.0f};
        for (int key = 0; key < 4; ++key) {
            const float coordinate = lv_spectral_flow_coordinate(
                half, thumb_distance[key], 3.2f + key * 0.2f);
            set_key(frame, lv_thumb_key_at(half, key),
                    energy_attack_texture_color_at(
                        bass, mids, energy, attack, texture,
                        coordinate, now_ms));
        }
    }
}
