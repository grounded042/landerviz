#pragma once

#include <stdint.h>

#include "landerviz/audio_analysis.h"
#include "landerviz/effects.h"
#include "landerviz/protocol.h"

enum class lv_viz_mode_t : uint8_t {
    FLOW = 0,
    SPECTRUM = 1,
    PULSE = 2,
    VU = 3,
    IDLE = 4,
    COUNT = 5,
};

struct lv_visualizer_t {
    lv_spectral_flow_t flow[LV_AUDIO_CHANNEL_COUNT];
    lv_kick_detector_t flow_kick;
    float spectrum_peaks[LV_AUDIO_CHANNEL_COUNT][6];
    float vu_levels[LV_AUDIO_CHANNEL_COUNT];
    uint32_t last_pulse_hue_ms;
    uint8_t pulse_hue;
    bool flow_kick_packets_enabled;
};

const char *lv_viz_mode_name(lv_viz_mode_t mode);

// Override FLOW travel time for preview tuning. Zero restores the firmware
// default. The value returned is always the effective nonzero time.
void lv_visualizer_set_flow_travel_ms(lv_visualizer_t *visualizer,
                                      uint16_t travel_ms);
uint16_t lv_visualizer_flow_travel_ms(const lv_visualizer_t *visualizer);

// Kick packets remain opt-in while their detector is calibrated. The base
// FLOW ribbon is unaffected.
void lv_visualizer_set_flow_kick_packets(lv_visualizer_t *visualizer,
                                         bool enabled);

// Map two already-populated FLOW ribbons onto the physical Moonlander keys.
// This keeps preview-only signal engines on the same spatial renderer as v1.
void lv_visualizer_render_flow_ribbons(
    const lv_spectral_flow_t flow[LV_AUDIO_CHANNEL_COUNT], uint32_t now_ms,
    lv_rgb_t frame[LV_KEY_COUNT]);

// Render all 72 Moonlander LEDs. When audio_present is false, every mode except
// IDLE renders the aurora so preview and firmware share identical quiet-state
// behavior.
void lv_visualizer_render(
    lv_visualizer_t *visualizer, lv_viz_mode_t mode,
    const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT],
    bool audio_present, uint32_t now_ms, lv_rgb_t frame[LV_KEY_COUNT]);
