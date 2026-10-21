#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>

#include "landerviz/audio_analysis.h"
#include "landerviz/audio_buffer.h"
#include "landerviz/effects.h"
#include "landerviz/flow_v2.h"
#include "landerviz/flow_v3.h"
#include "landerviz/flow_v3_attack.h"
#include "landerviz/flow_v3_combined.h"
#include "landerviz/flow_v3_energy.h"
#include "landerviz/flow_v3_engine.h"
#include "landerviz/flow_v3_mid.h"
#include "landerviz/flow_v3_texture.h"
#include "landerviz/moonlander_map.h"
#include "landerviz/protocol.h"
#include "landerviz/usb_bridge.h"
#include "landerviz/visualizer.h"

static std::array<lv_rgb_t, LV_KEY_COUNT> test_frame(uint8_t seed) {
    std::array<lv_rgb_t, LV_KEY_COUNT> frame{};
    for (uint8_t i = 0; i < frame.size(); ++i) {
        frame[i] = {(uint8_t)(seed + i), (uint8_t)(seed + i * 2),
                    (uint8_t)(seed + i * 3)};
    }
    return frame;
}

static void test_complete_frame_commits_atomically() {
    lv_decoder_t decoder{};
    const auto frame = test_frame(7);
    uint8_t packet[LV_PACKET_SIZE];

    for (uint8_t chunk = 0; chunk < LV_CHUNK_COUNT; ++chunk) {
        lv_encode_chunk(packet, 42, chunk, frame.data());
        const auto result = lv_decode_packet(&decoder, packet);
        assert(result == (chunk == 7 ? LV_FRAME_READY : LV_PACKET_ACCEPTED));
        if (chunk != 7) {
            assert(!decoder.active_valid);
        }
    }
    assert(decoder.active_valid);
    assert(std::memcmp(decoder.active, frame.data(), sizeof(decoder.active)) == 0);
}

static void test_out_of_order_chunks_and_bad_checksum() {
    lv_decoder_t decoder{};
    const auto frame = test_frame(19);
    const uint8_t order[] = {7, 2, 5, 0, 6, 1, 4, 3};
    uint8_t packet[LV_PACKET_SIZE];

    for (uint8_t i = 0; i < LV_CHUNK_COUNT; ++i) {
        lv_encode_chunk(packet, 9, order[i], frame.data());
        if (i == 2) {
            packet[8] ^= 1;
            assert(lv_decode_packet(&decoder, packet) == LV_PACKET_IGNORED);
            lv_encode_chunk(packet, 9, order[i], frame.data());
        }
        const auto result = lv_decode_packet(&decoder, packet);
        assert(result == (i == 7 ? LV_FRAME_READY : LV_PACKET_ACCEPTED));
    }
    assert(std::memcmp(decoder.active, frame.data(), sizeof(decoder.active)) == 0);
}

static void test_newer_frame_discards_partial_frame() {
    lv_decoder_t decoder{};
    const auto old_frame = test_frame(1);
    const auto new_frame = test_frame(100);
    uint8_t packet[LV_PACKET_SIZE];

    for (uint8_t chunk = 0; chunk < 4; ++chunk) {
        lv_encode_chunk(packet, 1, chunk, old_frame.data());
        assert(lv_decode_packet(&decoder, packet) == LV_PACKET_ACCEPTED);
    }
    for (uint8_t chunk = 0; chunk < LV_CHUNK_COUNT; ++chunk) {
        lv_encode_chunk(packet, 2, chunk, new_frame.data());
        lv_decode_packet(&decoder, packet);
    }
    assert(decoder.active_valid);
    assert(std::memcmp(decoder.active, new_frame.data(), sizeof(decoder.active)) == 0);
}

static void test_release_and_mapping() {
    lv_decoder_t decoder{};
    decoder.active_valid = true;
    uint8_t packet[LV_PACKET_SIZE];
    lv_encode_release(packet);
    assert(lv_decode_packet(&decoder, packet) == LV_RELEASED);
    assert(!decoder.active_valid);

    assert(lv_key_at(0, 0, 0) == 0);
    assert(lv_key_at(0, 5, 3) == 28);
    assert(lv_key_at(0, 0, 4) == 4);
    assert(lv_key_at(0, 5, 4) == -1);
    assert(lv_key_at(1, 0, 0) == 61);
    assert(lv_key_at(1, 5, 3) == 39);
    assert(lv_key_at(1, 0, 4) == 60);
    assert(lv_key_at(1, 5, 4) == -1);
    assert(lv_key_at(2, 0, 0) == -1);

    assert(lv_utility_key_at(0, 0) == 29);
    assert(lv_utility_key_at(0, 2) == 31);
    assert(lv_utility_key_at(1, 0) == 65);
    assert(lv_utility_key_at(1, 2) == 67);
    assert(lv_thumb_key_at(0, 0) == 35);
    assert(lv_thumb_key_at(0, 3) == 34);
    assert(lv_thumb_key_at(1, 0) == 71);
    assert(lv_thumb_key_at(1, 3) == 68);

    bool seen[72]{};
    for (uint8_t led : LV_ORYX_TO_LED) {
        assert(led < 72);
        assert(!seen[led]);
        seen[led] = true;
    }

    bool visualized[72]{};
    for (int half = 0; half < 2; ++half) {
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < 6; ++col) {
                const int led = lv_key_at(half, col, row);
                if (led < 0) continue;
                assert(!visualized[led]);
                visualized[led] = true;
            }
        }
        for (int key = 0; key < 3; ++key) {
            const int led = lv_utility_key_at(half, key);
            assert(!visualized[led]);
            visualized[led] = true;
        }
        for (int key = 0; key < 4; ++key) {
            const int led = lv_thumb_key_at(half, key);
            assert(!visualized[led]);
            visualized[led] = true;
        }
    }
    for (bool covered : visualized) assert(covered);
}

static void test_pending_audio_buffer_is_not_overwritten() {
    lv_audio_buffer_state_t buffers{0, -1, 0, 0};
    assert(lv_audio_buffer_complete(&buffers));
    assert(buffers.ready == 0);
    assert(buffers.fill == 1);

    assert(!lv_audio_buffer_complete(&buffers));
    assert(buffers.ready == 0);
    assert(buffers.fill == 1);
    assert(buffers.dropped == 1);

    assert(lv_audio_buffer_take(&buffers) == 0);
    assert(lv_audio_buffer_complete(&buffers));
    assert(buffers.ready == 1);
    assert(buffers.fill == 0);
    assert(buffers.dropped == 1);
}

static void test_audio_decimation_only_publishes_due_windows() {
    lv_audio_buffer_state_t buffers{0, -1, 0, 0};

    assert(lv_audio_buffer_complete_decimated(&buffers, 2));
    assert(buffers.ready == 0);
    assert(lv_audio_buffer_take(&buffers) == 0);

    assert(!lv_audio_buffer_complete_decimated(&buffers, 2));
    assert(buffers.ready == -1);
    assert(buffers.dropped == 0);

    assert(lv_audio_buffer_complete_decimated(&buffers, 2));
    assert(buffers.ready == 1);
    assert(!lv_audio_buffer_complete_decimated(&buffers, 2));
    assert(buffers.dropped == 0);
    assert(!lv_audio_buffer_complete_decimated(&buffers, 2));
    assert(buffers.dropped == 1);
}

static std::array<uint16_t, LV_FFT_N> audio_tone(int bin, float amplitude) {
    std::array<uint16_t, LV_FFT_N> samples{};
    for (int i = 0; i < LV_FFT_N; ++i) {
        const float phase = 2.0f * 3.14159265f * bin * i / LV_FFT_N;
        samples[i] = (uint16_t)(LV_ADC_MID + LV_ADC_MID * amplitude * std::sin(phase));
    }
    return samples;
}

static void test_mono_mix_preserves_centered_audio_and_stereo_levels() {
    uint16_t stereo[LV_AUDIO_CHANNEL_COUNT][LV_FFT_N]{};
    uint16_t mono[LV_FFT_N]{};
    float rms[LV_AUDIO_CHANNEL_COUNT]{};
    float peak[LV_AUDIO_CHANNEL_COUNT]{1.0f, 1.0f};
    const auto tone = audio_tone(4, 0.4f);
    std::copy(tone.begin(), tone.end(), stereo[0]);
    std::copy(tone.begin(), tone.end(), stereo[1]);

    lv_prepare_mono_analysis(stereo, mono, 1.0f, rms, peak);

    assert(std::equal(tone.begin(), tone.end(), mono));
    assert(std::fabs(rms[0] - rms[1]) < 0.000001f);
    assert(rms[0] > 0.28f && rms[0] < 0.29f);
    assert(peak[0] > 0.399f && peak[0] < 0.401f);
    assert(peak[1] > 0.399f && peak[1] < 0.401f);
}

static void test_mono_mix_accepts_opposite_phase_cancellation() {
    uint16_t stereo[LV_AUDIO_CHANNEL_COUNT][LV_FFT_N]{};
    uint16_t mono[LV_FFT_N]{};
    float rms[LV_AUDIO_CHANNEL_COUNT]{};
    float peak[LV_AUDIO_CHANNEL_COUNT]{};
    const auto tone = audio_tone(4, 0.4f);
    for (int sample = 0; sample < LV_FFT_N; ++sample) {
        stereo[0][sample] = tone[sample];
        stereo[1][sample] =
            (uint16_t)(2 * LV_ADC_MID - tone[sample]);
    }

    lv_prepare_mono_analysis(stereo, mono, 1.0f, rms, peak);

    for (uint16_t sample : mono) assert(sample == LV_ADC_MID);
    assert(rms[0] > 0.28f && rms[0] < 0.29f);
    assert(rms[1] > 0.28f && rms[1] < 0.29f);
    assert(peak[0] > 0.399f && peak[0] < 0.401f);
    assert(peak[1] > 0.399f && peak[1] < 0.401f);
}

static float synthetic_kick_sample(float time_seconds) {
    constexpr float interval = 0.5f;
    constexpr float first_kick = 0.5f;
    constexpr float pi = 3.14159265f;
    if (time_seconds < first_kick) return 0.0f;
    const float phase = std::fmod(time_seconds - first_kick, interval);
    if (phase >= 0.18f) return 0.0f;
    return 0.75f * std::exp(-phase * 18.0f) *
           std::sin(2.0f * pi * 74.0f * phase);
}

static void test_kick_detector_tracks_an_isolated_kick_train_end_to_end() {
    constexpr int sample_rate = 15625;
    constexpr int seconds = 8;
    lv_channel_analysis_t analysis{};
    lv_kick_detector_t detector{};
    std::array<uint16_t, LV_FFT_N> samples{};
    int detected = 0;
    int window = 0;

    for (int first_sample = 0; first_sample + LV_FFT_N <= seconds * sample_rate;
         first_sample += LV_FFT_N, ++window) {
        for (int sample = 0; sample < LV_FFT_N; ++sample) {
            const float time = (float)(first_sample + sample) / sample_rate;
            samples[sample] = (uint16_t)(
                LV_ADC_MID + LV_ADC_MID * synthetic_kick_sample(time));
        }
        if (window % LV_ANALYSIS_DECIMATION != 0) continue;
        lv_analyze_channel(&analysis, samples.data(), 1.0f);
        const uint32_t now_ms = (uint32_t)(
            (uint64_t)(first_sample + LV_FFT_N) * 1000 / sample_rate);
        const auto event = lv_kick_detector_update(
            &detector, analysis.bass_env, now_ms);
        if (event.triggered) {
            ++detected;
        }
    }

    // Fifteen kicks occur after the initial half-second of silence.
    if (detected < 14 || detected > 15) {
        std::fprintf(stderr, "isolated kick train detected %d of 15\n",
                     detected);
    }
    assert(detected >= 14 && detected <= 15);
}

static int dominant_band(const lv_channel_analysis_t &analysis) {
    int dominant = 0;
    for (int band = 1; band < LV_BAND_COUNT; ++band) {
        if (analysis.band_level[band] > analysis.band_level[dominant]) dominant = band;
    }
    return dominant;
}

static int dominant_mel_band(const lv_channel_analysis_t &analysis) {
    int dominant = 0;
    for (int band = 1; band < LV_MEL_BAND_COUNT; ++band) {
        if (analysis.mel_level[band] > analysis.mel_level[dominant]) {
            dominant = band;
        }
    }
    return dominant;
}

static void test_mel_filterbank_tracks_perceptual_frequency_regions() {
    lv_channel_analysis_t low{};
    lv_channel_analysis_t body{};
    lv_channel_analysis_t presence{};

    const auto low_tone = audio_tone(4, 0.5f);
    const auto body_tone = audio_tone(26, 0.5f);
    const auto presence_tone = audio_tone(82, 0.5f);
    lv_analyze_channel(&low, low_tone.data(), 1.0f);
    lv_analyze_channel(&body, body_tone.data(), 1.0f);
    lv_analyze_channel(&presence, presence_tone.data(), 1.0f);

    assert(dominant_mel_band(low) == 0);
    assert(dominant_mel_band(body) == 5);
    assert(dominant_mel_band(presence) == 10);
    assert(body.mel_level[4] > 0.0f);
    assert(body.mel_level[6] > 0.0f);
}

static void test_stereo_analysis_keeps_channels_independent() {
    lv_channel_analysis_t left{};
    lv_channel_analysis_t right{};
    const auto low_tone = audio_tone(4, 0.6f);
    const auto high_tone = audio_tone(40, 0.6f);

    lv_analyze_channel(&left, low_tone.data(), 1.0f);
    lv_analyze_channel(&right, high_tone.data(), 1.0f);

    assert(dominant_band(left) == 2);
    assert(dominant_band(right) == 9);
    assert(left.band_level[9] < 0.01f);
    assert(right.band_level[2] < 0.01f);
    assert(left.bass_env > 0.12f);
    assert(right.bass_env < 0.01f);
    assert(left.rms_level > 0.4f);
    assert(right.rms_level > 0.4f);
}

static void test_silent_channel_stays_silent() {
    lv_channel_analysis_t analysis{};
    std::array<uint16_t, LV_FFT_N> silence{};
    silence.fill(LV_ADC_MID);

    lv_analyze_channel(&analysis, silence.data(), 1.0f);

    assert(analysis.rms_level == 0.0f);
    assert(analysis.bass_env == 0.0f);
    for (float level : analysis.band_level) assert(level == 0.0f);
    for (float level : analysis.mel_level) assert(level == 0.0f);
}

static void test_biased_silence_removes_measured_dc_offset() {
    lv_channel_analysis_t analysis{};
    std::array<uint16_t, LV_FFT_N> biased_silence{};
    biased_silence.fill(2023);

    lv_analyze_channel(&analysis, biased_silence.data(), 8.0f);

    assert(analysis.rms_level == 0.0f);
    assert(analysis.bass_env == 0.0f);
    for (float level : analysis.band_level) assert(level == 0.0f);
    for (float level : analysis.mel_level) assert(level == 0.0f);
}

static void test_input_gain_scales_audio_before_analysis() {
    lv_channel_analysis_t normal{};
    lv_channel_analysis_t boosted{};
    const auto tone = audio_tone(8, 0.025f);

    lv_analyze_channel(&normal, tone.data(), 1.0f);
    lv_analyze_channel(&boosted, tone.data(), 8.0f);

    assert(boosted.rms_level > normal.rms_level * 7.9f);
    assert(boosted.rms_level < normal.rms_level * 8.1f);
    assert(dominant_band(boosted) == dominant_band(normal));
    assert(boosted.band_level[dominant_band(boosted)] >
           normal.band_level[dominant_band(normal)] * 7.9f);
}

static void test_fft_preserves_tone_amplitude() {
    lv_channel_analysis_t analysis{};
    constexpr float amplitude = 0.25f;
    const auto tone = audio_tone(8, amplitude);

    lv_analyze_channel(&analysis, tone.data(), 1.0f);

    const float peak = analysis.band_level[dominant_band(analysis)];
    assert(peak > amplitude * 0.98f);
    assert(peak < amplitude * 1.02f);
}

static void test_boosted_quiet_kick_creates_visible_envelope() {
    lv_channel_analysis_t analysis{};
    const auto quiet_bass = audio_tone(4, 0.025f);
    const auto silence = audio_tone(4, 0.0f);

    lv_analyze_channel(&analysis, quiet_bass.data(), 8.0f);
    assert(analysis.bass_env > 0.0f);
    const float kick = analysis.bass_env;

    lv_analyze_channel(&analysis, silence.data(), 8.0f);
    // Analysis now runs every other 16 ms capture window. The squared release
    // coefficient preserves the same decay per wall-clock second.
    assert(analysis.bass_env > kick * 0.90f);
    assert(analysis.bass_env < kick * 0.94f);
}

static void test_perceptual_level_uses_decibel_scale_and_noise_floor() {
    assert(lv_perceptual_level(0.0f) == 0.0f);
    assert(lv_perceptual_level(0.003f) == 0.0f);
    assert(lv_perceptual_level(0.004f) > 0.0f);
    assert(std::fabs(lv_perceptual_level(0.1f) - 0.5833f) < 0.001f);
    assert(lv_perceptual_level(0.5f) > lv_perceptual_level(0.1f));
    assert(lv_perceptual_level(1.0f) == 1.0f);
    assert(lv_perceptual_level(2.0f) == 1.0f);
}

static void test_level_dbfs_rounds_and_has_a_diagnostic_floor() {
    assert(lv_level_dbfs(1.0f) == 0);
    assert(lv_level_dbfs(2.0f) == 0);
    assert(lv_level_dbfs(0.501187f) == -6);
    assert(lv_level_dbfs(0.1f) == -20);
    assert(lv_level_dbfs(0.0f) == -72);
}

static void test_audio_normalizer_adapts_without_promoting_noise_or_beats() {
    lv_audio_normalizer_t normalizer{};
    uint32_t now_ms = 0;
    float gain = 0.0f;

    for (int frame = 0; frame < 150; ++frame) {
        gain = lv_audio_normalizer_update(&normalizer, 0.025f, false,
                                          now_ms);
        now_ms += 33;
    }
    assert(!normalizer.gate_open);
    assert(gain > 0.95f && gain < 1.05f);

    for (int frame = 0; frame < 2; ++frame) {
        gain = lv_audio_normalizer_update(&normalizer, 0.032f, false,
                                          now_ms);
        now_ms += 33;
        assert(!normalizer.gate_open);
    }
    gain = lv_audio_normalizer_update(&normalizer, 0.032f, false, now_ms);
    now_ms += 33;
    assert(normalizer.gate_open);
    assert(gain > 5.5f && gain <= 6.3f);

    const float before_hit = gain;
    gain = lv_audio_normalizer_update(&normalizer, 0.20f, false, now_ms);
    now_ms += 33;
    assert(normalizer.gate_open);
    assert(gain > before_hit * 0.95f);

    for (int frame = 0; frame < 75; ++frame) {
        gain = lv_audio_normalizer_update(&normalizer, 0.20f, false,
                                          now_ms);
        now_ms += 33;
    }
    assert(gain > 0.8f && gain < 2.3f);

    const float before_quiet_return = gain;
    gain = lv_audio_normalizer_update(&normalizer, 0.032f, false, now_ms);
    now_ms += 33;
    assert(gain < before_quiet_return + 0.1f);
    for (int frame = 0; frame < 180; ++frame) {
        gain = lv_audio_normalizer_update(&normalizer, 0.032f, false,
                                          now_ms);
        now_ms += 33;
    }
    assert(gain > 3.5f && gain < 6.3f);

    const float before_silence = gain;
    for (int frame = 0; frame < 30; ++frame) {
        gain = lv_audio_normalizer_update(&normalizer, 0.025f, false,
                                          now_ms);
        now_ms += 33;
    }
    assert(!normalizer.gate_open);
    assert(gain < before_silence);
}

static void test_audio_normalizer_rejects_an_isolated_noise_spike() {
    lv_audio_normalizer_t normalizer{};
    uint32_t now_ms = 0;
    for (int frame = 0; frame < 30; ++frame) {
        lv_audio_normalizer_update(&normalizer, 0.025f, false, now_ms);
        now_ms += 33;
    }
    lv_audio_normalizer_update(&normalizer, 0.040f, false, now_ms);
    now_ms += 33;
    assert(!normalizer.gate_open);
    for (int frame = 0; frame < 4; ++frame) {
        lv_audio_normalizer_update(&normalizer, 0.025f, false, now_ms);
        now_ms += 33;
    }
    assert(!normalizer.gate_open);
    assert(normalizer.gain > 0.95f && normalizer.gain < 1.05f);
}

static void test_audio_calibration_accepts_and_removes_stable_noise() {
    lv_audio_calibration_t calibration{};
    lv_audio_calibration_reset(&calibration);
    lv_channel_analysis_t noise{};
    noise.rms_level = 0.025f;
    for (int band = 0; band < LV_BAND_COUNT; ++band) {
        noise.band_level[band] = 0.010f + band * 0.0005f;
    }
    for (int band = 0; band < LV_MEL_BAND_COUNT; ++band) {
        noise.mel_level[band] = 0.008f + band * 0.0004f;
    }
    for (int frame = 0; frame < 100; ++frame) {
        lv_audio_calibration_add(&calibration, noise.rms_level, &noise);
    }

    assert(lv_audio_calibration_finish(&calibration));
    assert(calibration.valid);
    assert(calibration.rms_floor >= 0.025f);
    assert(calibration.rms_floor < 0.026f);

    auto filtered = noise;
    lv_audio_calibration_apply(&calibration, &filtered, 6.0f);
    assert(filtered.rms_level < 0.0001f);
    for (float level : filtered.band_level) assert(level < 0.0001f);
    for (float level : filtered.mel_level) assert(level < 0.0001f);

    lv_channel_analysis_t signal{};
    signal.rms_level = 0.10f;
    std::fill_n(signal.band_level, LV_BAND_COUNT, 0.10f);
    std::fill_n(signal.mel_level, LV_MEL_BAND_COUNT, 0.08f);
    lv_audio_calibration_apply(&calibration, &signal, 2.0f);
    assert(signal.rms_level > 0.19f && signal.rms_level < 0.20f);
    assert(signal.band_level[0] > 0.19f && signal.band_level[0] < 0.20f);
    assert(signal.mel_level[0] > 0.15f && signal.mel_level[0] < 0.16f);
}

static void test_calibrated_spectral_signal_rejects_noise_and_one_band_spikes() {
    lv_audio_calibration_t calibration{};
    calibration.valid = true;
    std::fill_n(calibration.mel_floor, LV_MEL_BAND_COUNT, 0.010f);

    lv_channel_analysis_t analysis{};
    std::fill_n(analysis.mel_level, LV_MEL_BAND_COUNT, 0.010f);
    assert(!lv_audio_calibration_signal_present(&calibration, &analysis));

    analysis.mel_level[4] = 0.013f;
    assert(!lv_audio_calibration_signal_present(&calibration, &analysis));

    analysis.mel_level[5] = 0.013f;
    assert(lv_audio_calibration_signal_present(&calibration, &analysis));
}

static void test_spectral_signal_keeps_quiet_music_gate_open_with_hold() {
    lv_audio_normalizer_t normalizer{};
    normalizer.noise_floor = 0.025f;
    uint32_t now_ms = 0;

    // Broadband RMS remains below the noise gate, but persistent calibrated
    // frequency content proves this is quiet music rather than silence.
    for (int frame = 0; frame < 3; ++frame) {
        lv_audio_normalizer_update(&normalizer, 0.024f, true, now_ms);
        now_ms += 33;
    }
    assert(normalizer.gate_open);
    assert(normalizer.gain > 6.0f);

    // Brief spaces between notes do not visibly chop the visualization.
    for (int frame = 0; frame < 12; ++frame) {
        lv_audio_normalizer_update(&normalizer, 0.024f, false, now_ms);
        now_ms += 33;
    }
    assert(normalizer.gate_open);

    // Sustained calibrated silence still closes the gate.
    for (int frame = 0; frame < 6; ++frame) {
        lv_audio_normalizer_update(&normalizer, 0.024f, false, now_ms);
        now_ms += 33;
    }
    assert(!normalizer.gate_open);
}

static void test_audio_calibration_rejects_variable_input() {
    lv_audio_calibration_t calibration{};
    lv_channel_analysis_t frame{};
    for (int sample = 0; sample < 100; ++sample) {
        frame.rms_level = sample % 2 == 0 ? 0.010f : 0.100f;
        std::fill_n(frame.band_level, LV_BAND_COUNT, frame.rms_level);
        std::fill_n(frame.mel_level, LV_MEL_BAND_COUNT, frame.rms_level);
        lv_audio_calibration_add(&calibration, frame.rms_level, &frame);
    }
    assert(!lv_audio_calibration_finish(&calibration));
    assert(!calibration.valid);
}

static void test_audio_normalizer_preserves_an_explicit_floor() {
    lv_audio_normalizer_t normalizer{};
    normalizer.noise_floor = 0.025f;
    uint32_t now_ms = 0;
    for (int frame = 0; frame < 300; ++frame) {
        lv_audio_normalizer_update(&normalizer, 0.015f, false, now_ms);
        now_ms += 33;
    }
    assert(!normalizer.gate_open);
    assert(std::fabs(normalizer.noise_floor - 0.025f) < 0.000001f);
}

static void test_meter_level_gates_noise_without_reducing_normal_audio() {
    assert(lv_meter_level(0.03f) == 0.0f);
    assert(lv_meter_level(0.04f) > 0.0f);
    assert(lv_meter_level(0.04f) < lv_perceptual_level(0.04f));
    assert(std::fabs(lv_meter_level(0.1f) - lv_perceptual_level(0.1f)) < 0.0001f);
}

static void test_lcd_spectrum_bins_preserve_low_and_high_frequency_edges() {
    float bands[LV_BAND_COUNT] = {};
    uint8_t bins[8] = {};
    lv_lcd_spectrum_bins(bands, bins);
    for (uint8_t level : bins) assert(level == 0);

    bands[0] = 1.0f;
    lv_lcd_spectrum_bins(bands, bins);
    assert(bins[0] == 8);
    assert(bins[7] == 0);

    bands[0] = 0.0f;
    bands[11] = 1.0f;
    lv_lcd_spectrum_bins(bands, bins);
    assert(bins[0] == 0);
    assert(bins[7] == 8);

    bands[11] = 0.0f;
    bands[4] = 0.1f;
    lv_lcd_spectrum_bins(bands, bins);
    assert(bins[4] >= 4 && bins[4] <= 6);
}

static void test_audio_activity_ignores_isolated_noise_spikes() {
    lv_audio_activity_t activity{};

    assert(!lv_audio_activity_update(&activity, true, 0));
    assert(!lv_audio_activity_update(&activity, true, 16));
    assert(!lv_audio_activity_update(&activity, false, 32));
    assert(lv_audio_activity_update(&activity, true, 48));

    assert(lv_audio_activity_update(&activity, false, 64));
    assert(lv_audio_activity_update(&activity, false, 80));
    assert(lv_audio_activity_update(&activity, false, 96));
    assert(lv_audio_activity_update(&activity, true, 4000));
    assert(lv_audio_activity_update(&activity, false, 4016));
    assert(!lv_audio_activity_update(&activity, false, 5064));
}

static void test_audio_activity_requires_sustained_sound_to_leave_idle() {
    lv_audio_activity_t activity{};

    assert(!lv_audio_activity_update(&activity, true, 100));
    assert(!lv_audio_activity_update(&activity, false, 116));
    assert(!lv_audio_activity_update(&activity, false, 132));
    assert(!lv_audio_activity_update(&activity, false, 148));

    assert(!lv_audio_activity_update(&activity, true, 200));
    assert(!lv_audio_activity_update(&activity, true, 216));
    assert(!lv_audio_activity_update(&activity, false, 232));
    assert(lv_audio_activity_update(&activity, true, 248));
}

static int rgb_brightness(lv_rgb_t color) {
    return std::max((int)color.r, std::max((int)color.g, (int)color.b));
}

static lv_channel_analysis_t flow_v2_audio(float rms, int active_band,
                                           float band_level) {
    lv_channel_analysis_t audio{};
    audio.rms_level = rms;
    if (active_band >= 0) audio.mel_level[active_band] = band_level;
    return audio;
}

static lv_channel_analysis_t flow_v2_audio_group(float rms, int first_band,
                                                 int last_band,
                                                 float band_level) {
    lv_channel_analysis_t audio{};
    audio.rms_level = rms;
    for (int band = first_band; band <= last_band; ++band) {
        audio.mel_level[band] = band_level;
    }
    return audio;
}

static lv_channel_analysis_t flow_v3_audio(float rms, float bass_61,
                                           float bass_122,
                                           int mel_band = 4,
                                           float mel_level = 0.4f) {
    lv_channel_analysis_t audio{};
    audio.rms_level = rms;
    audio.band_level[0] = bass_61;
    audio.band_level[1] = bass_122;
    if (mel_band >= 0) audio.mel_level[mel_band] = mel_level;
    return audio;
}

static void test_flow_v3_bass_extraction_is_low_and_phase_safe() {
    lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v3_audio(0.2f, 0.0f, 0.62f),
        flow_v3_audio(0.2f, 0.18f, 0.0f),
    };
    assert(std::fabs(lv_flow_v3_bass_level(audio) - 0.62f) < 0.001f);

    audio[1].band_level[2] = 0.90f;
    assert(std::fabs(lv_flow_v3_bass_level(audio) - 0.765f) < 0.001f);

    audio[0] = flow_v3_audio(0.2f, 0.0f, 0.0f, 7, 1.0f);
    audio[1] = audio[0];
    assert(lv_flow_v3_bass_level(audio) == 0.0f);
}

static void test_flow_v3_sustain_has_continuous_body_without_attack_flash() {
    lv_flow_v3_t flow{};
    lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v3_audio(0.2f, 0.02f, 0.02f),
        flow_v3_audio(0.2f, 0.02f, 0.02f),
    };
    lv_flow_v3_update(&flow, audio, true, 0);
    audio[0] = flow_v3_audio(0.3f, 0.55f, 0.48f);
    audio[1] = audio[0];
    lv_flow_v3_update(&flow, audio, true, 33);
    assert(flow.features.body > 0.5f);
    assert(flow.features.attack == 0.0f);

    for (uint32_t now_ms = 66; now_ms <= 330; now_ms += 33) {
        lv_flow_v3_update(&flow, audio, true, now_ms);
    }
    assert(flow.features.body > 0.5f);
    assert(flow.features.attack == 0.0f);
}

static void test_flow_v3_close_bass_rises_remain_distinguishable() {
    lv_flow_v3_t flow{};
    lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v3_audio(0.1f, 0.02f, 0.02f),
        flow_v3_audio(0.1f, 0.02f, 0.02f),
    };
    lv_flow_v3_update(&flow, audio, true, 0);

    audio[0] = flow_v3_audio(0.3f, 0.70f, 0.55f);
    audio[1] = audio[0];
    lv_flow_v3_update(&flow, audio, true, 33);
    const float first = flow.features.body;

    audio[0] = flow_v3_audio(0.15f, 0.08f, 0.06f);
    audio[1] = audio[0];
    lv_flow_v3_update(&flow, audio, true, 66);
    lv_flow_v3_update(&flow, audio, true, 99);
    const float valley = flow.features.body;

    audio[0] = flow_v3_audio(0.3f, 0.66f, 0.58f);
    audio[1] = audio[0];
    lv_flow_v3_update(&flow, audio, true, 132);
    const float second = flow.features.body;
    assert(first > valley + 0.20f);
    assert(second > valley + 0.20f);
    assert(flow.features.attack == 0.0f);
}

static void test_flow_v3_bass_lab_has_one_stable_color() {
    lv_flow_v3_t flow{};
    lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v3_audio(0.3f, 0.60f, 0.50f, 1, 0.8f),
        flow_v3_audio(0.3f, 0.60f, 0.50f, 1, 0.8f),
    };
    lv_flow_v3_update(&flow, audio, true, 0);
    const float first_hue = flow.features.hue;
    audio[0] = flow_v3_audio(0.3f, 0.60f, 0.50f, 10, 0.8f);
    audio[1] = audio[0];
    lv_flow_v3_update(&flow, audio, true, 33);
    assert(flow.features.hue == first_hue);
}

static void test_flow_v3_bass_contrast_preserves_body_and_separates_hits() {
    lv_flow_v3_t lab_a{};
    lv_flow_v3_t lab_b{};
    lv_flow_v3_set_bass_contrast(&lab_b, true);
    lv_channel_analysis_t bed[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v3_audio(0.3f, 0.34f, 0.30f),
        flow_v3_audio(0.3f, 0.34f, 0.30f),
    };
    for (uint32_t now_ms = 0; now_ms <= 3000; now_ms += 33) {
        lv_flow_v3_update(&lab_a, bed, true, now_ms);
        lv_flow_v3_update(&lab_b, bed, true, now_ms);
    }
    const float bed_a = lab_a.features.body;
    const float bed_b = lab_b.features.body;
    assert(bed_b < bed_a);
    assert(bed_b >= bed_a * 0.40f);

    lv_channel_analysis_t hit[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v3_audio(0.5f, 0.75f, 0.68f),
        flow_v3_audio(0.5f, 0.75f, 0.68f),
    };
    lv_flow_v3_update(&lab_a, hit, true, 3033);
    lv_flow_v3_update(&lab_b, hit, true, 3033);
    const float rise_a = lab_a.features.body - bed_a;
    const float rise_b = lab_b.features.body - bed_b;
    assert(rise_b > rise_a);
    assert(lab_b.features.attack == 0.0f);
}

static void test_flow_v3_bass_contrast_suppresses_micro_twinkle() {
    lv_flow_v3_t lab_a{};
    lv_flow_v3_t lab_b{};
    lv_flow_v3_set_bass_contrast(&lab_b, true);
    lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v3_audio(0.3f, 0.34f, 0.30f),
        flow_v3_audio(0.3f, 0.34f, 0.30f),
    };
    for (uint32_t now_ms = 0; now_ms <= 3000; now_ms += 33) {
        lv_flow_v3_update(&lab_a, audio, true, now_ms);
        lv_flow_v3_update(&lab_b, audio, true, now_ms);
    }
    const float a_before = lab_a.features.body;
    const float b_before = lab_b.features.body;

    audio[0] = flow_v3_audio(0.3f, 0.36f, 0.32f);
    audio[1] = audio[0];
    lv_flow_v3_update(&lab_a, audio, true, 3033);
    lv_flow_v3_update(&lab_b, audio, true, 3033);
    const float a_motion = lab_a.features.body - a_before;
    const float b_motion = lab_b.features.body - b_before;
    assert(a_motion > 0.0f);
    assert(b_motion > 0.0f);
    assert(b_motion < a_motion * 0.65f);
}

static void test_flow_v3_bass_lab_b_tracks_39_kicks_without_flashes() {
    constexpr int sample_rate = 15625;
    constexpr int seconds = 20;
    lv_channel_analysis_t analysis[LV_AUDIO_CHANNEL_COUNT]{};
    lv_flow_v3_t flow{};
    lv_flow_v3_set_bass_contrast(&flow, true);
    std::array<uint16_t, LV_FFT_N> samples{};
    float previous_previous = 0.0f;
    float previous = 0.0f;
    int visible_peaks = 0;
    int window = 0;

    for (int first_sample = 0;
         first_sample + LV_FFT_N <= seconds * sample_rate;
         first_sample += LV_FFT_N, ++window) {
        for (int sample = 0; sample < LV_FFT_N; ++sample) {
            const float time =
                (float)(first_sample + sample) / sample_rate;
            samples[sample] = (uint16_t)(
                LV_ADC_MID + LV_ADC_MID * synthetic_kick_sample(time));
        }
        if (window % LV_ANALYSIS_DECIMATION != 0) continue;
        for (auto &channel : analysis) {
            lv_analyze_channel(&channel, samples.data(), 1.0f);
        }
        const bool gate_open =
            lv_meter_level(analysis[0].rms_level * 1.41421356f) > 0.0f;
        const uint32_t audio_midpoint_ms = (uint32_t)(
            (uint64_t)(first_sample + LV_FFT_N / 2) * 1000 /
            sample_rate);
        lv_flow_v3_update(&flow, analysis, gate_open, audio_midpoint_ms);
        assert(flow.features.attack == 0.0f);
        const float current = flow.features.body;
        if (previous > previous_previous && previous >= current &&
            previous > 0.30f) {
            ++visible_peaks;
        }
        previous_previous = previous;
        previous = current;
    }

    if (visible_peaks != 39) {
        std::fprintf(stderr,
                     "FLOW v3 Bass Lab B rendered %d of 39 kick peaks\n",
                     visible_peaks);
    }
    assert(visible_peaks == 39);
}

static void test_flow_v3_mid_palette_is_vivid_and_equal_lightness() {
    constexpr float target_lightness = 0.62f;
    float capped_minimum_lightness = 1.0f;
    float capped_maximum_lightness = 0.0f;
    for (int hue = 0; hue < 360; hue += 15) {
        const auto color = lv_flow_v3_vivid_color(
            (float)hue, target_lightness);
        const int maximum = std::max({(int)color.r, (int)color.g,
                                      (int)color.b});
        const int minimum = std::min({(int)color.r, (int)color.g,
                                      (int)color.b});
        assert(maximum - minimum >= 80);
        assert(std::fabs(lv_flow_v3_color_lightness(color) -
                         target_lightness) < 0.012f);
        const lv_rgb_t capped = {
            (uint8_t)(((uint16_t)color.r * 175 + 127) / 255),
            (uint8_t)(((uint16_t)color.g * 175 + 127) / 255),
            (uint8_t)(((uint16_t)color.b * 175 + 127) / 255),
        };
        const float capped_lightness =
            lv_flow_v3_color_lightness(capped);
        capped_minimum_lightness = std::min(capped_minimum_lightness,
                                            capped_lightness);
        capped_maximum_lightness = std::max(capped_maximum_lightness,
                                            capped_lightness);
    }
    assert(capped_maximum_lightness - capped_minimum_lightness < 0.025f);
}

static void test_flow_v3_mid_lab_ignores_bass_and_highs() {
    lv_flow_v3_mid_t mids{};
    lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v2_audio(0.3f, 0, 0.9f),
        flow_v2_audio(0.3f, 0, 0.9f),
    };
    lv_flow_v3_mid_update(&mids, audio, true, 0);
    assert(mids.features.raw_level == 0.0f);

    audio[0] = flow_v2_audio(0.3f, 10, 0.9f);
    audio[1] = audio[0];
    lv_flow_v3_mid_update(&mids, audio, true, 33);
    assert(mids.features.raw_level == 0.0f);

    audio[0] = flow_v2_audio(0.3f, 4, 0.9f);
    audio[1] = audio[0];
    lv_flow_v3_mid_update(&mids, audio, true, 66);
    assert(mids.features.raw_level > 0.0f);
    assert(mids.features.presence > 0.0f);
}

static void test_flow_v3_mid_hue_tracks_frequency_without_brightness_motion() {
    lv_flow_v3_mid_t low{};
    lv_flow_v3_mid_t high{};
    lv_channel_analysis_t low_audio[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v2_audio(0.3f, 1, 0.8f),
        flow_v2_audio(0.3f, 1, 0.8f),
    };
    lv_channel_analysis_t high_audio[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v2_audio(0.3f, 7, 0.8f),
        flow_v2_audio(0.3f, 7, 0.8f),
    };
    lv_flow_v3_mid_update(&low, low_audio, true, 0);
    lv_flow_v3_mid_update(&high, high_audio, true, 0);
    assert(low.features.hue < high.features.hue);
    assert(std::fabs(low.features.lightness -
                     high.features.lightness) < 0.001f);

    const float initial_hue = low.features.hue;
    for (uint32_t now_ms = 33; now_ms <= 330; now_ms += 33) {
        lv_flow_v3_mid_update(&low, low_audio, true, now_ms);
    }
    assert(std::fabs(low.features.hue - initial_hue) < 0.001f);
    assert(std::fabs(low.features.lightness - 0.62f) < 0.001f);
}

static void test_flow_v3_mid_spectral_contrast_accents_new_frequency() {
    lv_flow_v3_mid_t high_accent{};
    lv_channel_analysis_t low_bed[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v2_audio(0.3f, 2, 0.55f),
        flow_v2_audio(0.3f, 2, 0.55f),
    };
    for (uint32_t now_ms = 0; now_ms <= 3000; now_ms += 33) {
        lv_flow_v3_mid_update(&high_accent, low_bed, true, now_ms);
    }
    assert(high_accent.features.contrast_strength < 0.01f);

    auto high_event = low_bed[0];
    high_event.mel_level[6] = 0.70f;
    lv_channel_analysis_t high_audio[LV_AUDIO_CHANNEL_COUNT] = {
        high_event,
        high_event,
    };
    lv_flow_v3_mid_update(&high_accent, high_audio, true, 3033);
    assert(high_accent.features.contrast_strength > 0.10f);
    assert(high_accent.features.contrast_centroid_hz >
           high_accent.features.centroid_hz);
    assert(high_accent.features.target_hue >
           high_accent.features.base_hue);

    lv_flow_v3_mid_t low_accent{};
    lv_channel_analysis_t high_bed[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v2_audio(0.3f, 6, 0.55f),
        flow_v2_audio(0.3f, 6, 0.55f),
    };
    for (uint32_t now_ms = 0; now_ms <= 3000; now_ms += 33) {
        lv_flow_v3_mid_update(&low_accent, high_bed, true, now_ms);
    }
    auto low_event = high_bed[0];
    low_event.mel_level[1] = 0.70f;
    lv_channel_analysis_t low_audio[LV_AUDIO_CHANNEL_COUNT] = {
        low_event,
        low_event,
    };
    lv_flow_v3_mid_update(&low_accent, low_audio, true, 3033);
    assert(low_accent.features.contrast_strength > 0.10f);
    assert(low_accent.features.contrast_centroid_hz <
           low_accent.features.centroid_hz);
    assert(low_accent.features.target_hue < low_accent.features.base_hue);
    assert(std::fabs(high_accent.features.lightness -
                     low_accent.features.lightness) < 0.001f);
}

static void test_flow_v3_mid_sustained_change_returns_to_base_hue() {
    lv_flow_v3_mid_t mids{};
    lv_channel_analysis_t bed[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v2_audio(0.3f, 2, 0.55f),
        flow_v2_audio(0.3f, 2, 0.55f),
    };
    for (uint32_t now_ms = 0; now_ms <= 3000; now_ms += 33) {
        lv_flow_v3_mid_update(&mids, bed, true, now_ms);
    }
    auto changed = bed[0];
    changed.mel_level[6] = 0.70f;
    lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT] = {
        changed,
        changed,
    };
    lv_flow_v3_mid_update(&mids, audio, true, 3033);
    const float initial_contrast = mids.features.contrast_strength;
    for (uint32_t now_ms = 3066; now_ms <= 10000; now_ms += 33) {
        lv_flow_v3_mid_update(&mids, audio, true, now_ms);
    }
    assert(mids.features.contrast_strength < initial_contrast * 0.10f);
    assert(std::fabs(mids.features.target_hue - mids.features.base_hue) <
           1.0f);
}

static void test_flow_v3_combined_sample_preserves_bass_body() {
    lv_flow_v3_t bass{};
    bass.travel_ms = 100;
    bass.history[0] = {195.0f, 255.0f, 0.2f, 0.2f,
                       0.0f, 0.0f, 0};
    bass.history[1] = {195.0f, 255.0f, 0.8f, 0.8f,
                       0.0f, 0.0f, 100};
    bass.history_count = 2;
    bass.history_head = 2;

    lv_flow_v3_mid_t mids{};
    mids.travel_ms = 100;
    mids.history[0] = {300.0f, 255.0f, 1.0f, 1.0f,
                       0.0f, 0.0f, 0};
    mids.history[1] = {300.0f, 255.0f, 1.0f, 1.0f,
                       0.0f, 0.0f, 100};
    mids.history_count = 2;
    mids.history_head = 2;

    const auto combined = lv_flow_v3_combined_sample(
        &bass, &mids, 0.5f, 100);
    assert(std::fabs(combined.body - 0.5f) < 0.001f);
    assert(std::fabs(combined.energy - 0.5f) < 0.001f);
    assert(std::fabs(combined.hue - 300.0f) < 0.001f);

    mids.history[0].body = 0.0f;
    mids.history[1].body = 0.0f;
    const auto bass_only = lv_flow_v3_combined_sample(
        &bass, &mids, 0.5f, 100);
    assert(std::fabs(bass_only.hue - 195.0f) < 0.001f);
    assert(std::fabs(bass_only.body - combined.body) < 0.001f);
}

static void test_flow_v3_combined_hue_preserves_bass_lightness() {
    constexpr float bodies[] = {0.05f, 0.20f, 0.50f, 1.0f};
    constexpr float coordinates[] = {0.0f, 3.0f, 7.0f};
    for (float body : bodies) {
        for (float coordinate : coordinates) {
            const float distance_fade = std::pow(0.975f, coordinate);
            const float value = (0.12f + 0.88f * body) * distance_fade;
            const auto reference = lv_hsv_to_rgb(
                (uint8_t)std::lround(195.0f * 255.0f / 360.0f), 255,
                (uint8_t)std::lround(value * 255.0f));
            const float reference_lightness =
                lv_flow_v3_color_lightness(reference);
            float capped_minimum = 1.0f;
            float capped_maximum = 0.0f;
            for (int hue = 0; hue < 360; hue += 15) {
                lv_flow_v3_slice_t slice{};
                slice.hue = (float)hue;
                slice.body = body;
                const auto color = lv_flow_v3_combined_color(
                    slice, coordinate);
                assert(std::fabs(lv_flow_v3_color_lightness(color) -
                                 reference_lightness) < 0.012f);
                const lv_rgb_t capped = {
                    (uint8_t)(((uint16_t)color.r * 175 + 127) / 255),
                    (uint8_t)(((uint16_t)color.g * 175 + 127) / 255),
                    (uint8_t)(((uint16_t)color.b * 175 + 127) / 255),
                };
                const float capped_lightness =
                    lv_flow_v3_color_lightness(capped);
                capped_minimum = std::min(capped_minimum,
                                          capped_lightness);
                capped_maximum = std::max(capped_maximum,
                                          capped_lightness);
            }
            assert(capped_maximum - capped_minimum < 0.025f);
        }
    }

    lv_flow_v3_slice_t silence{};
    silence.hue = 120.0f;
    const auto off = lv_flow_v3_combined_color(silence, 0.0f);
    assert(off.r == 0 && off.g == 0 && off.b == 0);
}

static void test_flow_v3_energy_uses_fixed_phase_safe_full_spectrum_level() {
    lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT]{};
    audio[0].rms_level = 0.10f;
    audio[1].rms_level = 0.30f;
    audio[0].mel_level[1] = 1.0f;
    audio[1].mel_level[10] = 1.0f;

    const float raw = lv_flow_v3_energy_level(audio);
    assert(std::fabs(raw - 0.30f * 1.41421356f) < 0.001f);

    lv_flow_v3_energy_t first{};
    lv_flow_v3_energy_t second{};
    lv_flow_v3_energy_update(&first, audio, true, 0);
    lv_flow_v3_energy_update(&second, audio, true, 5000);
    assert(std::fabs(first.features.target - second.features.target) <
           0.001f);
    assert(std::fabs(first.features.energy - second.features.energy) <
           0.001f);

    lv_flow_v3_energy_t gated{};
    lv_flow_v3_energy_update(&gated, audio, false, 0);
    assert(gated.features.target == 0.0f);
    assert(gated.features.energy == 0.0f);
}

static void test_flow_v3_energy_has_smooth_attack_and_release() {
    lv_flow_v3_energy_t energy{};
    lv_channel_analysis_t quiet[LV_AUDIO_CHANNEL_COUNT]{};
    quiet[0].rms_level = 0.04f;
    quiet[1].rms_level = 0.04f;
    lv_flow_v3_energy_update(&energy, quiet, true, 0);
    const float quiet_level = energy.features.energy;

    lv_channel_analysis_t loud[LV_AUDIO_CHANNEL_COUNT]{};
    loud[0].rms_level = 0.45f;
    loud[1].rms_level = 0.45f;
    lv_flow_v3_energy_update(&energy, loud, true, 33);
    const float attack = energy.features.energy;
    assert(attack > quiet_level);
    assert(attack < energy.features.target);

    for (uint32_t now_ms = 66; now_ms <= 1000; now_ms += 33) {
        lv_flow_v3_energy_update(&energy, loud, true, now_ms);
    }
    const float sustained = energy.features.energy;
    assert(std::fabs(sustained - energy.features.target) < 0.01f);

    lv_flow_v3_energy_update(&energy, quiet, true, 1033);
    const float release = energy.features.energy;
    assert(release < sustained);
    assert(release > energy.features.target);
}

static void test_flow_v3_energy_transport_interpolates_continuously() {
    lv_flow_v3_energy_t energy{};
    energy.travel_ms = 100;
    energy.history[0] = {195.0f, 255.0f, 0.2f, 0.2f,
                         0.0f, 0.0f, 0};
    energy.history[1] = {195.0f, 255.0f, 0.8f, 0.8f,
                         0.0f, 0.0f, 100};
    energy.history_count = 2;
    energy.history_head = 2;

    const auto halfway = lv_flow_v3_energy_sample(
        &energy, 0.5f, 100);
    assert(std::fabs(halfway.energy - 0.5f) < 0.001f);
    assert(std::fabs(halfway.body - 0.5f) < 0.001f);
}

static void test_flow_v3_energy_floor_preserves_bass_body() {
    lv_flow_v3_t bass{};
    bass.history[0] = {195.0f, 255.0f, 0.08f, 0.08f,
                       0.0f, 0.0f, 0};
    bass.history[1] = {195.0f, 255.0f, 0.08f, 0.08f,
                       0.0f, 0.0f, 100};
    bass.history_count = 2;
    bass.history_head = 2;

    lv_flow_v3_mid_t mids{};
    mids.history[0] = {300.0f, 255.0f, 1.0f, 1.0f,
                       0.0f, 0.0f, 0};
    mids.history[1] = {300.0f, 255.0f, 1.0f, 1.0f,
                       0.0f, 0.0f, 100};
    mids.history_count = 2;
    mids.history_head = 2;

    lv_flow_v3_energy_t energy{};
    energy.history[0] = {195.0f, 255.0f, 0.60f, 0.60f,
                         0.0f, 0.0f, 0};
    energy.history[1] = {195.0f, 255.0f, 0.60f, 0.60f,
                         0.0f, 0.0f, 100};
    energy.history_count = 2;
    energy.history_head = 2;

    const auto combined = lv_flow_v3_combined_energy_sample(
        &bass, &mids, &energy, 0.0f, 100);
    assert(std::fabs(combined.body - 0.08f) < 0.001f);
    assert(std::fabs(combined.energy - 0.60f) < 0.001f);
    assert(std::fabs(lv_flow_v3_energy_floor_body(combined.energy) -
                     0.132f) < 0.001f);
}

static void test_flow_v3_energy_floor_only_lifts_quiet_bass() {
    lv_flow_v3_slice_t quiet{};
    quiet.hue = 300.0f;
    quiet.body = 0.05f;
    quiet.energy = 0.50f;
    const auto quiet_baseline = lv_flow_v3_combined_color(quiet, 0.0f);
    const auto quiet_with_floor = lv_flow_v3_combined_energy_color(
        quiet, 0.0f);
    assert(lv_flow_v3_color_lightness(quiet_with_floor) >
           lv_flow_v3_color_lightness(quiet_baseline));

    lv_flow_v3_slice_t strong = quiet;
    strong.body = 0.50f;
    strong.energy = 1.0f;
    const auto strong_baseline = lv_flow_v3_combined_color(strong, 0.0f);
    const auto strong_with_floor = lv_flow_v3_combined_energy_color(
        strong, 0.0f);
    assert(strong_baseline.r == strong_with_floor.r);
    assert(strong_baseline.g == strong_with_floor.g);
    assert(strong_baseline.b == strong_with_floor.b);

    lv_flow_v3_slice_t no_energy = quiet;
    no_energy.energy = 0.0f;
    const auto no_energy_baseline = lv_flow_v3_combined_color(
        no_energy, 0.0f);
    const auto no_energy_floor = lv_flow_v3_combined_energy_color(
        no_energy, 0.0f);
    assert(no_energy_baseline.r == no_energy_floor.r);
    assert(no_energy_baseline.g == no_energy_floor.g);
    assert(no_energy_baseline.b == no_energy_floor.b);
}

static void test_flow_v3_attack_has_no_initial_or_falling_edge_flash() {
    lv_flow_v3_attack_t attack{};
    lv_channel_analysis_t steady[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v2_audio(0.3f, 3, 0.65f),
        flow_v2_audio(0.3f, 3, 0.65f),
    };
    lv_flow_v3_attack_update(&attack, steady, true, 0);
    assert(attack.features.attack == 0.0f);

    lv_channel_analysis_t quiet[LV_AUDIO_CHANNEL_COUNT]{};
    lv_flow_v3_attack_update(&attack, quiet, true, 33);
    assert(attack.features.attack == 0.0f);
}

static void test_flow_v3_attack_tracks_continuous_positive_rises() {
    lv_flow_v3_attack_t small{};
    lv_flow_v3_attack_t large{};
    lv_channel_analysis_t silence[LV_AUDIO_CHANNEL_COUNT]{};
    lv_flow_v3_attack_update(&small, silence, true, 0);
    lv_flow_v3_attack_update(&large, silence, true, 0);

    lv_channel_analysis_t small_rise[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v2_audio(0.2f, 4, 0.20f),
        flow_v2_audio(0.2f, 4, 0.20f),
    };
    lv_channel_analysis_t large_rise[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v2_audio(0.4f, 4, 0.80f),
        flow_v2_audio(0.4f, 4, 0.80f),
    };
    lv_flow_v3_attack_update(&small, small_rise, true, 33);
    lv_flow_v3_attack_update(&large, large_rise, true, 33);
    assert(small.features.attack > 0.0f);
    assert(large.features.attack > small.features.attack);

    const float initial = large.features.attack;
    for (uint32_t now_ms = 66; now_ms <= 1000; now_ms += 33) {
        lv_flow_v3_attack_update(&large, large_rise, true, now_ms);
    }
    assert(large.features.attack < initial * 0.05f);
}

static void test_flow_v3_attack_repeats_without_refractory_timer() {
    lv_flow_v3_attack_t attack{};
    lv_channel_analysis_t quiet[LV_AUDIO_CHANNEL_COUNT]{};
    lv_channel_analysis_t hit[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v2_audio(0.4f, 2, 0.80f),
        flow_v2_audio(0.4f, 2, 0.80f),
    };
    lv_flow_v3_attack_update(&attack, quiet, true, 0);
    lv_flow_v3_attack_update(&attack, hit, true, 33);
    const float first = attack.features.attack;
    lv_flow_v3_attack_update(&attack, quiet, true, 66);
    lv_flow_v3_attack_update(&attack, quiet, true, 99);
    lv_flow_v3_attack_update(&attack, hit, true, 132);
    const float second = attack.features.attack;
    assert(first > 0.0f);
    assert(second > 0.0f);
    assert(second > first * 0.50f);
}

static void test_flow_v3_attack_stereo_merge_is_phase_safe() {
    lv_flow_v3_attack_t left{};
    lv_flow_v3_attack_t right{};
    lv_channel_analysis_t silence[LV_AUDIO_CHANNEL_COUNT]{};
    lv_flow_v3_attack_update(&left, silence, true, 0);
    lv_flow_v3_attack_update(&right, silence, true, 0);

    lv_channel_analysis_t left_audio[LV_AUDIO_CHANNEL_COUNT]{};
    left_audio[0] = flow_v2_audio(0.3f, 5, 0.70f);
    lv_channel_analysis_t right_audio[LV_AUDIO_CHANNEL_COUNT]{};
    right_audio[1] = flow_v2_audio(0.3f, 5, 0.70f);
    lv_flow_v3_attack_update(&left, left_audio, true, 33);
    lv_flow_v3_attack_update(&right, right_audio, true, 33);
    assert(std::fabs(left.features.attack - right.features.attack) <
           0.001f);
}

static void test_flow_v3_attack_transport_interpolates_continuously() {
    lv_flow_v3_attack_t attack{};
    attack.travel_ms = 100;
    attack.history[0] = {195.0f, 255.0f, 0.0f, 0.0f,
                         0.2f, 0.0f, 0};
    attack.history[1] = {195.0f, 255.0f, 0.0f, 0.0f,
                         0.8f, 0.0f, 100};
    attack.history_count = 2;
    attack.history_head = 2;
    const auto halfway = lv_flow_v3_attack_sample(
        &attack, 0.5f, 100);
    assert(std::fabs(halfway.attack - 0.5f) < 0.001f);
}

static void test_flow_v3_attack_integration_is_inert_at_zero() {
    lv_flow_v3_t bass{};
    bass.travel_ms = 100;
    bass.history[0] = {195.0f, 255.0f, 0.15f, 0.15f,
                       0.0f, 0.0f, 0};
    bass.history[1] = {195.0f, 255.0f, 0.70f, 0.70f,
                       0.0f, 0.0f, 100};
    bass.history_count = 2;
    bass.history_head = 2;

    lv_flow_v3_mid_t mids{};
    mids.travel_ms = 100;
    mids.history[0] = {300.0f, 255.0f, 1.0f, 1.0f,
                       0.0f, 0.0f, 0};
    mids.history[1] = mids.history[0];
    mids.history[1].audio_time_ms = 100;
    mids.history_count = 2;
    mids.history_head = 2;

    lv_flow_v3_energy_t energy{};
    energy.travel_ms = 100;
    energy.history[0] = {195.0f, 255.0f, 0.5f, 0.5f,
                         0.0f, 0.0f, 0};
    energy.history[1] = energy.history[0];
    energy.history[1].audio_time_ms = 100;
    energy.history_count = 2;
    energy.history_head = 2;

    lv_flow_v3_attack_t attack{};
    attack.travel_ms = 100;
    attack.history[0] = {195.0f, 255.0f, 0.0f, 0.0f,
                         0.0f, 0.0f, 0};
    attack.history[1] = attack.history[0];
    attack.history[1].audio_time_ms = 100;
    attack.history_count = 2;
    attack.history_head = 2;

    const auto baseline = lv_flow_v3_combined_energy_sample(
        &bass, &mids, &energy, 0.4f, 100);
    const auto integrated = lv_flow_v3_combined_energy_attack_sample(
        &bass, &mids, &energy, &attack, 0.4f, 100);
    assert(std::fabs(integrated.body - baseline.body) < 0.000001f);
    assert(std::fabs(integrated.energy - baseline.energy) < 0.000001f);
    assert(std::fabs(integrated.hue - baseline.hue) < 0.000001f);
    assert(integrated.attack == 0.0f);
}

static void test_flow_v3_attack_integration_only_tightens_existing_body() {
    lv_flow_v3_t bass{};
    bass.travel_ms = 100;
    bass.history[0] = {195.0f, 255.0f, 0.0f, 0.0f,
                       0.0f, 0.0f, 0};
    bass.history[1] = {195.0f, 255.0f, 0.0f, 0.0f,
                       0.0f, 0.0f, 70};
    bass.history[2] = {195.0f, 255.0f, 1.0f, 1.0f,
                       0.0f, 0.0f, 100};
    bass.history_count = 3;
    bass.history_head = 3;

    lv_flow_v3_mid_t mids{};
    mids.travel_ms = 100;
    mids.history[0] = {300.0f, 255.0f, 1.0f, 1.0f,
                       0.0f, 0.0f, 0};
    mids.history[1] = mids.history[0];
    mids.history[1].audio_time_ms = 100;
    mids.history_count = 2;
    mids.history_head = 2;

    lv_flow_v3_energy_t energy{};
    energy.travel_ms = 100;
    energy.history[0] = {195.0f, 255.0f, 0.5f, 0.5f,
                         0.0f, 0.0f, 0};
    energy.history[1] = energy.history[0];
    energy.history[1].audio_time_ms = 100;
    energy.history_count = 2;
    energy.history_head = 2;

    lv_flow_v3_attack_t attack{};
    attack.travel_ms = 100;
    attack.history[0] = {195.0f, 255.0f, 0.0f, 0.0f,
                         1.0f, 0.0f, 0};
    attack.history[1] = attack.history[0];
    attack.history[1].audio_time_ms = 100;
    attack.history_count = 2;
    attack.history_head = 2;

    constexpr float coordinate = 0.10f;
    constexpr uint32_t now_ms = 100;
    const auto baseline = lv_flow_v3_combined_energy_sample(
        &bass, &mids, &energy, coordinate, now_ms);
    const auto raw = lv_flow_v3_sample(&bass, coordinate, now_ms);
    const auto integrated = lv_flow_v3_combined_energy_attack_sample(
        &bass, &mids, &energy, &attack, coordinate, now_ms);
    assert(std::fabs(integrated.body - raw.body) <
           std::fabs(baseline.body - raw.body));
    assert(integrated.body >= std::min(baseline.body, raw.body));
    assert(integrated.body <= std::max(baseline.body, raw.body));
    assert(integrated.energy == baseline.energy);
    assert(integrated.hue == baseline.hue);
    assert(integrated.attack == 1.0f);

    for (auto &slice : bass.history) {
        slice.body = 0.0f;
        slice.energy = 0.0f;
    }
    for (auto &slice : energy.history) {
        slice.body = 0.0f;
        slice.energy = 0.0f;
    }
    const auto silence = lv_flow_v3_combined_energy_attack_sample(
        &bass, &mids, &energy, &attack, coordinate, now_ms);
    assert(silence.body == 0.0f);
    assert(lv_flow_v3_energy_floor_body(silence.energy) == 0.0f);
}

static void test_flow_v3_texture_tracks_high_frequency_share() {
    lv_channel_analysis_t low[LV_AUDIO_CHANNEL_COUNT]{};
    low[0].mel_level[2] = 0.6f;
    low[1].mel_level[2] = 0.6f;
    assert(lv_flow_v3_texture_level(low) == 0.0f);

    lv_channel_analysis_t bright[LV_AUDIO_CHANNEL_COUNT]{};
    bright[0].mel_level[2] = 0.3f;
    bright[1].mel_level[2] = 0.3f;
    bright[0].mel_level[9] = 0.3f;
    bright[1].mel_level[9] = 0.3f;
    const float bright_share = lv_flow_v3_texture_level(bright);
    assert(bright_share > 0.0f);

    lv_channel_analysis_t louder = bright[0];
    for (float &level : louder.mel_level) level *= 2.0f;
    lv_channel_analysis_t louder_stereo[LV_AUDIO_CHANNEL_COUNT] = {
        louder,
        louder,
    };
    assert(std::fabs(lv_flow_v3_texture_level(louder_stereo) -
                     bright_share) < 0.001f);
}

static void test_flow_v3_texture_is_sustained_and_smooth() {
    lv_flow_v3_texture_t texture{};
    lv_channel_analysis_t quiet[LV_AUDIO_CHANNEL_COUNT]{};
    lv_flow_v3_texture_update(&texture, quiet, true, 0);

    lv_channel_analysis_t bright[LV_AUDIO_CHANNEL_COUNT]{};
    bright[0].mel_level[10] = 0.7f;
    bright[1].mel_level[10] = 0.7f;
    lv_flow_v3_texture_update(&texture, bright, true, 33);
    const float rising = texture.features.texture;
    assert(rising > 0.0f);
    assert(rising < texture.features.target);

    for (uint32_t now_ms = 66; now_ms <= 1000; now_ms += 33) {
        lv_flow_v3_texture_update(&texture, bright, true, now_ms);
    }
    const float sustained = texture.features.texture;
    assert(std::fabs(sustained - texture.features.target) < 0.01f);

    lv_flow_v3_texture_update(&texture, quiet, true, 1033);
    assert(texture.features.texture < sustained);
    assert(texture.features.texture > 0.0f);
}

static void test_flow_v3_texture_stereo_merge_is_phase_safe() {
    lv_channel_analysis_t left[LV_AUDIO_CHANNEL_COUNT]{};
    left[0].mel_level[1] = 0.2f;
    left[0].mel_level[11] = 0.5f;
    lv_channel_analysis_t right[LV_AUDIO_CHANNEL_COUNT]{};
    right[1] = left[0];
    assert(std::fabs(lv_flow_v3_texture_level(left) -
                     lv_flow_v3_texture_level(right)) < 0.001f);
}

static void test_flow_v3_texture_transport_interpolates_continuously() {
    lv_flow_v3_texture_t texture{};
    texture.travel_ms = 100;
    texture.history[0] = {330.0f, 255.0f, 0.2f, 0.2f,
                          0.0f, 0.2f, 0};
    texture.history[1] = {330.0f, 255.0f, 0.8f, 0.8f,
                          0.0f, 0.8f, 100};
    texture.history_count = 2;
    texture.history_head = 2;
    const auto halfway = lv_flow_v3_texture_sample(
        &texture, 0.5f, 100);
    assert(std::fabs(halfway.texture - 0.5f) < 0.001f);
    assert(std::fabs(halfway.body - 0.5f) < 0.001f);
}

static void test_flow_v3_texture_changes_slice_hue_not_lightness() {
    lv_flow_v3_slice_t smooth{};
    smooth.body = 1.0f;
    smooth.texture = 0.0f;
    const auto smooth_color = lv_flow_v3_texture_color(smooth, 0.0f);

    smooth.texture = 1.0f;
    const auto textured_color = lv_flow_v3_texture_color(smooth, 0.0f);
    assert(smooth_color.r != textured_color.r ||
           smooth_color.g != textured_color.g ||
           smooth_color.b != textured_color.b);
    assert(std::fabs(lv_flow_v3_color_lightness(smooth_color) -
                     lv_flow_v3_color_lightness(textured_color)) < 0.012f);
}

static void test_flow_v3_texture_renders_one_value_per_column() {
    lv_flow_v3_texture_t texture{};
    texture.travel_ms = 100;
    texture.history[0] = {330.0f, 255.0f, 1.0f, 1.0f,
                          0.0f, 0.0f, 0};
    texture.history[1] = {330.0f, 255.0f, 1.0f, 1.0f,
                          0.0f, 1.0f, 100};
    texture.history_count = 2;
    texture.history_head = 2;

    lv_rgb_t frame[LV_KEY_COUNT]{};
    lv_flow_v3_texture_render(&texture, 100, frame);
    const auto first = frame[lv_key_at(1, 0, 0)];
    for (int row = 1; row < 5; ++row) {
        const auto color = frame[lv_key_at(1, 0, row)];
        assert(color.r == first.r);
        assert(color.g == first.g);
        assert(color.b == first.b);
    }
}

static void test_flow_v3_texture_integration_is_bounded_and_equal_lightness() {
    lv_flow_v3_t bass{};
    bass.history[0] = {195.0f, 255.0f, 0.60f, 0.60f,
                       0.0f, 0.0f, 0};
    bass.history_count = 1;
    bass.history_head = 1;

    lv_flow_v3_mid_t mids{};
    mids.history[0] = {240.0f, 255.0f, 1.0f, 1.0f,
                       0.0f, 0.0f, 0};
    mids.history_count = 1;
    mids.history_head = 1;

    lv_flow_v3_energy_t energy{};
    energy.history[0] = {195.0f, 255.0f, 0.50f, 0.50f,
                         0.0f, 0.0f, 0};
    energy.history_count = 1;
    energy.history_head = 1;

    lv_flow_v3_attack_t attack{};
    attack.history[0] = {195.0f, 255.0f, 0.0f, 0.0f,
                         0.40f, 0.0f, 0};
    attack.history_count = 1;
    attack.history_head = 1;

    lv_flow_v3_texture_t texture{};
    texture.history[0] = {330.0f, 255.0f, 1.0f, 1.0f,
                          0.0f, 0.0f, 0};
    texture.history_count = 1;
    texture.history_head = 1;

    constexpr float coordinate = 0.0f;
    constexpr uint32_t now_ms = 100;
    const auto baseline = lv_flow_v3_combined_energy_attack_sample(
        &bass, &mids, &energy, &attack, coordinate, now_ms);
    const auto inert = lv_flow_v3_combined_energy_attack_texture_sample(
        &bass, &mids, &energy, &attack, &texture, coordinate, now_ms);
    assert(inert.hue == baseline.hue);
    assert(inert.body == baseline.body);
    assert(inert.energy == baseline.energy);
    assert(inert.attack == baseline.attack);
    assert(inert.texture == 0.0f);

    texture.history[0].texture = 1.0f;
    const auto textured = lv_flow_v3_combined_energy_attack_texture_sample(
        &bass, &mids, &energy, &attack, &texture, coordinate, now_ms);
    assert(std::fabs(textured.hue - baseline.hue - 32.0f) < 0.001f);
    assert(textured.body == baseline.body);
    assert(textured.energy == baseline.energy);
    assert(textured.attack == baseline.attack);
    assert(textured.texture == 1.0f);

    const auto baseline_color = lv_flow_v3_combined_energy_color(
        baseline, coordinate);
    const auto textured_color = lv_flow_v3_combined_energy_color(
        textured, coordinate);
    assert(std::fabs(lv_flow_v3_color_lightness(baseline_color) -
                     lv_flow_v3_color_lightness(textured_color)) < 0.012f);

    bass.history[0].body = 0.0f;
    bass.history[0].energy = 0.0f;
    energy.history[0].body = 0.0f;
    energy.history[0].energy = 0.0f;
    const auto silence = lv_flow_v3_combined_energy_attack_texture_sample(
        &bass, &mids, &energy, &attack, &texture, coordinate, now_ms);
    const auto off = lv_flow_v3_combined_energy_color(silence, coordinate);
    assert(off.r == 0 && off.g == 0 && off.b == 0);
}

static int flow_v3_service_calls = 0;

static void count_flow_v3_service_call() {
    ++flow_v3_service_calls;
}

static void test_flow_v3_engine_uses_the_accepted_compositor() {
    lv_flow_v3_engine_t engine{};
    lv_flow_v3_engine_init(&engine);
    assert(engine.bass.bass_contrast_enabled);
    assert(engine.bass.travel_ms == LV_FLOW_V3_DEFAULT_TRAVEL_MS);
    assert(engine.mids.travel_ms == LV_FLOW_V3_DEFAULT_TRAVEL_MS);
    assert(engine.energy.travel_ms == LV_FLOW_V3_DEFAULT_TRAVEL_MS);
    assert(engine.attack.travel_ms == LV_FLOW_V3_DEFAULT_TRAVEL_MS);
    assert(engine.texture.travel_ms == LV_FLOW_V3_DEFAULT_TRAVEL_MS);

    lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v3_audio(0.30f, 0.55f, 0.48f, 2, 0.70f),
        flow_v3_audio(0.30f, 0.40f, 0.35f, 9, 0.55f),
    };
    lv_flow_v3_engine_update(&engine, audio, true, 0);
    assert(engine.bass.history_count == 1);
    assert(engine.mids.history_count == 1);
    assert(engine.energy.history_count == 1);
    assert(engine.attack.history_count == 1);
    assert(engine.texture.history_count == 1);

    for (uint32_t now_ms = 33; now_ms <= 990; now_ms += 33) {
        audio[0].band_level[0] = 0.20f +
                                  0.50f * (float)(now_ms % 99) / 99.0f;
        audio[0].mel_level[(now_ms / 33) % LV_MEL_BAND_COUNT] = 0.65f;
        lv_flow_v3_engine_update(&engine, audio, true, now_ms);
    }

    constexpr uint32_t now_ms = 990;
    lv_rgb_t engine_frame[LV_KEY_COUNT]{};
    lv_rgb_t cooperative_frame[LV_KEY_COUNT]{};
    lv_rgb_t direct_frame[LV_KEY_COUNT]{};
    lv_flow_v3_engine_render(&engine, now_ms, engine_frame);
    flow_v3_service_calls = 0;
    lv_flow_v3_engine_render(&engine, now_ms, cooperative_frame,
                             count_flow_v3_service_call);
    assert(flow_v3_service_calls == LV_SPECTRAL_FLOW_COLUMNS +
                                        LV_AUDIO_CHANNEL_COUNT);
    assert(std::memcmp(engine_frame, cooperative_frame,
                       sizeof(engine_frame)) == 0);
    const auto color_at = [&engine](float coordinate) {
        const auto sample =
            lv_flow_v3_combined_energy_attack_texture_sample(
                &engine.bass, &engine.mids, &engine.energy,
                &engine.attack, &engine.texture, coordinate, now_ms);
        return lv_flow_v3_combined_energy_color(sample, coordinate);
    };
    const auto set_direct = [&direct_frame](int key, lv_rgb_t color) {
        if (key >= 0 && key < LV_KEY_COUNT) direct_frame[key] = color;
    };
    for (int half = 0; half < LV_AUDIO_CHANNEL_COUNT; ++half) {
        for (int col = 0; col < LV_SPECTRAL_FLOW_COLUMNS; ++col) {
            const int distance = half == 0
                                     ? LV_SPECTRAL_FLOW_COLUMNS - 1 - col
                                     : col;
            for (int row = 0; row < 5; ++row) {
                const float coordinate = lv_spectral_flow_coordinate(
                    half, (float)distance, (float)row);
                set_direct(lv_key_at(half, col, row),
                           color_at(coordinate));
            }
        }
        for (int key = 0; key < 3; ++key) {
            const float coordinate = lv_spectral_flow_coordinate(
                half, 0.0f, (float)(key + 1));
            set_direct(lv_utility_key_at(half, key),
                       color_at(coordinate));
        }
        constexpr float thumb_distance[4] = {
            1.25f, 0.85f, 0.45f, 0.0f,
        };
        for (int key = 0; key < 4; ++key) {
            const float coordinate = lv_spectral_flow_coordinate(
                half, thumb_distance[key], 3.2f + key * 0.2f);
            set_direct(lv_thumb_key_at(half, key),
                       color_at(coordinate));
        }
    }
    assert(std::memcmp(engine_frame, direct_frame,
                       sizeof(engine_frame)) == 0);
}

static void test_flow_v3_transport_interpolates_between_slices() {
    lv_flow_v3_t flow{};
    flow.travel_ms = 100;
    flow.history[0] = {20.0f, 240.0f, 0.2f, 0.2f, 0.0f, 0.0f, 0};
    flow.history[1] = {100.0f, 240.0f, 0.8f, 0.8f, 0.6f, 0.0f, 100};
    flow.history_count = 2;
    flow.history_head = 2;

    const auto halfway = lv_flow_v3_sample(&flow, 0.5f, 100);
    assert(std::fabs(halfway.hue - 60.0f) < 0.01f);
    assert(std::fabs(halfway.body - 0.5f) < 0.01f);
    assert(std::fabs(halfway.attack - 0.3f) < 0.01f);
}

static void test_flow_v2_closed_gate_cannot_amplify_noise() {
    lv_flow_v2_gain_t gain{};
    assert(std::fabs(lv_flow_v2_gain_update(&gain, 0.01f, false, 0) -
                     1.0f) < 0.001f);
    for (uint32_t now_ms = 33; now_ms <= 10000; now_ms += 33) {
        lv_flow_v2_gain_update(&gain, 0.02f, false, now_ms);
    }
    assert(std::fabs(gain.gain - 1.0f) < 0.001f);

    lv_flow_v2_t flow{};
    lv_channel_analysis_t noise[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v2_audio(0.02f, 2, 0.02f),
        flow_v2_audio(0.015f, 2, 0.015f),
    };
    for (uint32_t now_ms = 0; now_ms <= 10000; now_ms += 33) {
        lv_flow_v2_update(&flow, noise, false, now_ms);
    }
    assert(std::fabs(flow.features.gain - 1.0f) < 0.001f);
    assert(flow.features.channel[0].value == 0.0f);
    assert(flow.features.channel[1].value == 0.0f);
    assert(!flow.features.gate_open);
}

static void test_flow_v2_gain_is_bounded_and_recovers_slowly() {
    lv_flow_v2_gain_t gain{};
    const float quiet_gain =
        lv_flow_v2_gain_update(&gain, 0.03f, true, 0);
    assert(quiet_gain > 7.9f && quiet_gain <= 8.0f);

    const float after_loud_step =
        lv_flow_v2_gain_update(&gain, 0.70f, true, 33);
    assert(after_loud_step >= 0.5f && after_loud_step < quiet_gain);

    float recovering = after_loud_step;
    for (uint32_t now_ms = 66; now_ms <= 396; now_ms += 33) {
        recovering = lv_flow_v2_gain_update(&gain, 0.03f, true, now_ms);
    }
    assert(recovering > after_loud_step);
    assert(recovering < quiet_gain);
}

static void test_flow_v2_uses_one_gain_without_collapsing_stereo() {
    lv_flow_v2_t flow{};
    lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v2_audio(0.12f, 4, 0.30f),
        flow_v2_audio(0.06f, 4, 0.15f),
    };
    lv_flow_v2_update(&flow, audio, true, 100);

    assert(flow.features.gain == flow.gain.gain);
    assert(flow.features.channel[0].value >
           flow.features.channel[1].value);
    assert(flow.features.channel[0].raw_level >
           flow.features.channel[1].raw_level);
}

static void test_flow_v2_packet_value_follows_short_term_energy() {
    lv_flow_v2_t flow{};
    lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v2_audio(0.08f, 4, 0.20f),
        flow_v2_audio(0.08f, 4, 0.20f),
    };
    lv_flow_v2_update(&flow, audio, true, 0);
    const float quiet = flow.features.channel[0].value;

    audio[0] = flow_v2_audio(0.32f, 4, 0.80f);
    audio[1] = flow_v2_audio(0.32f, 4, 0.80f);
    lv_flow_v2_update(&flow, audio, true, 33);
    const float loud = flow.features.channel[0].value;
    assert(loud > quiet + 10.0f);

    audio[0] = flow_v2_audio(0.08f, 4, 0.20f);
    audio[1] = flow_v2_audio(0.08f, 4, 0.20f);
    lv_flow_v2_update(&flow, audio, true, 66);
    assert(flow.features.channel[0].value < loud);
    assert(flow.features.channel[0].value > quiet);
}

static void test_flow_v2_centroid_palette_is_ordered_and_vivid() {
    const auto low = lv_flow_v2_palette(212.0f, 1.0f);
    const auto mid = lv_flow_v2_palette(1135.0f, 1.0f);
    const auto high = lv_flow_v2_palette(6084.0f, 1.0f);
    assert(low.hue <= mid.hue && mid.hue <= high.hue);
    assert(high.hue - low.hue > 200.0f);
    assert(low.saturation >= 247.0f);
    assert(mid.saturation >= 247.0f);
    assert(high.saturation >= 247.0f);
    assert(low.value_scale >= 254.0f);
    assert(mid.value_scale >= 254.0f);
    assert(high.value_scale >= 254.0f);

    const auto balanced = lv_flow_v2_palette(1135.0f, 0.0f);
    assert(balanced.saturation >= 247.0f);
    assert(balanced.saturation < mid.saturation);

    const float quiet_color = lv_flow_v2_palette_position(342.0f);
    const float bright_color = lv_flow_v2_palette_position(743.0f);
    assert(quiet_color >= 0.0f && bright_color <= 1.0f);
    assert(bright_color - quiet_color > 0.52f);
}

static void test_flow_v2_smooths_centroid_in_log_frequency_space() {
    lv_flow_v2_t flow{};
    lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v2_audio(0.10f, 0, 0.60f),
        flow_v2_audio(0.10f, 0, 0.60f),
    };
    lv_flow_v2_update(&flow, audio, true, 0);
    const float low_centroid = flow.features.channel[0].centroid_hz;

    audio[0] = flow_v2_audio(0.10f, 11, 0.60f);
    audio[1] = flow_v2_audio(0.10f, 11, 0.60f);
    lv_flow_v2_update(&flow, audio, true, 33);
    const float smoothed = flow.features.channel[0].centroid_hz;
    assert(smoothed > low_centroid);
    assert(smoothed < 6084.0f);

    const float expected_log =
        std::log2(low_centroid) +
        (std::log2(6084.0f) - std::log2(low_centroid)) *
            (1.0f - std::exp(-33.0f / 140.0f));
    assert(std::fabs(std::log2(smoothed) - expected_log) < 0.02f);
}

static void test_flow_v2_shared_low_onset_detects_close_distinct_hits() {
    lv_flow_v2_t flow{};
    lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v2_audio_group(0.08f, 0, 3, 0.02f),
        flow_v2_audio_group(0.08f, 0, 3, 0.02f),
    };
    lv_flow_v2_update(&flow, audio, true, 0);
    assert(flow.features.onset_low == 0.0f);

    // The first hit is hard-panned right. Shared onset analysis must still
    // launch one event for both keyboard halves.
    audio[1] = flow_v2_audio_group(0.30f, 0, 3, 0.85f);
    lv_flow_v2_update(&flow, audio, true, 33);
    assert(flow.features.onset_low > 0.5f);
    const float first_hit = flow.features.onset_low;

    audio[1] = flow_v2_audio_group(0.12f, 0, 3, 0.18f);
    lv_flow_v2_update(&flow, audio, true, 66);
    assert(flow.features.onset_low == 0.0f);
    audio[1] = flow_v2_audio_group(0.09f, 0, 3, 0.08f);
    lv_flow_v2_update(&flow, audio, true, 99);
    assert(flow.features.onset_low == 0.0f);

    // A second real hit 99 ms later must survive the refractory period even
    // though the base brightness envelope has not fully released.
    audio[1] = flow_v2_audio_group(0.30f, 0, 3, 0.85f);
    lv_flow_v2_update(&flow, audio, true, 132);
    assert(flow.features.onset_low > 0.5f);
    assert(flow.features.onset_low > first_hit * 0.65f);
}

static void test_flow_v2_onsets_are_band_specific_and_do_not_retrigger_sustain() {
    for (int onset_band = 0; onset_band < 3; ++onset_band) {
        lv_flow_v2_t flow{};
        lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT] = {
            flow_v2_audio_group(0.08f, 0, 11, 0.02f),
            flow_v2_audio_group(0.08f, 0, 11, 0.02f),
        };
        lv_flow_v2_update(&flow, audio, true, 0);

        const int first = onset_band * 4;
        const int last = first + 3;
        audio[0] = flow_v2_audio_group(0.25f, first, last, 0.75f);
        audio[1] = audio[0];
        lv_flow_v2_update(&flow, audio, true, 33);
        const float onset[3] = {
            flow.features.onset_low,
            flow.features.onset_mid,
            flow.features.onset_high,
        };
        assert(onset[onset_band] > 0.5f);
        assert(onset[(onset_band + 1) % 3] == 0.0f);
        assert(onset[(onset_band + 2) % 3] == 0.0f);

        for (uint32_t now_ms = 66; now_ms <= 330; now_ms += 33) {
            lv_flow_v2_update(&flow, audio, true, now_ms);
            assert(flow.features.onset_low == 0.0f);
            assert(flow.features.onset_mid == 0.0f);
            assert(flow.features.onset_high == 0.0f);
        }
    }
}

static void test_flow_v2_routes_fft_onset_fixtures_to_low_mid_and_high() {
    const int fixture_bin[3] = {4, 26, 82};
    const auto silence = audio_tone(4, 0.0f);
    for (int expected_band = 0; expected_band < 3; ++expected_band) {
        lv_flow_v2_t flow{};
        lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT]{};
        for (auto &channel : audio) {
            lv_analyze_channel(&channel, silence.data(), 1.0f);
        }
        lv_flow_v2_update(&flow, audio, false, 0);

        const auto fixture = audio_tone(fixture_bin[expected_band], 0.45f);
        for (auto &channel : audio) {
            lv_analyze_channel(&channel, fixture.data(), 1.0f);
        }
        lv_flow_v2_update(&flow, audio, true, 33);
        const float onset[3] = {
            flow.features.onset_low,
            flow.features.onset_mid,
            flow.features.onset_high,
        };
        assert(onset[expected_band] > 0.0f);
        assert(onset[expected_band] >= onset[(expected_band + 1) % 3]);
        assert(onset[expected_band] >= onset[(expected_band + 2) % 3]);
    }
}

static void test_flow_v2_bass_can_follow_mid_event_inside_shared_refractory() {
    lv_flow_v2_t flow{};
    lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v2_audio_group(0.08f, 0, 11, 0.02f),
        flow_v2_audio_group(0.08f, 0, 11, 0.02f),
    };
    lv_flow_v2_update(&flow, audio, true, 0);

    audio[0] = flow_v2_audio_group(0.25f, 4, 7, 0.80f);
    audio[1] = audio[0];
    lv_flow_v2_update(&flow, audio, true, 33);
    assert(flow.features.onset_mid > 0.0f);
    assert(flow.features.onset_low == 0.0f);

    audio[0] = flow_v2_audio_group(0.25f, 0, 3, 0.80f);
    audio[1] = audio[0];
    lv_flow_v2_update(&flow, audio, true, 66);
    assert(flow.features.onset_low > 0.0f);
}

static void test_flow_v2_mid_event_cannot_displace_recent_bass() {
    lv_flow_v2_t flow{};
    lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT] = {
        flow_v2_audio_group(0.08f, 0, 11, 0.02f),
        flow_v2_audio_group(0.08f, 0, 11, 0.02f),
    };
    lv_flow_v2_update(&flow, audio, true, 0);

    audio[0] = flow_v2_audio_group(0.25f, 0, 3, 0.80f);
    audio[1] = audio[0];
    lv_flow_v2_update(&flow, audio, true, 33);
    assert(flow.features.onset_low > 0.0f);

    audio[0] = flow_v2_audio_group(0.25f, 4, 7, 0.80f);
    audio[1] = audio[0];
    lv_flow_v2_update(&flow, audio, true, 66);
    assert(flow.features.onset_mid == 0.0f);
}

static void test_flow_v2_event_wavefront_is_narrow_and_music_colored() {
    lv_spectral_flow_t flow{};
    lv_spectral_flow_set_distance_decay(&flow, 1.0f);
    flow.travel_ms = 110;
    lv_spectral_flow_update_packet(&flow, 150.0f, 255.0f, 190.0f,
                                   0.0f, 0);
    lv_spectral_flow_update_packet(&flow, 150.0f, 255.0f, 190.0f,
                                   1.0f, 33);
    lv_spectral_flow_update_packet(&flow, 150.0f, 255.0f, 190.0f,
                                   0.0f, 66);

    const auto source = lv_spectral_flow_color(&flow, 0.0f, 66);
    const auto crest = lv_spectral_flow_color(&flow, 0.30f, 66);
    const auto beyond = lv_spectral_flow_color(&flow, 0.60f, 66);
    assert(rgb_brightness(crest) >= 250);
    assert(rgb_brightness(crest) > rgb_brightness(source) + 45);
    assert(rgb_brightness(crest) > rgb_brightness(beyond) + 45);
    // The event inherits the packet's music-derived hue instead of replacing
    // it with a fixed white or red flash.
    assert(crest.b > crest.r && crest.b > crest.g);
    assert(crest.r < 10);
}

static void test_flow_v2_close_events_keep_two_separate_crests() {
    lv_spectral_flow_t flow{};
    lv_spectral_flow_set_distance_decay(&flow, 1.0f);
    flow.travel_ms = 110;
    lv_spectral_flow_update_packet(&flow, 30.0f, 255.0f, 190.0f,
                                   0.0f, 0);
    lv_spectral_flow_update_packet(&flow, 30.0f, 255.0f, 190.0f,
                                   1.0f, 33);
    lv_spectral_flow_update_packet(&flow, 30.0f, 255.0f, 190.0f,
                                   0.0f, 66);
    lv_spectral_flow_update_packet(&flow, 30.0f, 255.0f, 190.0f,
                                   1.0f, 99);
    lv_spectral_flow_update_packet(&flow, 30.0f, 255.0f, 190.0f,
                                   0.0f, 132);

    const auto recent_crest = lv_spectral_flow_color(&flow, 0.30f, 132);
    const auto valley = lv_spectral_flow_color(&flow, 0.60f, 132);
    const auto older_crest = lv_spectral_flow_color(&flow, 0.90f, 132);
    assert(rgb_brightness(recent_crest) > rgb_brightness(valley) + 40);
    assert(rgb_brightness(older_crest) > rgb_brightness(valley) + 40);
}

static void test_spectral_flow_maps_frequency_regions_to_vivid_hues() {
    float low_bands[LV_SPECTRAL_FLOW_BANDS] = {};
    low_bands[0] = 1.0f;
    lv_spectral_flow_t low_flow{};
    lv_spectral_flow_update(&low_flow, low_bands, 0.0f, 0);
    const auto low = lv_spectral_flow_color(&low_flow, 0.0f, 0);
    assert(low.r >= 240 && low.g < 40 && low.b < 40);

    float mid_bands[LV_SPECTRAL_FLOW_BANDS] = {};
    mid_bands[5] = 1.0f;
    lv_spectral_flow_t mid_flow{};
    lv_spectral_flow_update(&mid_flow, mid_bands, 0.0f, 0);
    const auto mid = lv_spectral_flow_color(&mid_flow, 0.0f, 0);
    assert(mid.g >= 240 && mid.g > mid.r && mid.g > mid.b);

    float high_bands[LV_SPECTRAL_FLOW_BANDS] = {};
    high_bands[11] = 1.0f;
    lv_spectral_flow_t high_flow{};
    lv_spectral_flow_update(&high_flow, high_bands, 0.0f, 0);
    const auto high = lv_spectral_flow_color(&high_flow, 0.0f, 0);
    assert(high.b >= 240 && high.b > high.r && high.b > high.g);

    assert(std::min({low.r, low.g, low.b}) == 0);
    assert(std::min({mid.r, mid.g, mid.b}) == 0);
    assert(std::min({high.r, high.g, high.b}) == 0);
}

static void test_spectral_flow_adjacent_bands_in_one_lane_do_not_blink() {
    float bands[LV_SPECTRAL_FLOW_BANDS] = {};
    bands[5] = 1.0f;
    lv_spectral_flow_t flow{};
    lv_spectral_flow_update(&flow, bands, 0.0f, 0);
    const float first_hue = flow.base_hue;

    bands[5] = 0.0f;
    bands[6] = 1.0f;
    lv_spectral_flow_update(&flow, bands, 0.0f,
                            LV_SPECTRAL_FLOW_FRAME_MS);
    assert(std::fabs(flow.base_hue - first_hue) < 1.0f);
}

static void test_spectral_flow_limits_hue_motion_per_frame() {
    float bands[LV_SPECTRAL_FLOW_BANDS] = {};
    bands[0] = 1.0f;
    lv_spectral_flow_t flow{};
    lv_spectral_flow_update(&flow, bands, 0.0f, 0);

    bands[0] = 0.0f;
    bands[11] = 1.0f;
    float previous_hue = flow.base_hue;
    for (uint32_t now_ms = LV_SPECTRAL_FLOW_FRAME_MS;
         now_ms <= 40 * LV_SPECTRAL_FLOW_FRAME_MS;
         now_ms += LV_SPECTRAL_FLOW_FRAME_MS) {
        lv_spectral_flow_update(&flow, bands, 0.0f, now_ms);
        assert(std::fabs(flow.base_hue - previous_hue) <= 8.01f);
        previous_hue = flow.base_hue;
    }
    const auto high = lv_spectral_flow_color(
        &flow, 0.0f, 40 * LV_SPECTRAL_FLOW_FRAME_MS);
    assert(high.b > high.r && high.b > high.g);
}

static void test_kick_detector_fires_on_a_sharp_bass_rise_once() {
    lv_kick_detector_t detector{};
    assert(!lv_kick_detector_update(&detector, 0.05f, 0).triggered);
    assert(!lv_kick_detector_update(&detector, 0.05f, 33).triggered);

    const auto kick = lv_kick_detector_update(&detector, 0.70f, 66);
    assert(kick.triggered);
    assert(kick.strength > 0.5f);
    assert(!lv_kick_detector_update(&detector, 0.65f, 99).triggered);
    assert(!lv_kick_detector_update(&detector, 0.60f, 132).triggered);
}

static void test_kick_detector_ignores_gradual_or_sustained_bass() {
    lv_kick_detector_t detector{};
    assert(!lv_kick_detector_update(&detector, 0.05f, 0).triggered);
    assert(!lv_kick_detector_update(&detector, 0.08f, 33).triggered);
    assert(!lv_kick_detector_update(&detector, 0.11f, 66).triggered);
    assert(!lv_kick_detector_update(&detector, 0.14f, 99).triggered);
    assert(!lv_kick_detector_update(&detector, 0.17f, 132).triggered);
    assert(!lv_kick_detector_update(&detector, 0.17f, 165).triggered);
}

static void test_spectral_flow_holds_attack_accent_for_a_visible_segment() {
    lv_spectral_flow_t flow{};
    float bands[LV_SPECTRAL_FLOW_BANDS] = {};
    bands[6] = 0.7f;
    const lv_kick_event_t event{true, 1.0f};

    lv_spectral_flow_update(&flow, bands, 0.0f, 0, &event);
    const auto initial = lv_spectral_flow_color(&flow, 0.0f, 0);
    for (uint32_t now_ms = 33; now_ms < LV_SPECTRAL_KICK_HOLD_MS;
         now_ms += 33) {
        lv_spectral_flow_update(&flow, bands, 0.0f, now_ms, nullptr);
        const auto held = lv_spectral_flow_color(&flow, 0.0f, now_ms);
        assert(std::abs((int)held.r - (int)initial.r) <= 1);
        assert(std::abs((int)held.g - (int)initial.g) <= 1);
        assert(std::abs((int)held.b - (int)initial.b) <= 1);
    }
    assert(initial.r > initial.b);
}

static void test_spectral_flow_latches_kick_brightness_for_the_whole_packet() {
    float bands[LV_SPECTRAL_FLOW_BANDS] = {};
    bands[6] = 0.7f;
    const lv_kick_event_t kick{true, 1.0f};
    lv_spectral_flow_t kick_flow{};

    lv_spectral_flow_update(&kick_flow, bands, 0.0f, 0, &kick);
    const auto initial = lv_spectral_flow_color(&kick_flow, 0.0f, 0);
    bands[6] = 0.05f;
    bands[10] = 1.0f;
    for (uint32_t now_ms = 33; now_ms < LV_SPECTRAL_KICK_HOLD_MS;
         now_ms += 33) {
        lv_spectral_flow_update(&kick_flow, bands, 0.0f, now_ms, nullptr);
        const auto held = lv_spectral_flow_color(&kick_flow, 0.0f, now_ms);
        assert(std::memcmp(&held, &initial, sizeof(held)) == 0);
    }
}

static void test_visualizer_shares_attack_timing_across_stereo_halves() {
    lv_visualizer_t visualizer{};
    lv_visualizer_set_flow_kick_packets(&visualizer, true);
    lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT]{};
    lv_rgb_t frame[LV_KEY_COUNT]{};
    for (auto &channel : audio) {
        channel.rms_level = 0.1f;
        channel.mel_level[5] = 0.08f;
        channel.bass_env = 0.05f;
    }
    lv_visualizer_render(&visualizer, lv_viz_mode_t::FLOW, audio, true,
                         0, frame);

    audio[0].mel_level[5] = 0.8f;
    audio[0].bass_env = 0.8f;
    lv_visualizer_render(&visualizer, lv_viz_mode_t::FLOW, audio, true,
                         33, frame);

    assert(visualizer.flow[0].accent_until_ms ==
           33 + LV_SPECTRAL_KICK_HOLD_MS);
    assert(visualizer.flow[1].accent_until_ms ==
           visualizer.flow[0].accent_until_ms);
    assert(std::fabs(visualizer.flow[0].accent_hue -
                     visualizer.flow[1].accent_hue) < 0.001f);
    assert(std::fabs(visualizer.flow[0].accent_value -
                     visualizer.flow[1].accent_value) < 0.001f);
}

static void test_spectral_flow_keeps_brightness_stable_while_audio_is_present() {
    float bands[LV_SPECTRAL_FLOW_BANDS] = {};
    bands[0] = 1.0f;
    lv_spectral_flow_t flow{};
    lv_spectral_flow_update(&flow, bands, 0.0f, 0);
    bands[0] = 0.1f;
    lv_spectral_flow_update(&flow, bands, 0.0f,
                            LV_SPECTRAL_FLOW_FRAME_MS);
    const auto quiet = lv_spectral_flow_color(
        &flow, 0.0f, LV_SPECTRAL_FLOW_FRAME_MS);
    assert(quiet.r == 255);
    assert(quiet.g == 0 && quiet.b == 0);
}

static void test_spectral_flow_balanced_spectrum_remains_saturated() {
    float bands[LV_SPECTRAL_FLOW_BANDS] = {};
    bands[0] = 1.0f;
    bands[5] = 1.0f;
    bands[11] = 1.0f;
    lv_spectral_flow_t flow{};
    lv_spectral_flow_update(&flow, bands, 0.0f, 0);

    const auto combined = lv_spectral_flow_color(&flow, 0.0f, 0);
    assert(rgb_brightness(combined) >= 240);
    assert(std::min({combined.r, combined.g, combined.b}) == 0);
}

static void test_spectral_flow_ribbon_propagates_every_intermediate_frame() {
    float bands[LV_SPECTRAL_FLOW_BANDS] = {};
    bands[0] = 1.0f;
    lv_spectral_flow_t flow{};
    lv_spectral_flow_update(&flow, bands, 0.0f, 0);

    bands[11] = 1.0f;
    lv_rgb_t inner_at_source_time{};
    lv_rgb_t previous_delayed{};
    int changed_frames = 0;
    for (uint32_t now_ms = 10;
         now_ms <= 2 * LV_SPECTRAL_FLOW_TRAVEL_MS; now_ms += 10) {
        // The analyzer releases an old band over several frames rather than
        // producing an impossible full-scale discontinuity.
        bands[0] *= 0.84f;
        lv_spectral_flow_update(&flow, bands, 0.0f, now_ms);
        if (now_ms == LV_SPECTRAL_FLOW_TRAVEL_MS) {
            // Stay far enough from the center edge that both positions sample
            // the same five-tap neighborhood one travel interval apart.
            inner_at_source_time =
                lv_spectral_flow_color(&flow, 0.7f, now_ms);
        }
        if (now_ms >= LV_SPECTRAL_FLOW_TRAVEL_MS) {
            const auto delayed = lv_spectral_flow_color(&flow, 1.7f, now_ms);
            if (now_ms > LV_SPECTRAL_FLOW_TRAVEL_MS &&
                std::memcmp(&delayed, &previous_delayed, sizeof(delayed)) != 0) {
                ++changed_frames;
            }
            previous_delayed = delayed;
        }
    }

    const uint32_t delayed_time = 2 * LV_SPECTRAL_FLOW_TRAVEL_MS;
    const auto delayed =
        lv_spectral_flow_color(&flow, 1.7f, delayed_time);
    assert(std::abs((int)delayed.r -
                    (int)(inner_at_source_time.r * 0.97f + 0.5f)) <= 2);
    assert(std::abs((int)delayed.g -
                    (int)(inner_at_source_time.g * 0.97f + 0.5f)) <= 2);
    assert(std::abs((int)delayed.b -
                    (int)(inner_at_source_time.b * 0.97f + 0.5f)) <= 2);
    // Color slew limiting deliberately trades a few immediate changes for
    // stability, but the traveling field must still advance on most frames.
    assert(changed_frames >= 6);

    for (int distance = 0; distance < LV_SPECTRAL_FLOW_COLUMNS; ++distance) {
        assert(rgb_brightness(lv_spectral_flow_color(
                   &flow, (float)distance, delayed_time)) > 0);
    }
}

static void test_spectral_flow_sustained_music_keeps_defining_the_source() {
    float bands[LV_SPECTRAL_FLOW_BANDS] = {};
    bands[6] = 0.7f;
    lv_spectral_flow_t flow{};
    lv_spectral_flow_update(&flow, bands, 0.0f, 0);

    for (uint32_t now_ms = LV_SPECTRAL_FLOW_FRAME_MS;
         now_ms <= 330; now_ms += LV_SPECTRAL_FLOW_FRAME_MS) {
        lv_spectral_flow_update(&flow, bands, 0.0f, now_ms);
    }

    const auto source = lv_spectral_flow_color(&flow, 0.0f, 330);
    assert(source.g >= 240);
    assert(source.r < 40 && source.b < 40);
}

static void test_visualizer_flow_travel_can_be_tuned_without_rebuilding() {
    lv_visualizer_t visualizer{};
    assert(lv_visualizer_flow_travel_ms(&visualizer) ==
           LV_SPECTRAL_FLOW_TRAVEL_MS);

    lv_visualizer_set_flow_travel_ms(&visualizer, 140);
    assert(lv_visualizer_flow_travel_ms(&visualizer) == 140);
    assert(visualizer.flow[0].travel_ms == 140);
    assert(visualizer.flow[1].travel_ms == 140);

    lv_visualizer_set_flow_travel_ms(&visualizer, 0);
    assert(lv_visualizer_flow_travel_ms(&visualizer) ==
           LV_SPECTRAL_FLOW_TRAVEL_MS);
}

static void test_spectral_flow_coordinates_stagger_every_row() {
    float previous_left = -1.0f;
    float previous_right = 100.0f;
    for (int row = 0; row < 5; ++row) {
        const float left = lv_spectral_flow_coordinate(0, 2.0f, (float)row);
        const float right = lv_spectral_flow_coordinate(1, 2.0f, (float)row);
        assert(left > previous_left);
        assert(right < previous_right);
        previous_left = left;
        previous_right = right;
    }
    assert(lv_spectral_flow_coordinate(0, 2.0f, 4.0f) -
               lv_spectral_flow_coordinate(0, 2.0f, 0.0f) >=
           0.4f);
}

static void test_spectral_flow_blurs_a_sharp_boundary_across_keys() {
    lv_spectral_flow_t flow{};
    flow.initialized = true;
    flow.history_count = 4;
    flow.history_head = 3;
    flow.history[0] = {0.0f, 255.0f, 0.0f, 255.0f, 0};
    flow.history[1] = {
        0.0f, 255.0f, 0.0f, 255.0f,
        1000 - LV_SPECTRAL_FLOW_TRAVEL_MS - 1};
    flow.history[2] = {
        -86.0f, 255.0f, 0.0f, 255.0f,
        1000 - LV_SPECTRAL_FLOW_TRAVEL_MS};
    flow.history[3] = {
        -86.0f, 255.0f, 0.0f, 255.0f, 1000};

    const auto blue = lv_spectral_flow_color(&flow, 0.0f, 1000);
    const auto blue_edge = lv_spectral_flow_color(&flow, 0.6f, 1000);
    const auto red_edge = lv_spectral_flow_color(&flow, 1.4f, 1000);
    const auto red = lv_spectral_flow_color(&flow, 2.2f, 1000);

    assert(blue.b > blue.r && blue.b > blue.g);
    assert(red.r > red.g && red.r > red.b);
    assert(blue_edge.r >= 15 && blue_edge.b >= 15);
    assert(red_edge.r >= 15 && red_edge.b >= 15);
}

static void test_spectral_flow_has_no_motion_unrelated_to_audio_history() {
    float bands[LV_SPECTRAL_FLOW_BANDS] = {};
    bands[0] = 1.0f;
    lv_spectral_flow_t flow{};
    lv_spectral_flow_update(&flow, bands, 0.0f, 0);

    const uint32_t dim_time = 3 * LV_SPECTRAL_FLOW_TRAVEL_MS / 4;
    const uint32_t bright_time = 2 * LV_SPECTRAL_FLOW_TRAVEL_MS;
    const int dim = rgb_brightness(
        lv_spectral_flow_color(&flow, 2.0f, dim_time));
    const int bright = rgb_brightness(
        lv_spectral_flow_color(&flow, 2.0f, bright_time));
    assert(std::abs(bright - dim) <= 1);
}

static void test_shared_visualizer_uses_idle_when_audio_is_quiet() {
    lv_channel_analysis_t silence[LV_AUDIO_CHANNEL_COUNT]{};
    lv_visualizer_t flow_state{};
    lv_visualizer_t idle_state{};
    lv_rgb_t quiet_flow[LV_KEY_COUNT]{};
    lv_rgb_t idle[LV_KEY_COUNT]{};

    lv_visualizer_render(&flow_state, lv_viz_mode_t::FLOW, silence, false,
                         1234, quiet_flow);
    lv_visualizer_render(&idle_state, lv_viz_mode_t::IDLE, silence, false,
                         1234, idle);

    assert(std::memcmp(quiet_flow, idle, sizeof(idle)) == 0);
    for (const auto color : idle) assert(rgb_brightness(color) > 0);
}

static void test_vu_rows_fill_smoothly_from_green_to_red() {
    const auto empty_bottom = lv_vu_row_color(0.0f, 0, 5);
    const auto full_bottom = lv_vu_row_color(1.0f, 0, 5);
    const auto full_top = lv_vu_row_color(1.0f, 4, 5);
    assert(rgb_brightness(empty_bottom) < rgb_brightness(full_bottom));
    assert(full_bottom.g > full_bottom.r);
    assert(full_top.r > full_top.g);

    const auto half_leading = lv_vu_row_color(0.5f, 2, 5);
    const auto half_unlit = lv_vu_row_color(0.5f, 3, 5);
    assert(rgb_brightness(half_leading) > rgb_brightness(half_unlit));
    assert(rgb_brightness(half_leading) < rgb_brightness(lv_vu_row_color(1.0f, 2, 5)));
}

static void test_keyboard_vu_is_sensitive_but_still_noise_gated() {
    assert(lv_keyboard_vu_level(0.03f) == 0.0f);
    assert(lv_keyboard_vu_level(0.04f) > lv_meter_level(0.04f));
    assert(std::fabs(lv_keyboard_vu_level(0.04f) -
                     lv_perceptual_level(0.04f)) < 0.0001f);
}

static void test_vu_smoothing_attacks_faster_than_it_releases() {
    const float attack = lv_vu_smooth_level(0.0f, 1.0f);
    const float release = lv_vu_smooth_level(1.0f, 0.0f);
    assert(attack > 0.0f && attack < 1.0f);
    assert(release > 0.0f && release < 1.0f);
    assert(attack > 1.0f - release);

    float level = 0.0f;
    for (int frame = 0; frame < 20; ++frame) {
        level = lv_vu_smooth_level(level, 0.8f);
    }
    assert(level > 0.79f && level <= 0.8f);
}

static void test_aurora_varies_across_layout_and_time() {
    const auto left = lv_aurora_color(0.0f, 0.0f, 0);
    const auto right = lv_aurora_color(11.0f, 0.0f, 0);
    const auto later = lv_aurora_color(0.0f, 0.0f, 2000);
    assert(rgb_brightness(left) >= 24);
    assert(rgb_brightness(left) <= 150);
    assert(std::memcmp(&left, &right, sizeof(left)) != 0);
    assert(std::memcmp(&left, &later, sizeof(left)) != 0);
}

static void test_peak_hold_waits_then_decays_to_live_level() {
    lv_peak_hold_t peak{};

    assert(lv_peak_hold_update(&peak, 60, 1000) == 60);
    assert(lv_peak_hold_update(&peak, 10, 1700) == 60);
    assert(lv_peak_hold_update(&peak, 10, 1850) == 58);
    assert(lv_peak_hold_update(&peak, 10, 4750) == 10);

    assert(lv_peak_hold_update(&peak, 70, 4800) == 70);
    assert(lv_peak_hold_update(&peak, 20, 5500) == 70);
}

static void test_usb_host_status_labels_show_furthest_stage() {
    assert(std::strcmp(lv_usb_host_status_label(0), "BOOT") == 0);
    assert(std::strcmp(lv_usb_host_status_label(LV_USB_HOST_STARTED), "WAIT") == 0);
    assert(std::strcmp(lv_usb_host_status_label(
                           LV_USB_HOST_STARTED | LV_USB_ATTACH_EVENT),
                       "ATT") == 0);
    assert(std::strcmp(lv_usb_host_status_label(
                           LV_USB_HOST_STARTED | LV_USB_DEVICE_ATTACHED),
                       "LS") == 0);
    assert(std::strcmp(lv_usb_host_status_label(
                           LV_USB_HOST_STARTED | LV_USB_DEVICE_ATTACHED |
                           LV_USB_DEVICE_FULL_SPEED),
                       "FS") == 0);
    assert(std::strcmp(lv_usb_host_status_label(
                           LV_USB_HOST_STARTED | LV_USB_DEVICE_MOUNTED),
                       "DEV") == 0);
    assert(std::strcmp(lv_usb_host_status_label(
                           LV_USB_HOST_STARTED | LV_USB_DEVICE_MOUNTED |
                           LV_USB_KEYBOARD_MOUNTED),
                       "KBD") == 0);
    assert(std::strcmp(lv_usb_host_status_label(
                           LV_USB_HOST_STARTED | LV_USB_DEVICE_MOUNTED |
                           LV_USB_RAW_MOUNTED),
                       "RAW") == 0);
    assert(std::strcmp(lv_usb_host_status_label(
                           LV_USB_HOST_STARTED | LV_USB_DEVICE_MOUNTED |
                           LV_USB_KEYBOARD_MOUNTED | LV_USB_RAW_MOUNTED),
                       "OK") == 0);
    assert(std::strcmp(lv_usb_host_status_label(LV_USB_HOST_ERROR), "ERR") == 0);
}

static void test_usb_status_display_holds_every_reached_milestone() {
    lv_usb_status_display_t display{};
    const uint8_t complete = LV_USB_HOST_STARTED | LV_USB_ATTACH_EVENT |
                             LV_USB_DEVICE_ATTACHED | LV_USB_DEVICE_FULL_SPEED |
                             LV_USB_DEVICE_MOUNTED | LV_USB_KEYBOARD_MOUNTED |
                             LV_USB_RAW_MOUNTED;

    assert(std::strcmp(lv_usb_status_display_update(&display, complete, 1300),
                       "WAIT") == 0);
    assert(std::strcmp(lv_usb_status_display_update(&display, complete, 2299),
                       "WAIT") == 0);
    assert(std::strcmp(lv_usb_status_display_update(&display, complete, 2300),
                       "ATT") == 0);
    assert(std::strcmp(lv_usb_status_display_update(&display, complete, 3300),
                       "FS") == 0);
    assert(std::strcmp(lv_usb_status_display_update(&display, complete, 4300),
                       "DEV") == 0);
    assert(std::strcmp(lv_usb_status_display_update(&display, complete, 5300),
                       "KBD") == 0);
    assert(std::strcmp(lv_usb_status_display_update(&display, complete, 6300),
                       "OK") == 0);
}

static void test_usb_status_display_keeps_last_failed_milestone() {
    lv_usb_status_display_t display{};
    const uint8_t attached_then_dropped = LV_USB_HOST_STARTED | LV_USB_ATTACH_EVENT;

    assert(std::strcmp(lv_usb_status_display_update(
                           &display, LV_USB_HOST_STARTED, 100),
                       "WAIT") == 0);
    assert(std::strcmp(lv_usb_status_display_update(
                           &display, attached_then_dropped, 1099),
                       "WAIT") == 0);
    assert(std::strcmp(lv_usb_status_display_update(
                           &display, attached_then_dropped, 1100),
                       "ATT") == 0);
    assert(std::strcmp(lv_usb_status_display_update(
                           &display, attached_then_dropped, 5000),
                       "ATT") == 0);
}

static void test_usb_status_display_resets_after_successful_disconnect() {
    lv_usb_status_display_t display{};
    const uint8_t complete = LV_USB_HOST_STARTED | LV_USB_ATTACH_EVENT |
                             LV_USB_DEVICE_ATTACHED | LV_USB_DEVICE_FULL_SPEED |
                             LV_USB_DEVICE_MOUNTED | LV_USB_KEYBOARD_MOUNTED |
                             LV_USB_RAW_MOUNTED;

    assert(std::strcmp(lv_usb_status_display_update(&display, complete, 0),
                       "WAIT") == 0);
    assert(std::strcmp(lv_usb_status_display_update(&display, complete, 1000),
                       "ATT") == 0);
    assert(std::strcmp(lv_usb_status_display_update(&display, complete, 2000),
                       "FS") == 0);
    assert(std::strcmp(lv_usb_status_display_update(&display, complete, 3000),
                       "DEV") == 0);
    assert(std::strcmp(lv_usb_status_display_update(&display, complete, 4000),
                       "KBD") == 0);
    assert(std::strcmp(lv_usb_status_display_update(&display, complete, 5000),
                       "OK") == 0);

    assert(std::strcmp(lv_usb_status_display_update(
                           &display, LV_USB_HOST_STARTED, 5100),
                       "WAIT") == 0);
}

int main() {
    test_complete_frame_commits_atomically();
    test_out_of_order_chunks_and_bad_checksum();
    test_newer_frame_discards_partial_frame();
    test_release_and_mapping();
    test_pending_audio_buffer_is_not_overwritten();
    test_audio_decimation_only_publishes_due_windows();
    test_mono_mix_preserves_centered_audio_and_stereo_levels();
    test_mono_mix_accepts_opposite_phase_cancellation();
    test_kick_detector_tracks_an_isolated_kick_train_end_to_end();
    test_stereo_analysis_keeps_channels_independent();
    test_mel_filterbank_tracks_perceptual_frequency_regions();
    test_silent_channel_stays_silent();
    test_biased_silence_removes_measured_dc_offset();
    test_input_gain_scales_audio_before_analysis();
    test_fft_preserves_tone_amplitude();
    test_boosted_quiet_kick_creates_visible_envelope();
    test_perceptual_level_uses_decibel_scale_and_noise_floor();
    test_level_dbfs_rounds_and_has_a_diagnostic_floor();
    test_audio_normalizer_adapts_without_promoting_noise_or_beats();
    test_audio_normalizer_rejects_an_isolated_noise_spike();
    test_audio_calibration_accepts_and_removes_stable_noise();
    test_calibrated_spectral_signal_rejects_noise_and_one_band_spikes();
    test_spectral_signal_keeps_quiet_music_gate_open_with_hold();
    test_audio_calibration_rejects_variable_input();
    test_audio_normalizer_preserves_an_explicit_floor();
    test_meter_level_gates_noise_without_reducing_normal_audio();
    test_lcd_spectrum_bins_preserve_low_and_high_frequency_edges();
    test_audio_activity_ignores_isolated_noise_spikes();
    test_audio_activity_requires_sustained_sound_to_leave_idle();
    test_flow_v2_closed_gate_cannot_amplify_noise();
    test_flow_v2_gain_is_bounded_and_recovers_slowly();
    test_flow_v2_uses_one_gain_without_collapsing_stereo();
    test_flow_v2_packet_value_follows_short_term_energy();
    test_flow_v2_centroid_palette_is_ordered_and_vivid();
    test_flow_v2_smooths_centroid_in_log_frequency_space();
    test_flow_v2_shared_low_onset_detects_close_distinct_hits();
    test_flow_v2_onsets_are_band_specific_and_do_not_retrigger_sustain();
    test_flow_v2_routes_fft_onset_fixtures_to_low_mid_and_high();
    test_flow_v2_bass_can_follow_mid_event_inside_shared_refractory();
    test_flow_v2_mid_event_cannot_displace_recent_bass();
    test_flow_v2_event_wavefront_is_narrow_and_music_colored();
    test_flow_v2_close_events_keep_two_separate_crests();
    test_flow_v3_bass_extraction_is_low_and_phase_safe();
    test_flow_v3_sustain_has_continuous_body_without_attack_flash();
    test_flow_v3_close_bass_rises_remain_distinguishable();
    test_flow_v3_bass_lab_has_one_stable_color();
    test_flow_v3_bass_contrast_preserves_body_and_separates_hits();
    test_flow_v3_bass_contrast_suppresses_micro_twinkle();
    test_flow_v3_bass_lab_b_tracks_39_kicks_without_flashes();
    test_flow_v3_mid_palette_is_vivid_and_equal_lightness();
    test_flow_v3_mid_lab_ignores_bass_and_highs();
    test_flow_v3_mid_hue_tracks_frequency_without_brightness_motion();
    test_flow_v3_mid_spectral_contrast_accents_new_frequency();
    test_flow_v3_mid_sustained_change_returns_to_base_hue();
    test_flow_v3_combined_sample_preserves_bass_body();
    test_flow_v3_combined_hue_preserves_bass_lightness();
    test_flow_v3_energy_uses_fixed_phase_safe_full_spectrum_level();
    test_flow_v3_energy_has_smooth_attack_and_release();
    test_flow_v3_energy_transport_interpolates_continuously();
    test_flow_v3_energy_floor_preserves_bass_body();
    test_flow_v3_energy_floor_only_lifts_quiet_bass();
    test_flow_v3_attack_has_no_initial_or_falling_edge_flash();
    test_flow_v3_attack_tracks_continuous_positive_rises();
    test_flow_v3_attack_repeats_without_refractory_timer();
    test_flow_v3_attack_stereo_merge_is_phase_safe();
    test_flow_v3_attack_transport_interpolates_continuously();
    test_flow_v3_attack_integration_is_inert_at_zero();
    test_flow_v3_attack_integration_only_tightens_existing_body();
    test_flow_v3_texture_tracks_high_frequency_share();
    test_flow_v3_texture_is_sustained_and_smooth();
    test_flow_v3_texture_stereo_merge_is_phase_safe();
    test_flow_v3_texture_transport_interpolates_continuously();
    test_flow_v3_texture_changes_slice_hue_not_lightness();
    test_flow_v3_texture_renders_one_value_per_column();
    test_flow_v3_texture_integration_is_bounded_and_equal_lightness();
    test_flow_v3_engine_uses_the_accepted_compositor();
    test_flow_v3_transport_interpolates_between_slices();
    test_spectral_flow_maps_frequency_regions_to_vivid_hues();
    test_spectral_flow_adjacent_bands_in_one_lane_do_not_blink();
    test_spectral_flow_limits_hue_motion_per_frame();
    test_kick_detector_fires_on_a_sharp_bass_rise_once();
    test_kick_detector_ignores_gradual_or_sustained_bass();
    test_spectral_flow_holds_attack_accent_for_a_visible_segment();
    test_spectral_flow_latches_kick_brightness_for_the_whole_packet();
    test_visualizer_shares_attack_timing_across_stereo_halves();
    test_spectral_flow_keeps_brightness_stable_while_audio_is_present();
    test_spectral_flow_balanced_spectrum_remains_saturated();
    test_spectral_flow_ribbon_propagates_every_intermediate_frame();
    test_spectral_flow_sustained_music_keeps_defining_the_source();
    test_visualizer_flow_travel_can_be_tuned_without_rebuilding();
    test_spectral_flow_coordinates_stagger_every_row();
    test_spectral_flow_blurs_a_sharp_boundary_across_keys();
    test_spectral_flow_has_no_motion_unrelated_to_audio_history();
    test_shared_visualizer_uses_idle_when_audio_is_quiet();
    test_vu_rows_fill_smoothly_from_green_to_red();
    test_keyboard_vu_is_sensitive_but_still_noise_gated();
    test_vu_smoothing_attacks_faster_than_it_releases();
    test_aurora_varies_across_layout_and_time();
    test_peak_hold_waits_then_decays_to_live_level();
    test_usb_host_status_labels_show_furthest_stage();
    test_usb_status_display_holds_every_reached_milestone();
    test_usb_status_display_keeps_last_failed_milestone();
    test_usb_status_display_resets_after_successful_disconnect();
}
