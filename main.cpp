// LanderViz — Moonlander audio visualizer firmware (v1)
// Target: SparkFun Pro Micro RP2040, Raspberry Pi Pico SDK, C++17
//
// Implements the audio visualization modes:
//   - FLOW:     mono spectral ribbons moving out from the center split
//   - SPECTRUM: column analyzer, mirrored across the split
//   - PULSE:    kick-driven whole-board breath, slow hue rotation
//   - VU:       smoothed stereo bottom-to-top meters
//   - IDLE:     slow aurora, also shown when the mixer is silent
// Plus: QC1602A LCD (spectrum or stereo VU, mode/fps status),
//       tactile mode button (short = next, long = previous).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/sync.h"
#include "hardware/timer.h"

#include "landerviz/audio_analysis.h"
#include "landerviz/audio_buffer.h"
#include "landerviz/effects.h"
#include "landerviz/flow_v3_engine.h"
#include "landerviz/moonlander_map.h"
#include "landerviz/usb_bridge.h"
#include "landerviz/visualizer.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

#define AUDIO_ADC_LEFT_PIN  26    // GPIO26 = ADC0, mixer left (biased, see README)
#define AUDIO_ADC_RIGHT_PIN 27    // GPIO27 = ADC1, mixer right (biased, see README)
#define LCD_RS          2
#define LCD_EN          3
#define LCD_D4          4
#define LCD_D5          5
#define LCD_D6          6
#define LCD_D7          7
#define BUTTON_PIN      8         // tactile switch: diagonal pins, GPIO -> GND, internal pull-up

#define NUM_KEYS        72        // Moonlander total; mapping is pinned in moonlander_map.h
#define SAMPLE_HZ       15625     // Per channel; 61 capture windows/s, ~30 analyses/s
#define AUDIO_INPUT_GAIN 8.0f     // Passive mixer compensation; +18 dB visualization gain
#define LCD_REFRESH_MS  100       // 10 fps; the character LCD need not steal RGB frame time
#define KEYBOARD_FRAME_US 33333   // 30 fps; Moonlander LED flush ceiling is ~38 fps

#define BOOT_LINE0      "LANDERVIZ"
#define BOOT_LINE1      "audio -> light"

// Global brightness cap — the QMK-side twin of the brownout plan.
// Keeps LED current spikes inside what the Anker hub data port + bulk cap can do.
#define BRIGHTNESS_CAP  175       // 0..255; never exceed the Moonlander's QMK cap

// ---------------------------------------------------------------------------
// Tiny RGB helpers
// ---------------------------------------------------------------------------

using RGB = lv_rgb_t;
static_assert(NUM_KEYS == LV_KEY_COUNT, "visualizer and transport key counts must match");

static RGB scale_rgb(RGB c, uint8_t k) {  // k 0..255
    return {(uint8_t)(((uint16_t)c.r * k + 127) / 255),
            (uint8_t)(((uint16_t)c.g * k + 127) / 255),
            (uint8_t)(((uint16_t)c.b * k + 127) / 255)};
}

// ---------------------------------------------------------------------------
// Keyboard LED bridge
// ---------------------------------------------------------------------------

static RGB frame[NUM_KEYS];
static lv_visualizer_t visualizer = {};
static lv_flow_v3_engine_t flow_v3 = {};

struct runtime_metrics_t {
    uint32_t max_analysis_us;
    uint32_t max_spectrum_us;
    uint32_t max_features_us;
    uint32_t max_render_us;
};

static runtime_metrics_t runtime_metrics = {};

static void board_show() {
    RGB capped[NUM_KEYS];
    for (int key = 0; key < NUM_KEYS; ++key) {
        capped[key] = scale_rgb(frame[key], BRIGHTNESS_CAP);
    }
    usb_bridge_submit_frame(capped);
}

// ---------------------------------------------------------------------------
// HD44780 LCD, 4-bit mode (QC1602A, 16x2)
// ---------------------------------------------------------------------------

static void lcd_pulse_en() {
    gpio_put(LCD_EN, 1);
    sleep_us(1);
    gpio_put(LCD_EN, 0);
    sleep_us(50);
}

static void lcd_write_nibble(uint8_t n, bool rs) {
    gpio_put(LCD_RS, rs);
    gpio_put(LCD_D4, (n >> 0) & 1);
    gpio_put(LCD_D5, (n >> 1) & 1);
    gpio_put(LCD_D6, (n >> 2) & 1);
    gpio_put(LCD_D7, (n >> 3) & 1);
    lcd_pulse_en();
}

static void lcd_cmd(uint8_t c) {
    lcd_write_nibble(c >> 4, false);
    lcd_write_nibble(c & 0x0F, false);
    sleep_us(60);
}

static void lcd_data(uint8_t c) {
    lcd_write_nibble(c >> 4, true);
    lcd_write_nibble(c & 0x0F, true);
    sleep_us(60);
}

static void lcd_init() {
    for (int p : {LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7}) {
        gpio_init(p);
        gpio_set_dir(p, GPIO_OUT);
        gpio_put(p, 0);
    }
    sleep_ms(50);
    // 4-bit init dance
    lcd_write_nibble(0x03, false); sleep_ms(5);
    lcd_write_nibble(0x03, false); sleep_us(150);
    lcd_write_nibble(0x03, false); sleep_us(150);
    lcd_write_nibble(0x02, false); sleep_us(150);
    lcd_cmd(0x28);  // 4-bit, 2 lines, 5x8
    lcd_cmd(0x0C);  // display on, no cursor
    lcd_cmd(0x06);  // entry mode
    lcd_cmd(0x01);  // clear
    sleep_ms(2);
}

// Custom chars 0..4 are VU bar segments with 1..5 filled columns. Character 5
// is a thin peak marker, visually distinct from the contiguous live bar.
static void lcd_load_vu_chars() {
    for (int ch = 0; ch < 5; ch++) {
        lcd_cmd(0x40 + ch * 8);
        uint8_t cols = ch + 1;
        uint8_t row_byte = 0;
        for (int c = 0; c < 5; c++)
            if (c < cols) row_byte |= (0x10 >> c);
        for (int r = 0; r < 8; r++) lcd_data(row_byte);
    }
    lcd_cmd(0x40 + 5 * 8);
    for (int r = 0; r < 8; r++) lcd_data(0x04);
}

// Custom chars 0..6 are vertical spectrum bars one through seven pixels tall.
// The LCD's built-in full-block character supplies the eighth level.
static void lcd_load_spectrum_chars() {
    for (int ch = 0; ch < 7; ++ch) {
        lcd_cmd(0x40 + ch * 8);
        const int height = ch + 1;
        for (int row = 0; row < 8; ++row) {
            lcd_data(row >= 8 - height ? 0x1F : 0x00);
        }
    }
}

static void lcd_set_cursor(uint8_t col, uint8_t row) {
    lcd_cmd(0x80 + (row ? 0x40 : 0x00) + col);
}

static void lcd_print(const char* s) {
    while (*s) lcd_data((uint8_t)*s++);
}

// level 0..80 -> "char + partial" rendering into a 6-char-wide bar slot
static void lcd_draw_vu_bar(uint8_t col, int level, int peak, bool clip) {
    int cells = level / 16;          // full chars (5 cols each -> 16 steps)
    int part  = (level % 16) / 3;     // partial char 0..5
    if (part > 5) part = 5;
    lcd_set_cursor(col, 0);
    for (int i = 0; i < 5; i++) {
        if (i < cells)      lcd_data(0xFF);          // full block
        else if (i == cells && part > 0) lcd_data((uint8_t)(part - 1)); // custom 0..4
        else                lcd_data(' ');
    }
    // Show the held peak only beyond the live bar. A thin tick avoids making
    // an isolated peak look like a missing or out-of-order filled cell.
    int peak_cell = peak > 0 ? (peak - 1) / 16 : -1;
    int live_cell = level > 0 ? (level - 1) / 16 : -1;
    if (peak > level && peak_cell > live_cell && peak_cell < 5) {
        lcd_set_cursor(col + peak_cell, 0);
        lcd_data(5);
    }
    lcd_set_cursor(col + 5, 0);
    lcd_data(clip ? '!' : ' ');
}

static void lcd_draw_spectrum_cell(uint8_t height) {
    if (height == 0) lcd_data(' ');
    else if (height >= 8) lcd_data(0xFF);
    else lcd_data((uint8_t)(height - 1));
}

// ---------------------------------------------------------------------------
// Audio capture: timer-driven ADC into a double buffer
// ---------------------------------------------------------------------------

static volatile uint16_t adc_buf[2][LV_AUDIO_CHANNEL_COUNT][LV_FFT_N];
static volatile uint32_t adc_completed_ms[2] = {};
static volatile int      adc_idx  = 0;
static volatile lv_audio_buffer_state_t adc_buffers = {0, -1, 0, 0};
static volatile bool     clip_flag[LV_AUDIO_CHANNEL_COUNT] = {false, false};

static bool adc_tick(struct repeating_timer*) {
    const uint8_t fill = adc_buffers.fill;
    for (int channel = 0; channel < LV_AUDIO_CHANNEL_COUNT; ++channel) {
        adc_select_input((uint)channel);
        const uint16_t value = adc_read();
        if (value < 100 || value > 3995) clip_flag[channel] = true;
        adc_buf[fill][channel][adc_idx] = value;
    }
    if (++adc_idx >= LV_FFT_N) {
        adc_idx = 0;
        const uint8_t completed = adc_buffers.fill;
        if (lv_audio_buffer_complete_decimated(
                &adc_buffers, LV_ANALYSIS_DECIMATION)) {
            adc_completed_ms[completed] =
                to_ms_since_boot(get_absolute_time());
        }
    }
    return true;
}

static lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT] = {};
static lv_audio_activity_t audio_activity = {};
static bool audio_present = false;

static void reset_metrics_after_usb_settles(uint32_t now_ms) {
    static uint32_t ready_since_ms = 0;
    static bool reset_done = false;
    if (reset_done) return;

    constexpr uint8_t required =
        LV_USB_HOST_STARTED | LV_USB_DEVICE_MOUNTED |
        LV_USB_KEYBOARD_MOUNTED | LV_USB_RAW_MOUNTED |
        LV_USB_DEVICE_ATTACHED | LV_USB_DEVICE_FULL_SPEED |
        LV_USB_ATTACH_EVENT;
    const uint8_t status = usb_bridge_host_status_history();
    if ((status & required) != required || (status & LV_USB_HOST_ERROR) != 0) {
        ready_since_ms = 0;
        return;
    }
    if (ready_since_ms == 0) {
        ready_since_ms = now_ms;
        return;
    }
    if (now_ms - ready_since_ms < 1000) return;

    runtime_metrics = {};
    const uint32_t irq_state = save_and_disable_interrupts();
    adc_buffers.dropped = 0;
    restore_interrupts(irq_state);
    usb_bridge_reset_metrics();
    reset_done = true;
}

static void analyze_frame(
    const uint16_t samples[LV_AUDIO_CHANNEL_COUNT][LV_FFT_N],
    uint32_t audio_time_ms, bool mono_flow) {
    const uint32_t spectrum_started_us = time_us_32();
    bool above_noise_gate = false;
    static lv_channel_analysis_t mono_analysis = {};
    static uint16_t mono_samples[LV_FFT_N];
    static bool prior_was_mono = false;

    if (mono_flow) {
        if (!prior_was_mono) mono_analysis = {};

        float stereo_rms[LV_AUDIO_CHANNEL_COUNT] = {};
        lv_prepare_mono_analysis(samples, mono_samples, AUDIO_INPUT_GAIN,
                                 stereo_rms);
        lv_analyze_channel(&mono_analysis, mono_samples, AUDIO_INPUT_GAIN);
        usb_bridge_device_task();

        for (int channel = 0; channel < LV_AUDIO_CHANNEL_COUNT; ++channel) {
            audio[channel] = mono_analysis;
            audio[channel].rms_level = stereo_rms[channel];
            above_noise_gate |=
                lv_meter_level(stereo_rms[channel] * 1.41421356f) > 0.0f;
        }
    } else {
        for (int channel = 0; channel < LV_AUDIO_CHANNEL_COUNT; ++channel) {
            lv_analyze_channel(&audio[channel], samples[channel],
                               AUDIO_INPUT_GAIN);
            // RMS is converted to approximate peak amplitude before applying
            // the meter's noise gate. Keyboard effects retain their more
            // sensitive visualization curve once real audio is present.
            above_noise_gate |=
                lv_meter_level(audio[channel].rms_level * 1.41421356f) > 0.0f;
            if (channel + 1 < LV_AUDIO_CHANNEL_COUNT) {
                usb_bridge_device_task();
            }
        }
    }
    prior_was_mono = mono_flow;
    runtime_metrics.max_spectrum_us = std::max(
        runtime_metrics.max_spectrum_us,
        time_us_32() - spectrum_started_us);

    const uint32_t features_started_us = time_us_32();
    const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    audio_present =
        lv_audio_activity_update(&audio_activity, above_noise_gate, now_ms);
    lv_flow_v3_engine_update(&flow_v3, audio, above_noise_gate,
                             audio_time_ms);
    runtime_metrics.max_features_us = std::max(
        runtime_metrics.max_features_us,
        time_us_32() - features_started_us);
}

// ---------------------------------------------------------------------------
// Mode button: short press = next, long press (>800 ms) = previous
// ---------------------------------------------------------------------------

static lv_viz_mode_t mode = lv_viz_mode_t::FLOW;
static uint32_t mode_announce_until_ms = 0;

static void poll_button() {
    static bool stable = true;          // pull-up: idle = high
    static bool candidate = true;
    static uint32_t press_ms = 0;
    static uint32_t candidate_since_ms = 0;
    static bool long_fired = false;

    bool raw = gpio_get(BUTTON_PIN);
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (raw != candidate) {
        candidate = raw;
        candidate_since_ms = now;
    }
    if (candidate != stable && now - candidate_since_ms >= 30) {
        stable = candidate;
        if (!stable) { press_ms = now; long_fired = false; }      // pressed
        else if (!long_fired) {                                   // released: short press
            mode = (lv_viz_mode_t)(
                ((int)mode + 1) % (int)lv_viz_mode_t::COUNT);
            mode_announce_until_ms = now + 1000;
        }
    }
    if (!stable && !long_fired && now - press_ms > 800) {         // long press
        long_fired = true;
        mode = (lv_viz_mode_t)(
            ((int)mode + (int)lv_viz_mode_t::COUNT - 1) %
            (int)lv_viz_mode_t::COUNT);
        mode_announce_until_ms = now + 1000;
    }
}

// ---------------------------------------------------------------------------
// LCD status: row 0 stereo VU or mirrored stereo spectrum,
// row 1 mode name + fps. Boot splash on power-up.
// ---------------------------------------------------------------------------

static uint32_t tenths_of_ms(uint32_t microseconds) {
    return std::min<uint32_t>((microseconds + 50) / 100, 999);
}

static void lcd_status(uint32_t now_ms, int fps) {
    const bool show_spectrum =
        mode == lv_viz_mode_t::FLOW || mode == lv_viz_mode_t::SPECTRUM;
    static bool spectrum_chars_loaded = false;
    if (show_spectrum != spectrum_chars_loaded) {
        if (show_spectrum) lcd_load_spectrum_chars();
        else lcd_load_vu_chars();
        spectrum_chars_loaded = show_spectrum;
    }

    int level[LV_AUDIO_CHANNEL_COUNT] = {};
    for (int channel = 0; channel < LV_AUDIO_CHANNEL_COUNT; ++channel) {
        // A logarithmic RMS meter behaves like an audio meter: quiet signals
        // remain visible without sacrificing headroom for loud peaks.
        const float rms_peak = audio[channel].rms_level * 1.41421356f;
        level[channel] = (int)(lv_meter_level(rms_peak) * 80.0f + 0.5f);
        if (level[channel] > 80) level[channel] = 80;
    }

    static lv_peak_hold_t peaks[LV_AUDIO_CHANNEL_COUNT] = {};
    int peak[LV_AUDIO_CHANNEL_COUNT] = {};
    for (int channel = 0; channel < LV_AUDIO_CHANNEL_COUNT; ++channel) {
        peak[channel] = lv_peak_hold_update(&peaks[channel], level[channel], now_ms);
    }

    const uint32_t irq_state = save_and_disable_interrupts();
    bool clip_seen[LV_AUDIO_CHANNEL_COUNT] = {clip_flag[0], clip_flag[1]};
    clip_flag[0] = false;
    clip_flag[1] = false;
    restore_interrupts(irq_state);

    static uint32_t clip_until_ms[LV_AUDIO_CHANNEL_COUNT] = {};
    bool clip[LV_AUDIO_CHANNEL_COUNT] = {};
    for (int channel = 0; channel < LV_AUDIO_CHANNEL_COUNT; ++channel) {
        if (clip_seen[channel]) clip_until_ms[channel] = now_ms + 500;
        clip[channel] = (int32_t)(clip_until_ms[channel] - now_ms) > 0;
    }

    if (show_spectrum) {
        uint8_t bins[LV_AUDIO_CHANNEL_COUNT][8] = {};
        for (int channel = 0; channel < LV_AUDIO_CHANNEL_COUNT; ++channel) {
            lv_lcd_spectrum_bins(audio[channel].band_level, bins[channel]);
            if (level[channel] == 0) memset(bins[channel], 0, sizeof(bins[channel]));
        }
        lcd_set_cursor(0, 0);
        for (int bin = 0; bin < 8; ++bin) {
            lcd_draw_spectrum_cell(bins[0][bin]);
        }
        for (int bin = 7; bin >= 0; --bin) {
            lcd_draw_spectrum_cell(bins[1][bin]);
        }
    } else {
        lcd_set_cursor(0, 0);
        lcd_data('L');
        lcd_draw_vu_bar(1, level[0], peak[0], clip[0]);
        lcd_set_cursor(8, 0);
        lcd_data('R');
        lcd_draw_vu_bar(9, level[1], peak[1], clip[1]);
    }

    char line1[17];
    static lv_usb_status_display_t usb_display{};
    const char *usb_label = lv_usb_status_display_update(
        &usb_display, usb_bridge_host_status_history(), now_ms);
    static const char *last_usb_label = nullptr;
    static uint32_t usb_status_until_ms = 0;
    if (last_usb_label == nullptr || strcmp(usb_label, last_usb_label) != 0) {
        last_usb_label = usb_label;
        usb_status_until_ms = now_ms + 3000;
    }
    const bool show_usb_status = strcmp(usb_label, "OK") != 0 ||
                                 (int32_t)(usb_status_until_ms - now_ms) > 0;
    if (show_usb_status) {
        snprintf(line1, sizeof(line1), "USB: %-11s", usb_label);
    } else if ((int32_t)(mode_announce_until_ms - now_ms) > 0) {
        snprintf(line1, sizeof(line1), ">%-15s", lv_viz_mode_name(mode));
    } else if (mode == lv_viz_mode_t::FLOW &&
               (now_ms / 2000u) % 4u == 1u) {
        const uint32_t missed_audio_windows = adc_buffers.dropped;
        snprintf(line1, sizeof(line1), "A%03lu R%03lu M%03lu  ",
                 (unsigned long)tenths_of_ms(
                     runtime_metrics.max_analysis_us),
                 (unsigned long)tenths_of_ms(
                     runtime_metrics.max_render_us),
                 (unsigned long)std::min<uint32_t>(missed_audio_windows,
                                                   999));
    } else if (mode == lv_viz_mode_t::FLOW &&
               (now_ms / 2000u) % 4u == 2u) {
        snprintf(line1, sizeof(line1), "F%03lu E%03lu       ",
                 (unsigned long)tenths_of_ms(
                     runtime_metrics.max_spectrum_us),
                 (unsigned long)tenths_of_ms(
                     runtime_metrics.max_features_us));
    } else if (mode == lv_viz_mode_t::FLOW &&
               (now_ms / 2000u) % 4u == 3u) {
        const lv_usb_bridge_metrics_t usb = usb_bridge_metrics();
        snprintf(line1, sizeof(line1), "D%03lu H%03lu K%03lu  ",
                 (unsigned long)tenths_of_ms(
                     usb.max_device_task_gap_us),
                 (unsigned long)tenths_of_ms(
                     usb.max_host_task_gap_us),
                 (unsigned long)tenths_of_ms(
                     usb.max_keyboard_forward_us));
    } else {
        const int display_fps = fps < 0 ? 0 : (fps > 999 ? 999 : fps);
        memset(line1, ' ', 16);
        line1[16] = '\0';
        const char *mode_name = lv_viz_mode_name(mode);
        for (int i = 0; i < 8 && mode_name[i] != '\0'; ++i) line1[i] = mode_name[i];
        line1[9] = display_fps >= 100 ? (char)('0' + display_fps / 100) : ' ';
        line1[10] = display_fps >= 10 ? (char)('0' + display_fps / 10 % 10) : ' ';
        line1[11] = (char)('0' + display_fps % 10);
        line1[12] = 'f';
        line1[13] = 'p';
        line1[14] = 's';
    }
    lcd_set_cursor(0, 1);
    lcd_print(line1);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    lv_audio_analysis_init();
    usb_bridge_init();
    lv_flow_v3_engine_init(&flow_v3);

    // LCD
    lcd_init();
    lcd_load_vu_chars();
    lcd_set_cursor(0, 0); lcd_print(BOOT_LINE0);
    lcd_set_cursor(0, 1); lcd_print(BOOT_LINE1);
    sleep_ms(1200);
    lcd_cmd(0x01); sleep_ms(2);

    // Button
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    // ADC
    adc_init();
    adc_gpio_init(AUDIO_ADC_LEFT_PIN);
    adc_gpio_init(AUDIO_ADC_RIGHT_PIN);

    static struct repeating_timer timer;
    add_repeating_timer_us(-1000000 / SAMPLE_HZ, adc_tick, NULL, &timer);

    uint32_t last_fps_ms = to_ms_since_boot(get_absolute_time());
    uint32_t last_lcd_ms = last_fps_ms;
    uint32_t last_render_us = time_us_32();
    uint32_t last_completed_frames = usb_bridge_completed_frame_count();
    int fps = 0;
    uint16_t samples[LV_AUDIO_CHANNEL_COUNT][LV_FFT_N];

    while (true) {
        usb_bridge_device_task();
        uint32_t now = to_ms_since_boot(get_absolute_time());
        reset_metrics_after_usb_settles(now);
        poll_button();

        if (adc_buffers.ready >= 0) {
            const uint32_t irq_state = save_and_disable_interrupts();
            const int completed = lv_audio_buffer_take(&adc_buffers);
            const uint32_t completed_ms = completed >= 0
                                              ? adc_completed_ms[completed]
                                              : 0;
            if (completed >= 0) {
                memcpy(samples, (const void *)adc_buf[completed], sizeof(samples));
            }
            restore_interrupts(irq_state);
            if (completed >= 0) {
                constexpr uint32_t kAudioHalfWindowMs =
                    LV_FFT_N * 1000u / SAMPLE_HZ / 2u;
                const uint32_t analysis_started_us = time_us_32();
                analyze_frame(samples, completed_ms - kAudioHalfWindowMs,
                              mode == lv_viz_mode_t::FLOW);
                runtime_metrics.max_analysis_us = std::max(
                    runtime_metrics.max_analysis_us,
                    time_us_32() - analysis_started_us);
                usb_bridge_device_task();
            }
        }

        const uint32_t render_us = time_us_32();
        if (render_us - last_render_us >= KEYBOARD_FRAME_US) {
            last_render_us += KEYBOARD_FRAME_US;
            if (render_us - last_render_us >= KEYBOARD_FRAME_US) {
                // Do not burst stale animation frames after an unusually long
                // LCD or USB stall; resume from the current wall-clock frame.
                last_render_us = render_us;
            }
            const uint32_t render_started_us = time_us_32();
            if (mode == lv_viz_mode_t::FLOW && audio_present) {
                lv_flow_v3_engine_render(
                    &flow_v3, now, frame, usb_bridge_device_task);
            } else {
                lv_visualizer_render(&visualizer, mode, audio,
                                     audio_present, now, frame);
            }
            board_show();
            runtime_metrics.max_render_us = std::max(
                runtime_metrics.max_render_us,
                time_us_32() - render_started_us);
        }

        if (now - last_fps_ms >= 1000) {
            const uint32_t completed = usb_bridge_completed_frame_count();
            const uint32_t elapsed_ms = now - last_fps_ms;
            fps = (int)(((completed - last_completed_frames) * 1000u +
                         elapsed_ms / 2u) /
                        elapsed_ms);
            last_completed_frames = completed;
            last_fps_ms = now;
        }

        if (now - last_lcd_ms >= LCD_REFRESH_MS) {
            last_lcd_ms = now;
            lcd_status(now, fps);
        }
    }
}
