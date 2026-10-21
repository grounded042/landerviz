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

typedef struct {
    float gain;
    float noise_floor;
    uint32_t last_update_ms;
    uint8_t frames_above_gate;
    uint8_t frames_below_gate;
    bool initialized;
    bool signal_seen;
    bool gate_open;
} lv_audio_normalizer_t;

typedef struct {
    float rms_sum;
    float rms_sum_sq;
    float rms_floor;
    float band_sum[LV_BAND_COUNT];
    float band_sum_sq[LV_BAND_COUNT];
    float band_floor[LV_BAND_COUNT];
    float mel_sum[LV_MEL_BAND_COUNT];
    float mel_sum_sq[LV_MEL_BAND_COUNT];
    float mel_floor[LV_MEL_BAND_COUNT];
    uint16_t frames_seen;
    uint16_t frames_collected;
    bool valid;
} lv_audio_calibration_t;

// Precompute the fixed Hann window before USB and timer service begins.
void lv_audio_analysis_init();

// Average a captured stereo window into one mono window while retaining each
// channel's independent RMS and peak amplitudes. Both measurements include
// input_gain. This is the deliberate time-domain mono input used by FLOW before
// its single FFT.
void lv_prepare_mono_analysis(
    const uint16_t stereo[LV_AUDIO_CHANNEL_COUNT][LV_FFT_N],
    uint16_t mono[LV_FFT_N], float input_gain,
    float rms_level[LV_AUDIO_CHANNEL_COUNT],
    float peak_level[LV_AUDIO_CHANNEL_COUNT]);

void lv_analyze_channel(lv_channel_analysis_t *analysis, const uint16_t *samples,
                        float input_gain);

// Map a normalized linear amplitude onto a 0..1 display level using a
// logarithmic curve. Signals at or below -48 dBFS are treated as silence.
float lv_perceptual_level(float linear_level);

// Apply a -30..-24 dBFS soft noise gate before using the perceptual curve.
// Intended for silence detection and meters, not keyboard visualizations.
float lv_meter_level(float linear_level);

// Convert a linear amplitude to the nearest whole decibel relative to ADC
// full scale without a runtime logarithm. Values below -72 dBFS use -72.
int lv_level_dbfs(float linear_level);

// Track one shared program level after the fixed input compensation. The
// calibrated_spectral_signal hint lets quiet, frequency-localized music hold
// the gate open even when broadband RMS is near the noise floor. Returns a
// bounded gain for the FFT and exposes the noise-relative gate through state.
float lv_audio_normalizer_update(lv_audio_normalizer_t *state,
                                 float linear_rms,
                                 bool calibrated_spectral_signal,
                                 uint32_t now_ms);

// Collect a deliberate silence window after allowing the analyzer to settle.
// Finalization rejects strongly varying input and freezes broadband plus
// per-band floors for the rest of the session.
void lv_audio_calibration_reset(lv_audio_calibration_t *calibration);
void lv_audio_calibration_add(lv_audio_calibration_t *calibration,
                              float linear_rms,
                              const lv_channel_analysis_t *analysis);
bool lv_audio_calibration_finish(lv_audio_calibration_t *calibration);

// Detect quiet music from frequency-localized energy above the calibrated
// mel-band noise floors. Requiring more than one overlapping band rejects an
// isolated spectral spike while retaining narrow musical tones.
bool lv_audio_calibration_signal_present(
    const lv_audio_calibration_t *calibration,
    const lv_channel_analysis_t *analysis);

// Subtract calibrated noise power, then apply adaptive gain before FLOW
// feature extraction. Without valid calibration this applies gain only.
float lv_audio_calibrated_level(
    const lv_audio_calibration_t *calibration,
    float linear_level, float gain);
void lv_audio_calibration_apply(
    const lv_audio_calibration_t *calibration,
    lv_channel_analysis_t *analysis, float gain);

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
