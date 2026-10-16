#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "landerviz/audio_analysis.h"
#include "landerviz/audio_buffer.h"
#include "landerviz/effects.h"
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
    lv_audio_buffer_state_t buffers{0, -1};
    assert(lv_audio_buffer_complete(&buffers));
    assert(buffers.ready == 0);
    assert(buffers.fill == 1);

    assert(!lv_audio_buffer_complete(&buffers));
    assert(buffers.ready == 0);
    assert(buffers.fill == 1);

    assert(lv_audio_buffer_take(&buffers) == 0);
    assert(lv_audio_buffer_complete(&buffers));
    assert(buffers.ready == 1);
    assert(buffers.fill == 0);
}

static std::array<uint16_t, LV_FFT_N> audio_tone(int bin, float amplitude) {
    std::array<uint16_t, LV_FFT_N> samples{};
    for (int i = 0; i < LV_FFT_N; ++i) {
        const float phase = 2.0f * 3.14159265f * bin * i / LV_FFT_N;
        samples[i] = (uint16_t)(LV_ADC_MID + LV_ADC_MID * amplitude * std::sin(phase));
    }
    return samples;
}

static int dominant_band(const lv_channel_analysis_t &analysis) {
    int dominant = 0;
    for (int band = 1; band < LV_BAND_COUNT; ++band) {
        if (analysis.band_level[band] > analysis.band_level[dominant]) dominant = band;
    }
    return dominant;
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
}

static void test_biased_silence_removes_measured_dc_offset() {
    lv_channel_analysis_t analysis{};
    std::array<uint16_t, LV_FFT_N> biased_silence{};
    biased_silence.fill(2023);

    lv_analyze_channel(&analysis, biased_silence.data(), 8.0f);

    assert(analysis.rms_level == 0.0f);
    assert(analysis.bass_env == 0.0f);
    for (float level : analysis.band_level) assert(level == 0.0f);
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

static void test_spectral_flow_flux_maps_all_frequency_regions_to_vivid_hues() {
    float low_bands[LV_SPECTRAL_FLOW_BANDS] = {};
    low_bands[0] = 1.0f;
    lv_spectral_flow_t low_flow{};
    lv_spectral_flow_update(&low_flow, low_bands, 0.0f, 0);
    const auto low = lv_spectral_flow_color(&low_flow, 0.0f, 0);
    assert(low.r >= 240 && low.g < 40 && low.b < 40);

    float mid_bands[LV_SPECTRAL_FLOW_BANDS] = {};
    mid_bands[7] = 1.0f;
    lv_spectral_flow_t mid_flow{};
    lv_spectral_flow_update(&mid_flow, mid_bands, 0.0f, 0);
    const auto mid = lv_spectral_flow_color(&mid_flow, 0.0f, 0);
    assert(mid.r < 80 && mid.g < 80 && mid.b >= 240);

    float high_bands[LV_SPECTRAL_FLOW_BANDS] = {};
    high_bands[11] = 1.0f;
    lv_spectral_flow_t high_flow{};
    lv_spectral_flow_update(&high_flow, high_bands, 0.0f, 0);
    const auto high = lv_spectral_flow_color(&high_flow, 0.0f, 0);
    assert(high.r >= 180 && high.b >= 240 && high.g < 40);
}

static void test_spectral_flow_keeps_quiet_ribbon_visible_after_a_peak() {
    float bands[LV_SPECTRAL_FLOW_BANDS] = {};
    bands[0] = 1.0f;
    lv_spectral_flow_t flow{};
    lv_spectral_flow_update(&flow, bands, 0.0f, 0);
    bands[0] = 0.1f;
    lv_spectral_flow_update(&flow, bands, 0.0f,
                            LV_SPECTRAL_FLOW_FRAME_MS);
    const auto quiet = lv_spectral_flow_color(
        &flow, 0.0f, LV_SPECTRAL_FLOW_FRAME_MS);
    assert(quiet.r >= 130);
    assert(quiet.g == 0 && quiet.b == 0);
}

static void test_spectral_flow_ribbon_propagates_every_intermediate_frame() {
    float bands[LV_SPECTRAL_FLOW_BANDS] = {};
    bands[0] = 1.0f;
    lv_spectral_flow_t flow{};
    lv_spectral_flow_update(&flow, bands, 0.0f, 0);

    bands[0] = 0.0f;
    bands[11] = 1.0f;
    lv_rgb_t inner_at_source_time{};
    lv_rgb_t previous_delayed{};
    int changed_frames = 0;
    for (uint32_t now_ms = 10;
         now_ms <= 2 * LV_SPECTRAL_FLOW_TRAVEL_MS; now_ms += 10) {
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
    assert(changed_frames >= 9);

    for (int distance = 0; distance < LV_SPECTRAL_FLOW_COLUMNS; ++distance) {
        assert(rgb_brightness(lv_spectral_flow_color(
                   &flow, (float)distance, delayed_time)) > 0);
    }
}

static void test_spectral_flow_kick_does_not_override_high_frequency_color() {
    float bands[LV_SPECTRAL_FLOW_BANDS];
    std::fill(std::begin(bands), std::end(bands), 0.2f);
    lv_spectral_flow_t flow{};
    lv_spectral_flow_update(&flow, bands, 0.0f, 0);

    for (int band = 8; band < LV_SPECTRAL_FLOW_BANDS; ++band) {
        bands[band] = 0.4f;
    }
    lv_spectral_flow_update(&flow, bands, 1.0f,
                            LV_SPECTRAL_FLOW_FRAME_MS);

    const float wrapped_hue = std::fmod(flow.target_hue + 256.0f, 256.0f);
    assert(wrapped_hue >= 120.0f && wrapped_hue <= 210.0f);
}

static void test_spectral_flow_body_event_is_not_diluted_by_more_high_bands() {
    float bands[LV_SPECTRAL_FLOW_BANDS];
    std::fill(std::begin(bands), std::end(bands), 0.2f);
    lv_spectral_flow_t flow{};
    lv_spectral_flow_update(&flow, bands, 0.0f, 0);

    bands[3] = 0.4f;
    for (int band = 8; band < LV_SPECTRAL_FLOW_BANDS; ++band) {
        bands[band] = 0.3f;
    }
    lv_spectral_flow_update(&flow, bands, 0.0f,
                            LV_SPECTRAL_FLOW_FRAME_MS);

    const float wrapped_hue = std::fmod(flow.target_hue + 256.0f, 256.0f);
    assert(wrapped_hue >= 30.0f && wrapped_hue <= 55.0f);
}

static void test_spectral_flow_kick_priority_expires_between_hits() {
    float bands[LV_SPECTRAL_FLOW_BANDS];
    std::fill(std::begin(bands), std::end(bands), 0.2f);
    lv_spectral_flow_t flow{};
    lv_spectral_flow_update(&flow, bands, 0.0f, 0);

    for (int band = 8; band < LV_SPECTRAL_FLOW_BANDS; ++band) {
        bands[band] = 0.4f;
    }
    lv_spectral_flow_update(&flow, bands, 1.0f, 33);

    for (uint32_t now_ms = 66; now_ms < 330; now_ms += 33) {
        const float falling_envelope =
            1.0f - 0.5f * ((float)now_ms - 33.0f) / 297.0f;
        lv_spectral_flow_update(&flow, bands, falling_envelope, now_ms);
    }
    for (int band = 8; band < LV_SPECTRAL_FLOW_BANDS; ++band) {
        bands[band] = 0.8f;
    }
    lv_spectral_flow_update(&flow, bands, 0.5f, 330);

    const float wrapped_hue = std::fmod(flow.target_hue + 256.0f, 256.0f);
    assert(wrapped_hue >= 120.0f && wrapped_hue <= 210.0f);
}

static void test_spectral_flow_dominant_body_band_chooses_hue() {
    float bands[LV_SPECTRAL_FLOW_BANDS];
    std::fill(std::begin(bands), std::end(bands), 0.2f);
    lv_spectral_flow_t flow{};
    lv_spectral_flow_update(&flow, bands, 0.0f, 0);

    bands[3] = 0.35f;
    bands[7] = 0.6f;
    lv_spectral_flow_update(&flow, bands, 0.0f,
                            LV_SPECTRAL_FLOW_FRAME_MS);

    const float wrapped_hue = std::fmod(flow.target_hue + 256.0f, 256.0f);
    assert(wrapped_hue >= 165.0f && wrapped_hue <= 190.0f);
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
    flow.history[0] = {0.0f, 255.0f, 0};
    flow.history[1] = {
        0.0f, 255.0f, 1000 - LV_SPECTRAL_FLOW_TRAVEL_MS - 1};
    flow.history[2] = {
        170.0f, 255.0f, 1000 - LV_SPECTRAL_FLOW_TRAVEL_MS};
    flow.history[3] = {170.0f, 255.0f, 1000};

    const auto blue = lv_spectral_flow_color(&flow, 0.0f, 1000);
    const auto blue_edge = lv_spectral_flow_color(&flow, 0.6f, 1000);
    const auto red_edge = lv_spectral_flow_color(&flow, 1.4f, 1000);
    const auto red = lv_spectral_flow_color(&flow, 2.2f, 1000);

    assert(blue.b > blue.r && blue.b > blue.g);
    assert(red.r > red.g && red.r > red.b);
    assert(blue_edge.g >= 30);
    assert(red_edge.g >= 30);
}

static void test_spectral_flow_brightness_carrier_shows_same_hue_motion() {
    lv_spectral_flow_t flow{};
    flow.initialized = true;
    flow.history_count = 1;
    flow.history[0] = {0.0f, 255.0f, 0};

    const uint32_t dim_time = 3 * LV_SPECTRAL_FLOW_TRAVEL_MS / 4;
    const uint32_t bright_time = 2 * LV_SPECTRAL_FLOW_TRAVEL_MS;
    const int dim = rgb_brightness(
        lv_spectral_flow_color(&flow, 2.0f, dim_time));
    const int bright = rgb_brightness(
        lv_spectral_flow_color(&flow, 2.0f, bright_time));
    assert(bright - dim >= 30);
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

static void test_spectral_flow_keeps_saturated_color_during_hue_changes() {
    float bands[LV_SPECTRAL_FLOW_BANDS] = {};
    bands[0] = 1.0f;
    lv_spectral_flow_t flow{};
    lv_spectral_flow_update(&flow, bands, 0.0f, 0);

    bands[0] = 0.0f;
    bands[11] = 1.0f;
    for (int frame = 1; frame <= 12; ++frame) {
        const uint32_t now_ms = frame * LV_SPECTRAL_FLOW_FRAME_MS;
        lv_spectral_flow_update(&flow, bands, 0.0f, now_ms);
        const auto color =
            lv_spectral_flow_color(&flow, 0.0f, now_ms);
        assert(std::min({color.r, color.g, color.b}) == 0);
    }
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
    test_stereo_analysis_keeps_channels_independent();
    test_silent_channel_stays_silent();
    test_biased_silence_removes_measured_dc_offset();
    test_input_gain_scales_audio_before_analysis();
    test_boosted_quiet_kick_creates_visible_envelope();
    test_perceptual_level_uses_decibel_scale_and_noise_floor();
    test_meter_level_gates_noise_without_reducing_normal_audio();
    test_lcd_spectrum_bins_preserve_low_and_high_frequency_edges();
    test_audio_activity_ignores_isolated_noise_spikes();
    test_audio_activity_requires_sustained_sound_to_leave_idle();
    test_spectral_flow_flux_maps_all_frequency_regions_to_vivid_hues();
    test_spectral_flow_keeps_quiet_ribbon_visible_after_a_peak();
    test_spectral_flow_ribbon_propagates_every_intermediate_frame();
    test_spectral_flow_kick_does_not_override_high_frequency_color();
    test_spectral_flow_body_event_is_not_diluted_by_more_high_bands();
    test_spectral_flow_kick_priority_expires_between_hits();
    test_spectral_flow_dominant_body_band_chooses_hue();
    test_visualizer_flow_travel_can_be_tuned_without_rebuilding();
    test_spectral_flow_coordinates_stagger_every_row();
    test_spectral_flow_blurs_a_sharp_boundary_across_keys();
    test_spectral_flow_brightness_carrier_shows_same_hue_motion();
    test_spectral_flow_keeps_saturated_color_during_hue_changes();
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
