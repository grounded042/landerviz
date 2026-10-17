#pragma once

#ifndef LANDERVIZ_PREVIEW_FLOW_V3
#error "FLOW v3 is preview-only until its behavior is accepted"
#endif

#include <stdint.h>

#include "landerviz/flow_v3.h"
#include "landerviz/flow_v3_energy.h"
#include "landerviz/flow_v3_mid.h"
#include "landerviz/protocol.h"

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
