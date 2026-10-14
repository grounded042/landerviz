#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "landerviz/audio_analysis.h"
#include "landerviz/audio_buffer.h"
#include "landerviz/moonlander_map.h"
#include "landerviz/protocol.h"
#include "landerviz/usb_bridge.h"

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
    assert(analysis.bass_env > kick * 0.95f);
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
    test_peak_hold_waits_then_decays_to_live_level();
    test_usb_host_status_labels_show_furthest_stage();
    test_usb_status_display_holds_every_reached_milestone();
    test_usb_status_display_keeps_last_failed_milestone();
    test_usb_status_display_resets_after_successful_disconnect();
}
