#pragma once

#include <stdint.h>

#include "landerviz/flow_v3.h"
#include "landerviz/flow_v3_attack.h"
#include "landerviz/flow_v3_energy.h"
#include "landerviz/flow_v3_mid.h"
#include "landerviz/flow_v3_texture.h"
#include "landerviz/protocol.h"

typedef void (*lv_flow_v3_service_fn)();

// Compose the accepted Bass B body and Mid Lab hue at one ribbon coordinate.
// This is stateless: both inputs retain their independent, frozen histories.
lv_flow_v3_slice_t lv_flow_v3_combined_sample(
    const lv_flow_v3_t *bass, const lv_flow_v3_mid_t *mids,
    float coordinate, uint32_t now_ms);

// Render one combined slice while matching the perceived lightness of the
// frozen Bass Lab renderer at the same body and keyboard distance.
lv_rgb_t lv_flow_v3_combined_color(lv_flow_v3_slice_t sample,
                                   float coordinate);

void lv_flow_v3_combined_render(
    const lv_flow_v3_t *bass, const lv_flow_v3_mid_t *mids,
    uint32_t now_ms, lv_rgb_t frame[LV_KEY_COUNT]);

// Cap whole-spectrum energy at 22% of the body range. The compositor uses
// this only as a visibility floor; the stored Bass B body remains unchanged.
float lv_flow_v3_energy_floor_body(float energy);

lv_flow_v3_slice_t lv_flow_v3_combined_energy_sample(
    const lv_flow_v3_t *bass, const lv_flow_v3_mid_t *mids,
    const lv_flow_v3_energy_t *energy, float coordinate,
    uint32_t now_ms);

lv_rgb_t lv_flow_v3_combined_energy_color(lv_flow_v3_slice_t sample,
                                          float coordinate);

void lv_flow_v3_combined_energy_render(
    const lv_flow_v3_t *bass, const lv_flow_v3_mid_t *mids,
    const lv_flow_v3_energy_t *energy, uint32_t now_ms,
    lv_rgb_t frame[LV_KEY_COUNT]);

// Accepted Attack integration: preserve the accepted Energy-floor ribbon,
// but continuously tighten Bass B's spatial blur toward its unblurred body.
// Attack cannot create light, change hue, or leave the bounds established by
// the accepted blurred body and its underlying unblurred body.
lv_flow_v3_slice_t lv_flow_v3_combined_energy_attack_sample(
    const lv_flow_v3_t *bass, const lv_flow_v3_mid_t *mids,
    const lv_flow_v3_energy_t *energy,
    const lv_flow_v3_attack_t *attack, float coordinate,
    uint32_t now_ms);

void lv_flow_v3_combined_energy_attack_render(
    const lv_flow_v3_t *bass, const lv_flow_v3_mid_t *mids,
    const lv_flow_v3_energy_t *energy,
    const lv_flow_v3_attack_t *attack, uint32_t now_ms,
    lv_rgb_t frame[LV_KEY_COUNT]);

// Texture stays on the composed slice and can shift its hue by at most 32
// degrees. It cannot alter body, energy, attack, saturation, or lightness.
lv_flow_v3_slice_t lv_flow_v3_combined_energy_attack_texture_sample(
    const lv_flow_v3_t *bass, const lv_flow_v3_mid_t *mids,
    const lv_flow_v3_energy_t *energy,
    const lv_flow_v3_attack_t *attack,
    const lv_flow_v3_texture_t *texture, float coordinate,
    uint32_t now_ms);

void lv_flow_v3_combined_energy_attack_texture_render(
    const lv_flow_v3_t *bass, const lv_flow_v3_mid_t *mids,
    const lv_flow_v3_energy_t *energy,
    const lv_flow_v3_attack_t *attack,
    const lv_flow_v3_texture_t *texture, uint32_t now_ms,
    lv_rgb_t frame[LV_KEY_COUNT],
    lv_flow_v3_service_fn service = nullptr);
