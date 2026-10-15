#include "landerviz/audio_analysis.h"

#include <cmath>

static void fft_radix2(float *re, float *im, int n) {
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            float value = re[i];
            re[i] = re[j];
            re[j] = value;
            value = im[i];
            im[i] = im[j];
            im[j] = value;
        }
    }

    for (int len = 2; len <= n; len <<= 1) {
        const float angle = -2.0f * 3.14159265f / len;
        const float step_re = std::cos(angle);
        const float step_im = std::sin(angle);
        for (int i = 0; i < n; i += len) {
            float twiddle_re = 1.0f;
            float twiddle_im = 0.0f;
            for (int k = 0; k < len / 2; ++k) {
                const int upper = i + k + len / 2;
                const float product_re = twiddle_re * re[upper] - twiddle_im * im[upper];
                const float product_im = twiddle_re * im[upper] + twiddle_im * re[upper];
                re[upper] = re[i + k] - product_re;
                im[upper] = im[i + k] - product_im;
                re[i + k] += product_re;
                im[i + k] += product_im;

                const float next_re = twiddle_re * step_re - twiddle_im * step_im;
                twiddle_im = twiddle_re * step_im + twiddle_im * step_re;
                twiddle_re = next_re;
            }
        }
    }
}

float lv_perceptual_level(float linear_level) {
    constexpr float floor_db = -48.0f;
    constexpr float floor_level = 0.0039810717f;  // 10 ^ (floor_db / 20)

    if (linear_level <= floor_level) return 0.0f;
    if (linear_level >= 1.0f) return 1.0f;

    const float level_db = 20.0f * std::log10(linear_level);
    return (level_db - floor_db) / -floor_db;
}

float lv_meter_level(float linear_level) {
    constexpr float gate_floor_db = -30.0f;
    constexpr float gate_open_db = -24.0f;
    constexpr float gate_floor_level = 0.0316227766f;
    constexpr float gate_open_level = 0.0630957344f;

    if (linear_level <= gate_floor_level) return 0.0f;

    const float perceptual = lv_perceptual_level(linear_level);
    if (linear_level >= gate_open_level) return perceptual;

    const float level_db = 20.0f * std::log10(linear_level);
    const float position = (level_db - gate_floor_db) / (gate_open_db - gate_floor_db);
    const float soft_knee = position * position * (3.0f - 2.0f * position);
    return perceptual * soft_knee;
}

bool lv_audio_activity_update(lv_audio_activity_t *activity,
                              bool above_noise_gate, uint32_t now_ms) {
    activity->recent_frames =
        (uint8_t)(((activity->recent_frames << 1) |
                   (above_noise_gate ? 1u : 0u)) &
                  0x0Fu);
    if (activity->frames_seen < 4) ++activity->frames_seen;

    uint8_t confirmed_frames = 0;
    uint8_t recent = activity->recent_frames;
    for (int frame = 0; frame < 4; ++frame) {
        confirmed_frames += recent & 1u;
        recent >>= 1;
    }

    if (activity->frames_seen == 4 && confirmed_frames >= 3) {
        activity->active = true;
        activity->last_confirmed_ms = now_ms;
    } else if (activity->active &&
               now_ms - activity->last_confirmed_ms >= 5000) {
        activity->active = false;
    }
    return activity->active;
}

int lv_peak_hold_update(lv_peak_hold_t *peak, int live_level, uint32_t now_ms) {
    constexpr uint32_t hold_ms = 750;
    constexpr uint32_t release_ms = 3000;
    constexpr int meter_max = 80;

    if (live_level < 0) live_level = 0;
    if (live_level > meter_max) live_level = meter_max;

    if (live_level >= peak->displayed_level) {
        peak->displayed_level = live_level;
        peak->held_level = live_level;
        peak->held_at_ms = now_ms;
        return peak->displayed_level;
    }

    const uint32_t age_ms = now_ms - peak->held_at_ms;
    if (age_ms <= hold_ms) return peak->displayed_level;

    const uint32_t falling_ms = age_ms - hold_ms;
    int released_level = 0;
    if (falling_ms < release_ms) {
        const int drop = (int)(falling_ms * meter_max / release_ms);
        released_level = peak->held_level - drop;
        if (released_level < 0) released_level = 0;
    }
    peak->displayed_level = released_level > live_level ? released_level : live_level;
    return peak->displayed_level;
}

void lv_analyze_channel(lv_channel_analysis_t *analysis, const uint16_t *samples,
                        float input_gain) {
    static const uint8_t band_start[LV_BAND_COUNT] = {1, 2, 3, 5, 7, 10, 14, 20, 28, 40, 57, 80};
    static const uint8_t band_end[LV_BAND_COUNT] = {1, 2, 4, 6, 9, 13, 19, 27, 39, 56, 79, 128};
    // Analysis is called sequentially for left and right. Keep the FFT scratch
    // space out of the RP2040's small stack and reuse it between channels.
    static float re[LV_FFT_N];
    static float im[LV_FFT_N];
    static float magnitude[LV_FFT_N / 2];
    float sum_sq = 0.0f;
    float frame_mean = 0.0f;

    if (analysis->bass_avg == 0.0f) analysis->bass_avg = 0.05f;

    for (int i = 0; i < LV_FFT_N; ++i) frame_mean += samples[i];
    frame_mean /= LV_FFT_N;

    for (int i = 0; i < LV_FFT_N; ++i) {
        const float sample =
            (((float)samples[i] - frame_mean) / LV_ADC_MID) * input_gain;
        const float window = 0.5f - 0.5f * std::cos(2.0f * 3.14159265f * i / LV_FFT_N);
        sum_sq += sample * sample;
        re[i] = sample * window;
        im[i] = 0.0f;
    }

    fft_radix2(re, im, LV_FFT_N);
    for (int i = 0; i < LV_FFT_N / 2; ++i) {
        magnitude[i] = std::sqrt(re[i] * re[i] + im[i] * im[i]) / (LV_FFT_N / 4);
    }

    for (int band = 0; band < LV_BAND_COUNT; ++band) {
        float peak = 0.0f;
        for (int bin = band_start[band]; bin <= band_end[band] && bin < LV_FFT_N / 2; ++bin) {
            if (magnitude[bin] > peak) peak = magnitude[bin];
        }
        if (peak > analysis->band_level[band]) analysis->band_level[band] = peak;
        else analysis->band_level[band] += (peak - analysis->band_level[band]) * 0.25f;
        if (analysis->band_level[band] > 1.0f) analysis->band_level[band] = 1.0f;
    }

    const float bass = (analysis->band_level[0] + analysis->band_level[1] +
                        analysis->band_level[2]) / 3.0f;
    if (bass > analysis->bass_avg * 1.25f && bass > 0.04f && bass > analysis->bass_env) {
        analysis->bass_env = bass;
    }
    analysis->bass_avg += (bass - analysis->bass_avg) * 0.02f;
    analysis->bass_env *= 0.96f;
    analysis->rms_level = std::sqrt(sum_sq / LV_FFT_N);
}
