// LanderViz — Moonlander audio visualizer firmware (v1)
// Target: SparkFun Pro Micro RP2040, Raspberry Pi Pico SDK, C++17
//
// Implements the audio visualization modes:
//   - SPECTRUM: column analyzer, mirrored across the split
//   - PULSE:    kick-driven whole-board breath, slow hue rotation
//   - VU:       smoothed stereo bottom-to-top meters
//   - IDLE:     slow aurora, also shown when the mixer is silent
// Plus: QC1602A LCD (stereo VU + peak-hold + clip, mode/fps status),
//       tactile mode button (short = next, long = previous).

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
#include "landerviz/moonlander_map.h"
#include "landerviz/usb_bridge.h"

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
#define SAMPLE_HZ       15625     // Per channel; 256-pt FFT => ~61 Hz spectrum refresh
#define AUDIO_INPUT_GAIN 8.0f     // Passive mixer compensation; +18 dB visualization gain
#define LCD_REFRESH_MS  50        // 20 fps; keep LCD writes from delaying USB too often

#define BOOT_LINE0      "LANDERVIZ"
#define BOOT_LINE1      "audio -> light"

// Global brightness cap — the QMK-side twin of the brownout plan.
// Keeps LED current spikes inside what the Anker hub data port + bulk cap can do.
#define BRIGHTNESS_CAP  175       // 0..255; never exceed the Moonlander's QMK cap

enum class VizMode : uint8_t { SPECTRUM = 0, PULSE = 1, VU = 2, IDLE = 3, COUNT = 4 };
static const char* const MODE_NAMES[] = { "SPECTRUM", "PULSE", "VU", "IDLE" };

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

// half: 0 = left, 1 = right. col/row address each half's 6x5 ragged grid.
static inline int key_at(int half, int col, int row, int cols_per_half, int rows_per_half) {
    if (cols_per_half != 6 || rows_per_half != 5) return -1;
    return lv_key_at(half, col, row);
}

static void board_set(int key, RGB c) {
    if (key < 0 || key >= NUM_KEYS) return;
    frame[key] = scale_rgb(c, BRIGHTNESS_CAP);
}

static void board_clear() { memset(frame, 0, sizeof(frame)); }

static void board_show() {
    usb_bridge_submit_frame(frame);
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

// ---------------------------------------------------------------------------
// Audio capture: timer-driven ADC into a double buffer
// ---------------------------------------------------------------------------

static volatile uint16_t adc_buf[2][LV_AUDIO_CHANNEL_COUNT][LV_FFT_N];
static volatile int      adc_idx  = 0;
static volatile lv_audio_buffer_state_t adc_buffers = {0, -1};
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
        lv_audio_buffer_complete(&adc_buffers);
    }
    return true;
}

static lv_channel_analysis_t audio[LV_AUDIO_CHANNEL_COUNT] = {};
static lv_audio_activity_t audio_activity = {};
static bool audio_present = false;

static void analyze_frame(const uint16_t samples[LV_AUDIO_CHANNEL_COUNT][LV_FFT_N]) {
    bool above_noise_gate = false;
    for (int channel = 0; channel < LV_AUDIO_CHANNEL_COUNT; ++channel) {
        lv_analyze_channel(&audio[channel], samples[channel], AUDIO_INPUT_GAIN);
        // RMS is converted to approximate peak amplitude before applying the
        // meter's noise gate. Keyboard effects retain their more sensitive
        // visualization curve once real audio is present.
        above_noise_gate |=
            lv_meter_level(audio[channel].rms_level * 1.41421356f) > 0.0f;
    }

    const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    audio_present =
        lv_audio_activity_update(&audio_activity, above_noise_gate, now_ms);
}

// ---------------------------------------------------------------------------
// Visualizations — all write into `frame`, then board_show() pushes it.
// ---------------------------------------------------------------------------

static void viz_spectrum(uint32_t now_ms) {
    (void)now_ms;
    board_clear();
    const int cols = 6, rows = 5;
    RGB rendered[2][5][6]{};
    static float peaks[LV_AUDIO_CHANNEL_COUNT][6] = {};
    for (int half = 0; half < 2; half++) {
        for (int c = 0; c < cols; c++) {
            // Each stereo channel drives its matching keyboard half. Bass remains
            // at the outside edge and treble remains toward the center split.
            const int visual_band = half == 0 ? c : cols - 1 - c;
            const int band = visual_band * 2;
            const float lvl = lv_perceptual_level(
                fmaxf(audio[half].band_level[band], audio[half].band_level[band + 1]));
            const int active_rows = lv_key_at(half, c, rows - 1) >= 0 ? rows : rows - 1;
            int lit = (int)(lvl * active_rows + 0.5f);
            uint8_t hue = (uint8_t)(visual_band * 42);  // red -> violet, outside -> split
            for (int r = 0; r < active_rows; r++) {
                RGB col = (r < lit)
                    ? lv_hsv_to_rgb(hue, 255, 255)
                    : lv_hsv_to_rgb(hue, 255, 20);   // dim floor so the board never looks dead
                const int row = active_rows - 1 - r;
                rendered[half][row][c] = col;
                board_set(key_at(half, c, row, cols, rows), col);
            }
            // peak-hold dot on top
            if (lvl > peaks[half][visual_band]) peaks[half][visual_band] = lvl;
            else peaks[half][visual_band] *= 0.985f;
            int pk = (int)(peaks[half][visual_band] * active_rows);
            if (pk > 0 && pk <= active_rows) {
                const int row = active_rows - pk;
                rendered[half][row][c] = {255, 255, 255};
                board_set(key_at(half, c, row, cols, rows), {255, 255, 255});
            }
        }

        // The three inner utility keys mirror the nearest (treble) column.
        const int inner_col = half == 0 ? cols - 1 : 0;
        for (int key = 0; key < 3; ++key) {
            board_set(lv_utility_key_at(half, key), rendered[half][key][inner_col]);
        }

        // The thumb cluster acts as a stereo bass pulse for its half.
        const float bass = lv_perceptual_level(
            fmaxf(audio[half].band_level[0], audio[half].band_level[1]));
        const uint8_t thumb_value = (uint8_t)(20.0f + bass * 235.0f);
        const RGB thumb_color = lv_hsv_to_rgb(0, 255, thumb_value);
        for (int key = 0; key < 4; ++key) {
            board_set(lv_thumb_key_at(half, key), thumb_color);
        }
    }
}

static uint8_t pulse_hue = 0;

static void viz_pulse(uint32_t now_ms) {
    const float stereo_bass = fmaxf(audio[0].bass_env, audio[1].bass_env);
    const float visible_bass = lv_perceptual_level(stereo_bass);
    uint8_t breath = (uint8_t)(24.0f + visible_bass * 231.0f);
    // slow hue drift so it never looks static between kicks
    static uint32_t last_hue_ms = 0;
    if (now_ms - last_hue_ms >= 120) {
        pulse_hue += (uint8_t)((now_ms - last_hue_ms) / 120);
        last_hue_ms = now_ms;
    }
    RGB col = lv_hsv_to_rgb(pulse_hue, 220, breath);
    for (int k = 0; k < NUM_KEYS; k++) board_set(k, col);
}

static void viz_vu(uint32_t now_ms) {
    (void)now_ms;
    static float displayed_levels[LV_AUDIO_CHANNEL_COUNT] = {};

    for (int half = 0; half < LV_AUDIO_CHANNEL_COUNT; ++half) {
        const float rms_peak = audio[half].rms_level * 1.41421356f;
        const float target_level = lv_keyboard_vu_level(rms_peak);
        displayed_levels[half] =
            lv_vu_smooth_level(displayed_levels[half], target_level);
        const float level = displayed_levels[half];
        for (int col = 0; col < 6; ++col) {
            const int row_count = lv_key_at(half, col, 4) >= 0 ? 5 : 4;
            for (int row_from_bottom = 0; row_from_bottom < row_count;
                 ++row_from_bottom) {
                const int row = row_count - 1 - row_from_bottom;
                board_set(lv_key_at(half, col, row),
                          lv_vu_row_color(level, row_from_bottom, row_count));
            }
        }

        for (int key = 0; key < 3; ++key) {
            const int row_from_bottom = 2 - key;
            board_set(lv_utility_key_at(half, key),
                      lv_vu_row_color(level, row_from_bottom, 3));
        }

        const RGB thumb_color = lv_vu_row_color(level, 0, 5);
        for (int key = 0; key < 4; ++key) {
            board_set(lv_thumb_key_at(half, key), thumb_color);
        }
    }
}

static void viz_idle(uint32_t now_ms) {
    for (int half = 0; half < 2; ++half) {
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < 6; ++col) {
                const float x = half == 0 ? (float)col : (float)(7 + col);
                board_set(lv_key_at(half, col, row),
                          lv_aurora_color(x, (float)row, now_ms));
            }
        }

        const float inner_x = half == 0 ? 5.8f : 6.2f;
        for (int key = 0; key < 3; ++key) {
            board_set(lv_utility_key_at(half, key),
                      lv_aurora_color(inner_x, 0.8f + key, now_ms));
        }

        for (int key = 0; key < 4; ++key) {
            const float x = half == 0 ? 3.8f + key * 0.55f
                                      : 8.2f - key * 0.55f;
            board_set(lv_thumb_key_at(half, key),
                      lv_aurora_color(x, 5.0f + (key & 1) * 0.35f, now_ms));
        }
    }
}

// ---------------------------------------------------------------------------
// Mode button: short press = next, long press (>800 ms) = previous
// ---------------------------------------------------------------------------

static VizMode mode = VizMode::SPECTRUM;
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
            mode = (VizMode)(((int)mode + 1) % (int)VizMode::COUNT);
            mode_announce_until_ms = now + 1000;
        }
    }
    if (!stable && !long_fired && now - press_ms > 800) {         // long press
        long_fired = true;
        mode = (VizMode)(((int)mode + (int)VizMode::COUNT - 1) % (int)VizMode::COUNT);
        mode_announce_until_ms = now + 1000;
    }
}

// ---------------------------------------------------------------------------
// LCD status: row 0 stereo VU,
// row 1 mode name + fps. Boot splash on power-up.
// ---------------------------------------------------------------------------

static void lcd_status(uint32_t now_ms, int fps) {
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

    lcd_set_cursor(0, 0);
    lcd_data('L');
    lcd_draw_vu_bar(1, level[0], peak[0], clip[0]);
    lcd_set_cursor(8, 0);
    lcd_data('R');
    lcd_draw_vu_bar(9, level[1], peak[1], clip[1]);

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
        snprintf(line1, sizeof(line1), ">%-15s", MODE_NAMES[(int)mode]);
    } else {
        const int display_fps = fps < 0 ? 0 : (fps > 999 ? 999 : fps);
        memset(line1, ' ', 16);
        line1[16] = '\0';
        const char *mode_name = MODE_NAMES[(int)mode];
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
    usb_bridge_init();

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
    int frames = 0, fps = 0;
    uint16_t samples[LV_AUDIO_CHANNEL_COUNT][LV_FFT_N];

    while (true) {
        usb_bridge_device_task();
        uint32_t now = to_ms_since_boot(get_absolute_time());
        poll_button();

        if (adc_buffers.ready >= 0) {
            const uint32_t irq_state = save_and_disable_interrupts();
            const int completed = lv_audio_buffer_take(&adc_buffers);
            if (completed >= 0) {
                memcpy(samples, (const void *)adc_buf[completed], sizeof(samples));
            }
            restore_interrupts(irq_state);
            if (completed >= 0) analyze_frame(samples);
        }

        const uint32_t render_us = time_us_32();
        if (render_us - last_render_us >= 16667) {
            last_render_us = render_us;
            if (!audio_present && mode != VizMode::IDLE) {
                viz_idle(now);
            } else {
                switch (mode) {
                    case VizMode::SPECTRUM: viz_spectrum(now); break;
                    case VizMode::PULSE:    viz_pulse(now);    break;
                    case VizMode::VU:       viz_vu(now);       break;
                    case VizMode::IDLE:     viz_idle(now);     break;
                    default: break;
                }
            }
            board_show();
            frames++;
        }

        if (now - last_fps_ms >= 1000) {
            fps = frames;
            frames = 0;
            last_fps_ms = now;
        }

        if (now - last_lcd_ms >= LCD_REFRESH_MS) {
            last_lcd_ms = now;
            lcd_status(now, fps);
        }
    }
}
