#pragma once

#include <stdint.h>

enum {
    LV_AUDIO_CHANNEL_COUNT = 2,
    LV_FFT_N = 256,
    LV_BAND_COUNT = 12,
    LV_ADC_MID = 2048,
};

typedef struct {
    float band_level[LV_BAND_COUNT];
    float bass_env;
    float bass_avg;
    float rms_level;
} lv_channel_analysis_t;

typedef struct {
    int displayed_level;
    int held_level;
    uint32_t held_at_ms;
} lv_peak_hold_t;

void lv_analyze_channel(lv_channel_analysis_t *analysis, const uint16_t *samples,
                        float input_gain);

// Map a normalized linear amplitude onto a 0..1 display level using a
// logarithmic curve. Signals at or below -48 dBFS are treated as silence.
float lv_perceptual_level(float linear_level);

// Apply a -30..-24 dBFS soft noise gate before using the perceptual curve.
// Intended for silence detection and meters, not keyboard visualizations.
float lv_meter_level(float linear_level);

// Hold a new meter peak for 750 ms, then release a full-scale peak over 3 s.
int lv_peak_hold_update(lv_peak_hold_t *peak, int live_level, uint32_t now_ms);
