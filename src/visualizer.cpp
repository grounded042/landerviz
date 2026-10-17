#include "landerviz/visualizer.h"

#include <algorithm>
#include <cstring>

#include "landerviz/moonlander_map.h"

namespace {

void set_key(lv_rgb_t frame[LV_KEY_COUNT], int key, lv_rgb_t color) {
    if (key >= 0 && key < LV_KEY_COUNT) frame[key] = color;
}

void render_flow_ribbons(
    const lv_spectral_flow_t flow[LV_AUDIO_CHANNEL_COUNT], uint32_t now_ms,
    lv_rgb_t frame[LV_KEY_COUNT]) {
    for (int half = 0; half < LV_AUDIO_CHANNEL_COUNT; ++half) {
        for (int col = 0; col < LV_SPECTRAL_FLOW_COLUMNS; ++col) {
            const int distance = half == 0
                                     ? LV_SPECTRAL_FLOW_COLUMNS - 1 - col
                                     : col;
            for (int row = 0; row < 5; ++row) {
                const float coordinate = lv_spectral_flow_coordinate(
                    half, (float)distance, (float)row);
                set_key(frame, lv_key_at(half, col, row),
                        lv_spectral_flow_color(&flow[half], coordinate,
                                               now_ms));
            }
        }

        for (int key = 0; key < 3; ++key) {
            const float coordinate = lv_spectral_flow_coordinate(
                half, 0.0f, (float)(key + 1));
            set_key(frame, lv_utility_key_at(half, key),
                    lv_spectral_flow_color(&flow[half], coordinate, now_ms));
        }
        constexpr float thumb_distance[4] = {1.25f, 0.85f, 0.45f, 0.0f};
        for (int key = 0; key < 4; ++key) {
            const float coordinate = lv_spectral_flow_coordinate(
                half, thumb_distance[key], 3.2f + key * 0.2f);
            set_key(frame, lv_thumb_key_at(half, key),
                    lv_spectral_flow_color(&flow[half], coordinate, now_ms));
        }
    }
}

void render_flow(lv_visualizer_t *visualizer,
                 const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT],
                 uint32_t now_ms, lv_rgb_t frame[LV_KEY_COUNT]) {
    static_assert((int)LV_SPECTRAL_FLOW_BANDS == (int)LV_MEL_BAND_COUNT);

    float flow_bands[LV_AUDIO_CHANNEL_COUNT][LV_SPECTRAL_FLOW_BANDS] = {};
    float bass_transient[LV_AUDIO_CHANNEL_COUNT] = {};
    float shared_bass = 0.0f;
    for (int half = 0; half < LV_AUDIO_CHANNEL_COUNT; ++half) {
        const bool gate_open =
            lv_meter_level(audio[half].rms_level * 1.41421356f) > 0.0f;
        if (gate_open) {
            for (int band = 0; band < LV_SPECTRAL_FLOW_BANDS; ++band) {
                flow_bands[half][band] =
                    lv_perceptual_level(audio[half].mel_level[band]);
            }
            bass_transient[half] =
                lv_perceptual_level(audio[half].bass_env);
            // Kick timing is shared so a panned bass hit launches one
            // coherent packet across both keyboard halves. Rendering remains
            // stereo because each half keeps its own mel spectrum.
            shared_bass = std::max(shared_bass, audio[half].bass_env);
        }
    }
    const lv_kick_event_t kick = lv_kick_detector_update(
        &visualizer->flow_kick, shared_bass, now_ms);

    for (int half = 0; half < LV_AUDIO_CHANNEL_COUNT; ++half) {
        const lv_kick_event_t *visible_kick =
            visualizer->flow_kick_packets_enabled ? &kick : nullptr;
        lv_spectral_flow_update(&visualizer->flow[half], flow_bands[half],
                                bass_transient[half], now_ms, visible_kick);
    }
    render_flow_ribbons(visualizer->flow, now_ms, frame);
}

void render_spectrum(lv_visualizer_t *visualizer,
                     const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT],
                     lv_rgb_t frame[LV_KEY_COUNT]) {
    constexpr int columns = 6;
    constexpr int rows = 5;
    lv_rgb_t rendered[2][rows][columns]{};

    for (int half = 0; half < LV_AUDIO_CHANNEL_COUNT; ++half) {
        for (int col = 0; col < columns; ++col) {
            const int visual_band = half == 0 ? col : columns - 1 - col;
            const int band = visual_band * 2;
            const float level = lv_perceptual_level(std::max(
                audio[half].band_level[band],
                audio[half].band_level[band + 1]));
            const int active_rows =
                lv_key_at(half, col, rows - 1) >= 0 ? rows : rows - 1;
            const int lit = (int)(level * active_rows + 0.5f);
            const uint8_t hue = (uint8_t)(visual_band * 42);
            for (int row_from_bottom = 0; row_from_bottom < active_rows;
                 ++row_from_bottom) {
                const lv_rgb_t color =
                    row_from_bottom < lit
                        ? lv_hsv_to_rgb(hue, 255, 255)
                        : lv_hsv_to_rgb(hue, 255, 20);
                const int row = active_rows - 1 - row_from_bottom;
                rendered[half][row][col] = color;
                set_key(frame, lv_key_at(half, col, row), color);
            }

            float &peak = visualizer->spectrum_peaks[half][visual_band];
            peak = level > peak ? level : peak * 0.970225f;
            const int peak_row = (int)(peak * active_rows);
            if (peak_row > 0 && peak_row <= active_rows) {
                const int row = active_rows - peak_row;
                rendered[half][row][col] = {255, 255, 255};
                set_key(frame, lv_key_at(half, col, row), {255, 255, 255});
            }
        }

        const int inner_col = half == 0 ? columns - 1 : 0;
        for (int key = 0; key < 3; ++key) {
            set_key(frame, lv_utility_key_at(half, key),
                    rendered[half][key][inner_col]);
        }

        const float bass = lv_perceptual_level(std::max(
            audio[half].band_level[0], audio[half].band_level[1]));
        const uint8_t value = (uint8_t)(20.0f + bass * 235.0f);
        const lv_rgb_t color = lv_hsv_to_rgb(0, 255, value);
        for (int key = 0; key < 4; ++key) {
            set_key(frame, lv_thumb_key_at(half, key), color);
        }
    }
}

void render_pulse(lv_visualizer_t *visualizer,
                  const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT],
                  uint32_t now_ms, lv_rgb_t frame[LV_KEY_COUNT]) {
    const float bass = std::max(audio[0].bass_env, audio[1].bass_env);
    const uint8_t value =
        (uint8_t)(24.0f + lv_perceptual_level(bass) * 231.0f);
    if (now_ms - visualizer->last_pulse_hue_ms >= 120) {
        visualizer->pulse_hue += (uint8_t)(
            (now_ms - visualizer->last_pulse_hue_ms) / 120);
        visualizer->last_pulse_hue_ms = now_ms;
    }
    const lv_rgb_t color =
        lv_hsv_to_rgb(visualizer->pulse_hue, 220, value);
    for (int key = 0; key < LV_KEY_COUNT; ++key) frame[key] = color;
}

void render_vu(lv_visualizer_t *visualizer,
               const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT],
               lv_rgb_t frame[LV_KEY_COUNT]) {
    for (int half = 0; half < LV_AUDIO_CHANNEL_COUNT; ++half) {
        const float target =
            lv_keyboard_vu_level(audio[half].rms_level * 1.41421356f);
        float &displayed = visualizer->vu_levels[half];
        displayed = lv_vu_smooth_level(displayed, target);

        for (int col = 0; col < 6; ++col) {
            const int row_count = lv_key_at(half, col, 4) >= 0 ? 5 : 4;
            for (int row_from_bottom = 0; row_from_bottom < row_count;
                 ++row_from_bottom) {
                const int row = row_count - 1 - row_from_bottom;
                set_key(frame, lv_key_at(half, col, row),
                        lv_vu_row_color(displayed, row_from_bottom,
                                        row_count));
            }
        }
        for (int key = 0; key < 3; ++key) {
            set_key(frame, lv_utility_key_at(half, key),
                    lv_vu_row_color(displayed, 2 - key, 3));
        }
        const lv_rgb_t thumb = lv_vu_row_color(displayed, 0, 5);
        for (int key = 0; key < 4; ++key) {
            set_key(frame, lv_thumb_key_at(half, key), thumb);
        }
    }
}

void render_idle(uint32_t now_ms, lv_rgb_t frame[LV_KEY_COUNT]) {
    for (int half = 0; half < 2; ++half) {
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < 6; ++col) {
                const float x = half == 0 ? (float)col : (float)(7 + col);
                set_key(frame, lv_key_at(half, col, row),
                        lv_aurora_color(x, (float)row, now_ms));
            }
        }

        const float inner_x = half == 0 ? 5.8f : 6.2f;
        for (int key = 0; key < 3; ++key) {
            set_key(frame, lv_utility_key_at(half, key),
                    lv_aurora_color(inner_x, 0.8f + key, now_ms));
        }
        for (int key = 0; key < 4; ++key) {
            const float x = half == 0 ? 3.8f + key * 0.55f
                                      : 8.2f - key * 0.55f;
            set_key(frame, lv_thumb_key_at(half, key),
                    lv_aurora_color(x, 5.0f + (key & 1) * 0.35f, now_ms));
        }
    }
}

}  // namespace

void lv_visualizer_render_flow_ribbons(
    const lv_spectral_flow_t flow[LV_AUDIO_CHANNEL_COUNT], uint32_t now_ms,
    lv_rgb_t frame[LV_KEY_COUNT]) {
    std::memset(frame, 0, sizeof(lv_rgb_t) * LV_KEY_COUNT);
    render_flow_ribbons(flow, now_ms, frame);
}

const char *lv_viz_mode_name(lv_viz_mode_t mode) {
    static constexpr const char *names[] = {
        "FLOW", "SPECTRUM", "PULSE", "VU", "IDLE"};
    const int index = (int)mode;
    return index >= 0 && index < (int)lv_viz_mode_t::COUNT
               ? names[index]
               : "UNKNOWN";
}

void lv_visualizer_set_flow_travel_ms(lv_visualizer_t *visualizer,
                                      uint16_t travel_ms) {
    if (travel_ms > 0) {
        travel_ms = std::clamp<uint16_t>(travel_ms, 50, 1000);
    }
    for (auto &flow : visualizer->flow) flow.travel_ms = travel_ms;
}

uint16_t lv_visualizer_flow_travel_ms(const lv_visualizer_t *visualizer) {
    return visualizer->flow[0].travel_ms > 0
               ? visualizer->flow[0].travel_ms
               : (uint16_t)LV_SPECTRAL_FLOW_TRAVEL_MS;
}

void lv_visualizer_set_flow_kick_packets(lv_visualizer_t *visualizer,
                                         bool enabled) {
    visualizer->flow_kick_packets_enabled = enabled;
}

void lv_visualizer_render(
    lv_visualizer_t *visualizer, lv_viz_mode_t mode,
    const lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT],
    bool audio_present, uint32_t now_ms, lv_rgb_t frame[LV_KEY_COUNT]) {
    std::memset(frame, 0, sizeof(lv_rgb_t) * LV_KEY_COUNT);
    if (!audio_present && mode != lv_viz_mode_t::IDLE) {
        mode = lv_viz_mode_t::IDLE;
    }

    switch (mode) {
        case lv_viz_mode_t::FLOW:
            render_flow(visualizer, audio, now_ms, frame);
            break;
        case lv_viz_mode_t::SPECTRUM:
            render_spectrum(visualizer, audio, frame);
            break;
        case lv_viz_mode_t::PULSE:
            render_pulse(visualizer, audio, now_ms, frame);
            break;
        case lv_viz_mode_t::VU:
            render_vu(visualizer, audio, frame);
            break;
        case lv_viz_mode_t::IDLE:
            render_idle(now_ms, frame);
            break;
        default:
            break;
    }

}
