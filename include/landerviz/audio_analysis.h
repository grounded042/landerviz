#pragma once

#include <stdint.h>

enum {
    LV_AUDIO_CHANNEL_COUNT = 2,
    LV_FFT_N = 256,
    LV_BAND_COUNT = 12,
    LV_MEL_BAND_COUNT = 12,
    LV_ADC_MID = 2048,
    // Capture every window so frequency bins remain unchanged, but run the
    // stereo FFT on every other window: about 30.5 analyses per second.
    LV_ANALYSIS_DECIMATION = 2,
};

typedef struct {
    float band_level[LV_BAND_COUNT];
    // Overlapping triangular bands spaced on the perceptual mel scale.
    // FLOW consumes these independently from the legacy meter bands above.
    float mel_level[LV_MEL_BAND_COUNT];
    float bass_env;
    float bass_avg;
    float rms_level;
} lv_channel_analysis_t;

typedef struct {
    int displayed_level;
    int held_level;
    uint32_t held_at_ms;
} lv_peak_hold_t;

typedef struct {
    uint8_t recent_frames;
    uint8_t frames_seen;
    bool active;
    uint32_t last_confirmed_ms;
} lv_audio_activity_t;

void lv_analyze_channel(lv_channel_analysis_t *analysis, const uint16_t *samples,
                        float input_gain);

// Map a normalized linear amplitude onto a 0..1 display level using a
// logarithmic curve. Signals at or below -48 dBFS are treated as silence.
float lv_perceptual_level(float linear_level);

// Apply a -30..-24 dBFS soft noise gate before using the perceptual curve.
// Intended for silence detection and meters, not keyboard visualizations.
float lv_meter_level(float linear_level);

// Collapse the 12 logarithmic FFT bands into eight 0..8 character-cell
// heights for the LCD spectrum display.
void lv_lcd_spectrum_bins(const float band_level[LV_BAND_COUNT],
                          uint8_t bins[8]);

// Treat sound as real only when at least three of the latest four analysis
// frames exceed the noise gate. Once active, return to idle after five seconds
// without another confirmed window.
bool lv_audio_activity_update(lv_audio_activity_t *activity,
                              bool above_noise_gate, uint32_t now_ms);

// Hold a new meter peak for 750 ms, then release a full-scale peak over 3 s.
int lv_peak_hold_update(lv_peak_hold_t *peak, int live_level, uint32_t now_ms);
