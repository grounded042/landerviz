#include "landerviz/audio_analysis.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#if defined(LANDERVIZ_USE_CMSIS_DSP_Q15)
#include "dsp/transform_functions.h"
#endif

namespace {

#if defined(LANDERVIZ_USE_CMSIS_DSP_Q15)
q15_t hann_window[LV_FFT_N];
arm_rfft_instance_q15 rfft;
#else
float hann_window[LV_FFT_N];
#endif
bool window_initialized = false;

#if !defined(LANDERVIZ_USE_CMSIS_DSP_Q15)
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

    static constexpr float step_re[] = {
        -1.0f, 0.0f, 0.707106781f, 0.923879533f,
        0.980785280f, 0.995184727f, 0.998795456f, 0.999698819f,
    };
    static constexpr float step_im[] = {
        0.0f, -1.0f, -0.707106781f, -0.382683432f,
        -0.195090322f, -0.098017140f, -0.049067674f, -0.024541229f,
    };
    int stage = 0;
    for (int len = 2; len <= n; len <<= 1, ++stage) {
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

                const float next_re = twiddle_re * step_re[stage] -
                                      twiddle_im * step_im[stage];
                twiddle_im = twiddle_re * step_im[stage] +
                             twiddle_im * step_re[stage];
                twiddle_re = next_re;
            }
        }
    }
}
#endif

}  // namespace

void lv_audio_analysis_init() {
    if (window_initialized) return;
    for (int i = 0; i < LV_FFT_N; ++i) {
        const float window =
            0.5f - 0.5f * std::cos(2.0f * 3.14159265f * i / LV_FFT_N);
#if defined(LANDERVIZ_USE_CMSIS_DSP_Q15)
        hann_window[i] = (q15_t)std::lround(window * 32767.0f);
#else
        hann_window[i] = window;
#endif
    }
#if defined(LANDERVIZ_USE_CMSIS_DSP_Q15)
    (void)arm_rfft_init_256_q15(&rfft, 0, 1);
#endif
    window_initialized = true;
}

void lv_prepare_mono_analysis(
    const uint16_t stereo[LV_AUDIO_CHANNEL_COUNT][LV_FFT_N],
    uint16_t mono[LV_FFT_N], float input_gain,
    float rms_level[LV_AUDIO_CHANNEL_COUNT],
    float peak_level[LV_AUDIO_CHANNEL_COUNT]) {
    float mean[LV_AUDIO_CHANNEL_COUNT]{};
    for (int sample = 0; sample < LV_FFT_N; ++sample) {
        for (int channel = 0; channel < LV_AUDIO_CHANNEL_COUNT; ++channel) {
            mean[channel] += stereo[channel][sample];
        }
        mono[sample] = (uint16_t)(
            ((uint32_t)stereo[0][sample] + stereo[1][sample] + 1u) / 2u);
    }
    for (float &channel_mean : mean) channel_mean /= LV_FFT_N;

    float sum_sq[LV_AUDIO_CHANNEL_COUNT]{};
    for (int channel = 0; channel < LV_AUDIO_CHANNEL_COUNT; ++channel) {
        peak_level[channel] = 0.0f;
    }
    for (int sample = 0; sample < LV_FFT_N; ++sample) {
        for (int channel = 0; channel < LV_AUDIO_CHANNEL_COUNT; ++channel) {
            const float value =
                (((float)stereo[channel][sample] - mean[channel]) /
                 LV_ADC_MID) * input_gain;
            sum_sq[channel] += value * value;
            peak_level[channel] = std::max(
                peak_level[channel], std::fabs(value));
        }
    }
    for (int channel = 0; channel < LV_AUDIO_CHANNEL_COUNT; ++channel) {
        rms_level[channel] = std::sqrt(sum_sq[channel] / LV_FFT_N);
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

int lv_level_dbfs(float linear_level) {
    constexpr int floor_db = -72;
    constexpr float one_db_ratio = 0.891250938f;
    float half_db_threshold = 0.944060876f;
    for (int db = 0; db > floor_db; --db) {
        if (linear_level >= half_db_threshold) return db;
        half_db_threshold *= one_db_ratio;
    }
    return floor_db;
}

float lv_audio_normalizer_update(lv_audio_normalizer_t *state,
                                 float linear_rms,
                                 bool calibrated_spectral_signal,
                                 uint32_t now_ms) {
    constexpr float initial_noise_floor = 0.025f;
    constexpr float gate_open_ratio = 1.18f;
    constexpr float gate_close_ratio = 1.08f;
    constexpr float gate_open_margin = 0.003f;
    constexpr float gate_close_margin = 0.001f;
    constexpr float target_rms = 0.20f;
    constexpr float minimum_gain = 0.50f;
    constexpr float maximum_gain = 6.30f;
    constexpr float gain_reduce_ms = 1200.0f;
    constexpr float gain_recover_ms = 6000.0f;
    constexpr float silence_return_ms = 10000.0f;

    linear_rms = std::max(0.0f, linear_rms);
    if (!state->initialized) {
        state->gain = 1.0f;
        if (state->noise_floor <= 0.0f) {
            state->noise_floor = initial_noise_floor;
        }
        state->last_update_ms = now_ms;
        state->initialized = true;
    }

    const uint32_t elapsed_ms = now_ms - state->last_update_ms;
    const auto response = [elapsed_ms](float time_constant_ms) {
        return elapsed_ms > 0
                   ? 1.0f - std::exp(-(float)elapsed_ms / time_constant_ms)
                   : 0.0f;
    };

    const float open_threshold = std::max(
        state->noise_floor * gate_open_ratio,
        state->noise_floor + gate_open_margin);
    const float close_threshold = std::max(
        state->noise_floor * gate_close_ratio,
        state->noise_floor + gate_close_margin);
    constexpr uint8_t gate_open_confirmation_frames = 3;
    constexpr uint8_t gate_close_confirmation_frames = 15;
    if (state->gate_open) {
        state->frames_above_gate = 0;
        if (linear_rms < close_threshold &&
            !calibrated_spectral_signal) {
            state->frames_below_gate = std::min<uint8_t>(
                (uint8_t)(state->frames_below_gate + 1),
                gate_close_confirmation_frames);
            if (state->frames_below_gate >=
                gate_close_confirmation_frames) {
                state->gate_open = false;
                state->frames_below_gate = 0;
            }
        } else {
            state->frames_below_gate = 0;
        }
    } else {
        state->frames_below_gate = 0;
        if (linear_rms >= open_threshold ||
            calibrated_spectral_signal) {
            state->frames_above_gate = std::min<uint8_t>(
                (uint8_t)(state->frames_above_gate + 1),
                gate_open_confirmation_frames);
            if (state->frames_above_gate >=
                gate_open_confirmation_frames) {
                state->gate_open = true;
                state->frames_above_gate = 0;
            }
        } else {
            state->frames_above_gate = 0;
        }
    }

    if (!state->gate_open) {
        state->gain +=
            (1.0f - state->gain) * response(silence_return_ms);
        state->last_update_ms = now_ms;
        return state->gain;
    }

    const float signal_rms = std::sqrt(std::max(
        0.0f, linear_rms * linear_rms -
                  state->noise_floor * state->noise_floor));
    const float target_gain = std::clamp(
        target_rms / std::max(signal_rms, 0.0001f),
        minimum_gain, maximum_gain);
    if (!state->signal_seen) {
        state->gain = target_gain;
        state->signal_seen = true;
    } else {
        const float time_constant = target_gain < state->gain
                                        ? gain_reduce_ms
                                        : gain_recover_ms;
        state->gain +=
            (target_gain - state->gain) * response(time_constant);
    }
    state->gain = std::clamp(state->gain, minimum_gain, maximum_gain);
    state->last_update_ms = now_ms;
    return state->gain;
}

bool lv_audio_calibration_signal_present(
    const lv_audio_calibration_t *calibration,
    const lv_channel_analysis_t *analysis) {
    if (!calibration->valid) return false;

    constexpr float minimum_clean_level = 0.001f;
    constexpr float clean_to_noise_ratio = 0.50f;
    constexpr int required_bands = 2;
    int bands_above_floor = 0;
    for (int band = 0; band < LV_MEL_BAND_COUNT; ++band) {
        const float level = std::max(0.0f, analysis->mel_level[band]);
        const float floor = std::max(0.0f, calibration->mel_floor[band]);
        const float clean_power = std::max(
            0.0f, level * level - floor * floor);
        const float clean_level = std::sqrt(clean_power);
        const float threshold = std::max(
            minimum_clean_level, floor * clean_to_noise_ratio);
        if (clean_level > threshold &&
            ++bands_above_floor >= required_bands) {
            return true;
        }
    }
    return false;
}

void lv_audio_calibration_reset(lv_audio_calibration_t *calibration) {
    std::memset(calibration, 0, sizeof(*calibration));
}

void lv_audio_calibration_add(lv_audio_calibration_t *calibration,
                              float linear_rms,
                              const lv_channel_analysis_t *analysis) {
    constexpr uint16_t warmup_frames = 15;
    ++calibration->frames_seen;
    if (calibration->frames_seen <= warmup_frames) return;

    linear_rms = std::max(0.0f, linear_rms);
    calibration->rms_sum += linear_rms;
    calibration->rms_sum_sq += linear_rms * linear_rms;
    for (int band = 0; band < LV_BAND_COUNT; ++band) {
        const float level = std::max(0.0f, analysis->band_level[band]);
        calibration->band_sum[band] += level;
        calibration->band_sum_sq[band] += level * level;
    }
    for (int band = 0; band < LV_MEL_BAND_COUNT; ++band) {
        const float level = std::max(0.0f, analysis->mel_level[band]);
        calibration->mel_sum[band] += level;
        calibration->mel_sum_sq[band] += level * level;
    }
    ++calibration->frames_collected;
}

bool lv_audio_calibration_finish(lv_audio_calibration_t *calibration) {
    constexpr uint16_t minimum_frames = 60;
    constexpr float maximum_variation = 0.35f;
    if (calibration->frames_collected < minimum_frames) {
        calibration->valid = false;
        return false;
    }

    const float count = (float)calibration->frames_collected;
    const auto floor_from_moments = [count](float sum, float sum_sq) {
        const float mean = sum / count;
        const float variance = std::max(0.0f, sum_sq / count - mean * mean);
        return mean + 2.0f * std::sqrt(variance);
    };
    const float mean_rms = calibration->rms_sum / count;
    const float variance_rms = std::max(
        0.0f, calibration->rms_sum_sq / count - mean_rms * mean_rms);
    const float variation = std::sqrt(variance_rms) /
                            std::max(mean_rms, 0.0001f);
    if (variation > maximum_variation) {
        calibration->valid = false;
        return false;
    }

    calibration->rms_floor = floor_from_moments(
        calibration->rms_sum, calibration->rms_sum_sq);
    for (int band = 0; band < LV_BAND_COUNT; ++band) {
        calibration->band_floor[band] = floor_from_moments(
            calibration->band_sum[band],
            calibration->band_sum_sq[band]);
    }
    for (int band = 0; band < LV_MEL_BAND_COUNT; ++band) {
        calibration->mel_floor[band] = floor_from_moments(
            calibration->mel_sum[band],
            calibration->mel_sum_sq[band]);
    }
    calibration->valid = true;
    return true;
}

float lv_audio_calibrated_level(
    const lv_audio_calibration_t *calibration,
    float linear_level, float gain) {
    gain = std::max(0.0f, gain);
    linear_level = std::max(0.0f, linear_level);
    const float floor = calibration->valid
                            ? calibration->rms_floor
                            : 0.0f;
    const float clean_power = std::max(
        0.0f, linear_level * linear_level - floor * floor);
    return std::min(1.0f, std::sqrt(clean_power) * gain);
}

void lv_audio_calibration_apply(
    const lv_audio_calibration_t *calibration,
    lv_channel_analysis_t *analysis, float gain) {
    gain = std::max(0.0f, gain);
    const auto clean_level = [gain](float level, float floor) {
        const float clean_power = std::max(
            0.0f, level * level - floor * floor);
        return std::min(1.0f, std::sqrt(clean_power) * gain);
    };

    analysis->rms_level = lv_audio_calibrated_level(
        calibration, analysis->rms_level, gain);
    for (int band = 0; band < LV_BAND_COUNT; ++band) {
        analysis->band_level[band] = clean_level(
            analysis->band_level[band],
            calibration->valid ? calibration->band_floor[band] : 0.0f);
    }
    for (int band = 0; band < LV_MEL_BAND_COUNT; ++band) {
        analysis->mel_level[band] = clean_level(
            analysis->mel_level[band],
            calibration->valid ? calibration->mel_floor[band] : 0.0f);
    }
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

void lv_lcd_spectrum_bins(const float band_level[LV_BAND_COUNT],
                          uint8_t bins[8]) {
    static const uint8_t first_band[8] = {0, 1, 2, 3, 4, 6, 8, 10};
    static const uint8_t last_band[8] = {0, 1, 2, 3, 5, 7, 9, 11};

    for (int bin = 0; bin < 8; ++bin) {
        float peak = 0.0f;
        for (int band = first_band[bin]; band <= last_band[bin]; ++band) {
            peak = std::max(peak, band_level[band]);
        }
        const int height = (int)(lv_perceptual_level(peak) * 8.0f + 0.5f);
        bins[bin] = (uint8_t)std::clamp(height, 0, 8);
    }
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
    // Lower edge, center, and upper edge for twelve triangular bands from
    // 60 Hz to 7.5 kHz, equally spaced on the perceptual mel scale. Values are
    // FFT-bin coordinates at 15.625 kHz / 256 samples.
    static constexpr float mel_points[LV_MEL_BAND_COUNT + 2] = {
        0.98304f, 3.48306f, 6.48501f, 10.08968f, 14.41808f,
        19.61552f, 25.85646f, 33.35044f, 42.34901f, 53.15427f,
        66.12895f, 81.70862f, 100.41629f, 122.88f,
    };
    // Analysis is called sequentially for left and right. Keep FFT scratch
    // space out of the RP2040's small stack and reuse it between channels.
#if defined(LANDERVIZ_USE_CMSIS_DSP_Q15)
    static q15_t fft_input[LV_FFT_N];
    // CMSIS-DSP emits the complete conjugate spectrum plus the Nyquist bin.
    static q15_t fft_output[2 * LV_FFT_N + 2];
#else
    static float re[LV_FFT_N];
    static float im[LV_FFT_N];
#endif
    static float magnitude_power[LV_FFT_N / 2];
    lv_audio_analysis_init();
    float sum_sq = 0.0f;
    float frame_mean = 0.0f;
#if defined(LANDERVIZ_USE_CMSIS_DSP_Q15)
    float frame_peak = 0.0f;
#endif

    if (analysis->bass_avg == 0.0f) analysis->bass_avg = 0.05f;

    for (int i = 0; i < LV_FFT_N; ++i) frame_mean += samples[i];
    frame_mean /= LV_FFT_N;

    for (int i = 0; i < LV_FFT_N; ++i) {
        const float sample =
            (((float)samples[i] - frame_mean) / LV_ADC_MID) * input_gain;
        sum_sq += sample * sample;
#if defined(LANDERVIZ_USE_CMSIS_DSP_Q15)
        frame_peak = std::max(frame_peak, std::fabs(sample));
#else
        re[i] = sample * hann_window[i];
        im[i] = 0.0f;
#endif
    }

#if defined(LANDERVIZ_USE_CMSIS_DSP_Q15)
    // Q15 represents -1.0..just under +1.0. Scale down only when a gained
    // frame would overflow, then undo that scale in the spectrum below.
    const float q15_scale = frame_peak > 0.999f ? 0.999f / frame_peak : 1.0f;
    for (int i = 0; i < LV_FFT_N; ++i) {
        const float sample =
            (((float)samples[i] - frame_mean) / LV_ADC_MID) * input_gain;
        const int32_t sample_q15 =
            (int32_t)std::lround(sample * q15_scale * 32767.0f);
        fft_input[i] = (q15_t)((sample_q15 * hann_window[i]) >> 15);
    }
    arm_rfft_q15(&rfft, fft_input, fft_output);

    // A 256-point CMSIS Q15 RFFT is downscaled by 8 bits internally. With
    // the Hann window's 0.5 coherent gain, a full-scale bin-centered sine
    // therefore produces a complex magnitude of 32768 / 4 = 8192.
    const float magnitude_scale = 8192.0f * q15_scale;
    const float magnitude_scale_squared = magnitude_scale * magnitude_scale;
    for (int i = 0; i < LV_FFT_N / 2; ++i) {
        const float real = (float)fft_output[2 * i];
        const float imaginary = (float)fft_output[2 * i + 1];
        magnitude_power[i] =
            (real * real + imaginary * imaginary) /
            magnitude_scale_squared;
    }
#else
    fft_radix2(re, im, LV_FFT_N);
    constexpr float magnitude_scale_squared =
        (float)(LV_FFT_N / 4) * (float)(LV_FFT_N / 4);
    for (int i = 0; i < LV_FFT_N / 2; ++i) {
        magnitude_power[i] =
            (re[i] * re[i] + im[i] * im[i]) /
            magnitude_scale_squared;
    }
#endif

    for (int band = 0; band < LV_BAND_COUNT; ++band) {
        float peak_power = 0.0f;
        for (int bin = band_start[band]; bin <= band_end[band] && bin < LV_FFT_N / 2; ++bin) {
            if (magnitude_power[bin] > peak_power) {
                peak_power = magnitude_power[bin];
            }
        }
        const float peak = std::sqrt(peak_power);
        if (peak > analysis->band_level[band]) analysis->band_level[band] = peak;
        else analysis->band_level[band] +=
                 (peak - analysis->band_level[band]) * 0.4375f;
        if (analysis->band_level[band] > 1.0f) analysis->band_level[band] = 1.0f;
    }

    for (int band = 0; band < LV_MEL_BAND_COUNT; ++band) {
        const float lower = mel_points[band];
        const float center = mel_points[band + 1];
        const float upper = mel_points[band + 2];
        float weighted_power = 0.0f;
        const int first_bin = std::max(1, (int)lower);
        const int last_bin = std::min(
            LV_FFT_N / 2 - 1, (int)std::ceil(upper));
        for (int bin = first_bin; bin <= last_bin; ++bin) {
            const float position = (float)bin;
            float weight = 0.0f;
            if (position >= lower && position <= center) {
                weight = (position - lower) / (center - lower);
            } else if (position > center && position <= upper) {
                weight = (upper - position) / (upper - center);
            }
            weighted_power += magnitude_power[bin] * weight;
        }
        const float level = std::sqrt(weighted_power);
        if (level > analysis->mel_level[band]) {
            analysis->mel_level[band] = level;
        } else {
            analysis->mel_level[band] +=
                (level - analysis->mel_level[band]) * 0.4375f;
        }
        if (analysis->mel_level[band] > 1.0f) {
            analysis->mel_level[band] = 1.0f;
        }
    }

    const float bass = (analysis->band_level[0] + analysis->band_level[1] +
                        analysis->band_level[2]) / 3.0f;
    if (bass > analysis->bass_avg * 1.25f && bass > 0.04f && bass > analysis->bass_env) {
        analysis->bass_env = bass;
    }
    analysis->bass_avg += (bass - analysis->bass_avg) * 0.0396f;
    analysis->bass_env *= 0.9216f;
    analysis->rms_level = std::sqrt(sum_sq / LV_FFT_N);
}
