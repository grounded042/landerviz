// LanderViz — Moonlander audio visualizer firmware (v1)
// Target: SparkFun Pro Micro RP2040, Raspberry Pi Pico SDK, C++17
//
// Implements the audio visualization modes:
//   - FLOW:     mono spectral ribbons moving out from the center split
//   - SPECTRUM: column analyzer, mirrored across the split
//   - PULSE:    kick-driven whole-board breath, slow hue rotation
//   - VU:       smoothed stereo bottom-to-top meters
//   - IDLE:     slow aurora, also shown when the mixer is silent
// Plus: PCF8574-backed 16x2 LCD (spectrum or stereo VU, mode/fps status),
//       tactile mode button (short = next, long = previous).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/sync.h"
#include "hardware/timer.h"

#include "landerviz/audio_analysis.h"
#include "landerviz/audio_buffer.h"
#include "landerviz/effects.h"
#include "landerviz/flow_v3_engine.h"
#include "landerviz/moonlander_map.h"
#include "landerviz/pcf8574_lcd.h"
#ifdef LANDERVIZ_POWER_TEST
#include "landerviz/power_test.h"
#endif
#include "landerviz/usb_bridge.h"
#include "landerviz/visualizer.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

#define AUDIO_ADC_LEFT_PIN  26    // GPIO26 = ADC0, mixer left (biased, see README)
#define AUDIO_ADC_RIGHT_PIN 27    // GPIO27 = ADC1, mixer right (biased, see README)
#define LCD_SDA_PIN     2
#define LCD_SCL_PIN     3
#define LCD_I2C_HZ      100000
#define BUTTON_PIN      8         // tactile switch: diagonal pins, GPIO -> GND, internal pull-up

#define NUM_KEYS        72        // Moonlander total; mapping is pinned in moonlander_map.h
#define SAMPLE_HZ       15625     // Per channel; 61 capture windows/s, ~30 analyses/s
#define AUDIO_INPUT_GAIN 8.0f     // Passive mixer compensation; +18 dB visualization gain
#define AUDIO_CALIBRATION_DISPLAY 1 // Temporary: physical input + FLOW bass on LCD
#define LCD_REFRESH_MS  100       // 10 fps; the character LCD need not steal RGB frame time
#define KEYBOARD_FRAME_US 33333   // 30 fps; Moonlander LED flush ceiling is ~38 fps

#ifdef LANDERVIZ_POWER_TEST
#define BOOT_LINE0      "POWER TEST"
#define BOOT_LINE1      "starts black"
#else
#define BOOT_LINE0      "LANDERVIZ"
#define BOOT_LINE1      "audio -> light"
#endif

// Global brightness cap — the QMK-side twin of the brownout plan.
// Keeps LED current spikes inside what the Anker hub data port + bulk cap can do.
#ifndef LANDERVIZ_BRIGHTNESS_CAP
#define LANDERVIZ_BRIGHTNESS_CAP 175
#endif

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
#ifndef LANDERVIZ_POWER_TEST
static lv_visualizer_t visualizer = {};
#endif
static lv_flow_v3_engine_t flow_v3 = {};
#ifdef LANDERVIZ_POWER_TEST
static lv_power_test_t power_test = {};
#endif

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
        capped[key] = scale_rgb(frame[key], LANDERVIZ_BRIGHTNESS_CAP);
    }
    usb_bridge_submit_frame(capped);
}

// ---------------------------------------------------------------------------
// HD44780 LCD through a PCF8574 I2C backpack, 4-bit mode (16x2)
// ---------------------------------------------------------------------------

static uint8_t lcd_i2c_address = 0;
static bool lcd_present = false;
static bool lcd_cgram_selected = false;
static uint8_t lcd_buffer_cursor = 0;
static std::array<uint8_t, LV_LCD_CELL_COUNT> lcd_desired = {};
static std::array<uint8_t, LV_LCD_CELL_COUNT> lcd_shadow = {};

static bool lcd_write_expander(const uint8_t *bytes, size_t count) {
    if (!lcd_present) return false;
    const int written = i2c_write_timeout_us(
        i2c1, lcd_i2c_address, bytes, count, false, 2000);
    // A complete LCD byte occupies about 0.6 ms at 100 kHz. Service the
    // upstream keyboard device between bytes so display refreshes do not turn
    // into typing latency.
    usb_bridge_device_task();
    return written == static_cast<int>(count);
}

static void lcd_write_nibble(uint8_t n, bool rs) {
    const uint8_t bytes[] = {
        lv_pcf8574_encode_nibble(n, rs, false),
        lv_pcf8574_encode_nibble(n, rs, true),
        lv_pcf8574_encode_nibble(n, rs, false),
    };
    lcd_write_expander(bytes, sizeof(bytes));
}

static bool lcd_write_byte(uint8_t value, bool rs) {
    const auto bytes = lv_pcf8574_encode_byte(value, rs);
    return lcd_write_expander(bytes.data(), bytes.size());
}

static bool lcd_hw_cmd(uint8_t c) {
    const bool written = lcd_write_byte(c, false);
    sleep_us(60);
    return written;
}

static bool lcd_hw_data(uint8_t c) {
    const bool written = lcd_write_byte(c, true);
    sleep_us(60);
    return written;
}

static void lcd_cmd(uint8_t c) {
    lcd_hw_cmd(c);
    if (c == 0x01) {
        lcd_desired.fill(' ');
        lcd_shadow.fill(' ');
        lcd_buffer_cursor = 0;
        lcd_cgram_selected = false;
    } else if ((c & 0xC0u) == 0x40u) {
        lcd_cgram_selected = true;
    }
}

static void lcd_data(uint8_t c) {
    if (lcd_cgram_selected) {
        lcd_hw_data(c);
        return;
    }
    if (lcd_buffer_cursor < LV_LCD_CELL_COUNT) {
        lcd_desired[lcd_buffer_cursor++] = c;
    }
}

static void lcd_flush() {
    if (!lcd_present) return;

    lv_lcd_dirty_run_t run{};
    uint8_t from = 0;
    while (lv_lcd_next_dirty_run(lcd_desired, lcd_shadow, from, &run)) {
        const uint8_t row = run.begin / LV_LCD_COLUMNS;
        const uint8_t col = run.begin % LV_LCD_COLUMNS;
        const uint8_t address = static_cast<uint8_t>(
            0x80u + (row == 0 ? 0x00u : 0x40u) + col);
        if (!lcd_hw_cmd(address)) return;
        for (uint8_t cell = run.begin; cell < run.end; ++cell) {
            if (!lcd_hw_data(lcd_desired[cell])) return;
            lcd_shadow[cell] = lcd_desired[cell];
        }
        from = run.end;
    }
}

static void lcd_init() {
    lcd_desired.fill(' ');
    lcd_shadow.fill(0xFF);
    i2c_init(i2c1, LCD_I2C_HZ);
    gpio_set_function(LCD_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(LCD_SCL_PIN, GPIO_FUNC_I2C);
    sleep_ms(50);

    const uint8_t idle = LV_PCF8574_BACKLIGHT;
    for (const uint8_t candidate : LV_PCF8574_ADDRESS_CANDIDATES) {
        const int written = i2c_write_timeout_us(
            i2c1, candidate, &idle, 1, false, 2000);
        usb_bridge_device_task();
        if (written == 1) {
            lcd_i2c_address = candidate;
            lcd_present = true;
            break;
        }
    }

    if (!lcd_present) return;

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
    lcd_cgram_selected = false;
    lcd_buffer_cursor = static_cast<uint8_t>(
        std::min<uint16_t>(row * LV_LCD_COLUMNS + col,
                           LV_LCD_CELL_COUNT));
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
static lv_audio_normalizer_t flow_normalizer = {};
static lv_audio_calibration_t audio_calibration = {};

enum class input_calibration_state_t {
    NONE,
    WAIT_RELEASE,
    COLLECTING,
    COMPLETE,
    FAILED,
};

static input_calibration_state_t input_calibration_state =
    input_calibration_state_t::NONE;
static uint32_t input_calibration_message_until_ms = 0;
static constexpr uint16_t INPUT_CALIBRATION_FRAMES = 92;
static bool audio_present = false;
static float physical_rms_max[LV_AUDIO_CHANNEL_COUNT] = {};
static float physical_peak_max[LV_AUDIO_CHANNEL_COUNT] = {};
static float diagnostic_bass_max = 0.0f;
static float diagnostic_body_max = 0.0f;

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
    static bool prior_was_calibrating = false;

    if (mono_flow) {
        const bool calibrating =
            input_calibration_state ==
                input_calibration_state_t::WAIT_RELEASE ||
            input_calibration_state ==
                input_calibration_state_t::COLLECTING;
        if (!prior_was_mono || (prior_was_calibrating && !calibrating)) {
            mono_analysis = {};
        }

        float stereo_rms[LV_AUDIO_CHANNEL_COUNT] = {};
        float stereo_peak[LV_AUDIO_CHANNEL_COUNT] = {};
        lv_prepare_mono_analysis(samples, mono_samples, AUDIO_INPUT_GAIN,
                                 stereo_rms, stereo_peak);
        const float shared_rms = std::max(stereo_rms[0], stereo_rms[1]);
        lv_analyze_channel(&mono_analysis, mono_samples, AUDIO_INPUT_GAIN);
        usb_bridge_device_task();

        if (input_calibration_state ==
            input_calibration_state_t::COLLECTING) {
            lv_audio_calibration_add(
                &audio_calibration, shared_rms, &mono_analysis);
            if (audio_calibration.frames_seen >=
                INPUT_CALIBRATION_FRAMES) {
                const bool accepted = lv_audio_calibration_finish(
                    &audio_calibration);
                flow_normalizer = {};
                if (accepted) {
                    flow_normalizer.noise_floor =
                        audio_calibration.rms_floor;
                    input_calibration_state =
                        input_calibration_state_t::COMPLETE;
                } else {
                    input_calibration_state =
                        input_calibration_state_t::FAILED;
                }
                input_calibration_message_until_ms =
                    to_ms_since_boot(get_absolute_time()) + 2000;
                audio_activity = {};
                audio_present = false;
                lv_flow_v3_engine_init(&flow_v3);
            }
        }

        float adaptive_gain = 1.0f;
        if (!calibrating) {
            const bool calibrated_spectral_signal =
                lv_audio_calibration_signal_present(
                    &audio_calibration, &mono_analysis);
            adaptive_gain = lv_audio_normalizer_update(
                &flow_normalizer, shared_rms,
                calibrated_spectral_signal, audio_time_ms);
        }
        lv_channel_analysis_t normalized_analysis = mono_analysis;
        lv_audio_calibration_apply(
            &audio_calibration, &normalized_analysis, adaptive_gain);

        for (int channel = 0; channel < LV_AUDIO_CHANNEL_COUNT; ++channel) {
            audio[channel] = normalized_analysis;
            audio[channel].rms_level = lv_audio_calibrated_level(
                &audio_calibration, stereo_rms[channel], adaptive_gain);
            physical_rms_max[channel] = std::max(
                physical_rms_max[channel],
                stereo_rms[channel] / AUDIO_INPUT_GAIN);
            physical_peak_max[channel] = std::max(
                physical_peak_max[channel],
                stereo_peak[channel] / AUDIO_INPUT_GAIN);
        }
        above_noise_gate = !calibrating && flow_normalizer.gate_open;
        prior_was_calibrating = calibrating;
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
    if (!mono_flow) prior_was_calibrating = false;
    runtime_metrics.max_spectrum_us = std::max(
        runtime_metrics.max_spectrum_us,
        time_us_32() - spectrum_started_us);

    const uint32_t features_started_us = time_us_32();
    const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    audio_present =
        lv_audio_activity_update(&audio_activity, above_noise_gate, now_ms);
    lv_flow_v3_engine_update(&flow_v3, audio, above_noise_gate,
                             audio_time_ms);
    if (mono_flow) {
        diagnostic_bass_max = std::max(
            diagnostic_bass_max, flow_v3.bass.features.raw_bass);
        diagnostic_body_max = std::max(
            diagnostic_body_max, flow_v3.bass.features.body);
    }
    runtime_metrics.max_features_us = std::max(
        runtime_metrics.max_features_us,
        time_us_32() - features_started_us);
}

// ---------------------------------------------------------------------------
// Mode button: short press = next, long press (>800 ms) = previous
// ---------------------------------------------------------------------------

static lv_viz_mode_t mode = lv_viz_mode_t::FLOW;
static uint32_t mode_announce_until_ms = 0;

static bool poll_input_calibration(uint32_t now_ms) {
    static uint32_t release_since_ms = 0;
    switch (input_calibration_state) {
        case input_calibration_state_t::NONE:
            return false;
        case input_calibration_state_t::WAIT_RELEASE:
            if (!gpio_get(BUTTON_PIN)) {
                release_since_ms = 0;
                return true;
            }
            if (release_since_ms == 0) release_since_ms = now_ms;
            if (now_ms - release_since_ms >= 30) {
                lv_audio_calibration_reset(&audio_calibration);
                flow_normalizer = {};
                audio_activity = {};
                audio_present = false;
                input_calibration_state =
                    input_calibration_state_t::COLLECTING;
                release_since_ms = 0;
            }
            return true;
        case input_calibration_state_t::COLLECTING:
            return true;
        case input_calibration_state_t::COMPLETE:
        case input_calibration_state_t::FAILED:
            if ((int32_t)(input_calibration_message_until_ms - now_ms) > 0) {
                return true;
            }
            input_calibration_state = input_calibration_state_t::NONE;
            return false;
    }
    return false;
}

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
#ifdef LANDERVIZ_POWER_TEST
            lv_power_test_next(&power_test);
#else
            mode = (lv_viz_mode_t)(
                ((int)mode + 1) % (int)lv_viz_mode_t::COUNT);
            mode_announce_until_ms = now + 1000;
#endif
        }
    }
    if (!stable && !long_fired && now - press_ms > 800) {         // long press
        long_fired = true;
#ifdef LANDERVIZ_POWER_TEST
        lv_power_test_previous(&power_test);
#else
        mode = (lv_viz_mode_t)(
            ((int)mode + (int)lv_viz_mode_t::COUNT - 1) %
            (int)lv_viz_mode_t::COUNT);
        mode_announce_until_ms = now + 1000;
#endif
    }
}

// ---------------------------------------------------------------------------
// LCD status: row 0 stereo VU or mirrored stereo spectrum,
// row 1 mode name + fps. Boot splash on power-up.
// ---------------------------------------------------------------------------

#if !AUDIO_CALIBRATION_DISPLAY
static uint32_t tenths_of_ms(uint32_t microseconds) {
    return std::min<uint32_t>((microseconds + 50) / 100, 999);
}
#endif

typedef struct {
    float rms[LV_AUDIO_CHANNEL_COUNT];
    float peak[LV_AUDIO_CHANNEL_COUNT];
    float displayed_rms[LV_AUDIO_CHANNEL_COUNT];
    float displayed_peak[LV_AUDIO_CHANNEL_COUNT];
    float bass;
    float body;
    float displayed_bass;
    float displayed_body;
    uint32_t window_started_ms;
    bool published;
} input_diagnostic_t;

static input_diagnostic_t input_diagnostic = {};

static bool lcd_input_calibration(uint32_t now_ms) {
    (void)now_ms;
    const char *line0 = nullptr;
    char line1[17] = {};
    switch (input_calibration_state) {
        case input_calibration_state_t::NONE:
            return false;
        case input_calibration_state_t::WAIT_RELEASE:
            line0 = "INPUT CAL";
            snprintf(line1, sizeof(line1), "%-16s", "RELEASE BUTTON");
            break;
        case input_calibration_state_t::COLLECTING: {
            line0 = "INPUT CAL";
            const uint32_t percent = std::min<uint32_t>(
                audio_calibration.frames_seen * 100u /
                    INPUT_CALIBRATION_FRAMES,
                100u);
            snprintf(line1, sizeof(line1), "SILENCE %3lu%%    ",
                     (unsigned long)percent);
            break;
        }
        case input_calibration_state_t::COMPLETE:
            line0 = "CALIBRATION OK";
            snprintf(line1, sizeof(line1), "NOISE %3d dBFS ",
                     lv_level_dbfs(audio_calibration.rms_floor /
                                   AUDIO_INPUT_GAIN));
            break;
        case input_calibration_state_t::FAILED:
            line0 = "CALIBRATION BAD";
            snprintf(line1, sizeof(line1), "%-16s", "MUSIC? TRY AGAIN");
            break;
    }

    char padded_line0[17];
    char padded_line1[17];
    snprintf(padded_line0, sizeof(padded_line0), "%-16s", line0);
    snprintf(padded_line1, sizeof(padded_line1), "%-16s", line1);
    lcd_set_cursor(0, 0);
    lcd_print(padded_line0);
    lcd_set_cursor(0, 1);
    lcd_print(padded_line1);
    return true;
}

static void update_input_diagnostic(uint32_t now_ms) {
    for (int channel = 0; channel < LV_AUDIO_CHANNEL_COUNT; ++channel) {
        input_diagnostic.rms[channel] = std::max(
            input_diagnostic.rms[channel], physical_rms_max[channel]);
        input_diagnostic.peak[channel] = std::max(
            input_diagnostic.peak[channel], physical_peak_max[channel]);
        physical_rms_max[channel] = 0.0f;
        physical_peak_max[channel] = 0.0f;
    }
    input_diagnostic.bass = std::max(
        input_diagnostic.bass, diagnostic_bass_max);
    input_diagnostic.body = std::max(
        input_diagnostic.body, diagnostic_body_max);
    diagnostic_bass_max = 0.0f;
    diagnostic_body_max = 0.0f;
    if (input_diagnostic.window_started_ms == 0) {
        input_diagnostic.window_started_ms = now_ms;
    }
    if (now_ms - input_diagnostic.window_started_ms < 1000) return;

    for (int channel = 0; channel < LV_AUDIO_CHANNEL_COUNT; ++channel) {
        input_diagnostic.displayed_rms[channel] =
            input_diagnostic.rms[channel];
        input_diagnostic.displayed_peak[channel] =
            input_diagnostic.peak[channel];
        input_diagnostic.rms[channel] = 0.0f;
        input_diagnostic.peak[channel] = 0.0f;
    }
    input_diagnostic.displayed_bass = input_diagnostic.bass;
    input_diagnostic.displayed_body = input_diagnostic.body;
    input_diagnostic.bass = 0.0f;
    input_diagnostic.body = 0.0f;
    input_diagnostic.window_started_ms = now_ms;
    input_diagnostic.published = true;
}

static void lcd_status(uint32_t now_ms, int fps) {
#ifdef LANDERVIZ_POWER_TEST
    (void)now_ms;
    (void)fps;
    char line[17];
    lcd_set_cursor(0, 0);
    lcd_print("POWER TEST      ");
    snprintf(line, sizeof(line), "WHITE %3u       ",
             (unsigned int)lv_power_test_level(&power_test));
    lcd_set_cursor(0, 1);
    lcd_print(line);
    return;
#endif
    if (lcd_input_calibration(now_ms)) return;
    update_input_diagnostic(now_ms);
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
#if AUDIO_CALIBRATION_DISPLAY
    const uint32_t flow_status_page = (now_ms / 2000u) % 5u;
#else
    const uint32_t flow_status_page = (now_ms / 2000u) % 4u;
#endif
    if (show_usb_status) {
        snprintf(line1, sizeof(line1), "USB: %-11s", usb_label);
    } else if ((int32_t)(mode_announce_until_ms - now_ms) > 0) {
        snprintf(line1, sizeof(line1), ">%-15s", lv_viz_mode_name(mode));
    } else if (mode == lv_viz_mode_t::FLOW &&
               flow_status_page == 1u) {
#if AUDIO_CALIBRATION_DISPLAY
        const float *rms = input_diagnostic.published
                               ? input_diagnostic.displayed_rms
                               : input_diagnostic.rms;
        snprintf(line1, sizeof(line1), "RMS L%3d R%3d",
                 lv_level_dbfs(rms[0]), lv_level_dbfs(rms[1]));
#else
        const uint32_t missed_audio_windows = adc_buffers.dropped;
        snprintf(line1, sizeof(line1), "A%03lu R%03lu M%03lu  ",
                 (unsigned long)tenths_of_ms(
                     runtime_metrics.max_analysis_us),
                 (unsigned long)tenths_of_ms(
                     runtime_metrics.max_render_us),
                 (unsigned long)std::min<uint32_t>(missed_audio_windows,
                                                   999));
#endif
    } else if (mode == lv_viz_mode_t::FLOW &&
               flow_status_page == 2u) {
#if AUDIO_CALIBRATION_DISPLAY
        const float *peak = input_diagnostic.published
                                ? input_diagnostic.displayed_peak
                                : input_diagnostic.peak;
        snprintf(line1, sizeof(line1), "PEAK L%3d R%3d",
                 lv_level_dbfs(peak[0]), lv_level_dbfs(peak[1]));
#else
        snprintf(line1, sizeof(line1), "F%03lu E%03lu       ",
                 (unsigned long)tenths_of_ms(
                     runtime_metrics.max_spectrum_us),
                 (unsigned long)tenths_of_ms(
                     runtime_metrics.max_features_us));
#endif
    } else if (mode == lv_viz_mode_t::FLOW &&
               flow_status_page == 3u) {
#if AUDIO_CALIBRATION_DISPLAY
        const uint32_t gain_tenths = (uint32_t)std::clamp(
            std::lround(flow_normalizer.gain * 10.0f), 0l, 999l);
        snprintf(line1, sizeof(line1), "GAIN %lu.%lu N%3d%c",
                 (unsigned long)(gain_tenths / 10u),
                 (unsigned long)(gain_tenths % 10u),
                 lv_level_dbfs(flow_normalizer.noise_floor /
                               AUDIO_INPUT_GAIN),
                 flow_normalizer.gate_open ? 'O' : 'C');
#else
        const lv_usb_bridge_metrics_t usb = usb_bridge_metrics();
        snprintf(line1, sizeof(line1), "D%03lu H%03lu K%03lu  ",
                 (unsigned long)tenths_of_ms(
                     usb.max_device_task_gap_us),
                 (unsigned long)tenths_of_ms(
                     usb.max_host_task_gap_us),
                 (unsigned long)tenths_of_ms(
                     usb.max_keyboard_forward_us));
#endif
#if AUDIO_CALIBRATION_DISPLAY
    } else if (mode == lv_viz_mode_t::FLOW &&
               flow_status_page == 4u) {
        const uint32_t bass = (uint32_t)std::clamp(
            std::lround(input_diagnostic.displayed_bass * 1000.0f),
            0l, 999l);
        const uint32_t body = (uint32_t)std::clamp(
            std::lround(input_diagnostic.displayed_body * 1000.0f),
            0l, 999l);
        const uint32_t missed_audio_windows = adc_buffers.dropped;
        snprintf(line1, sizeof(line1), "B%03lu>%03lu M%03lu",
                 (unsigned long)bass, (unsigned long)body,
                 (unsigned long)std::min<uint32_t>(missed_audio_windows,
                                                   999));
#endif
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

    // Latch calibration only when the external mode button remains held
    // throughout the splash. This makes startup intent independent of one
    // instantaneous GPIO read.
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
    sleep_ms(2);
    const bool calibration_pressed_at_start = !gpio_get(BUTTON_PIN);

    usb_bridge_init();
    lv_flow_v3_engine_init(&flow_v3);

    // LCD
    lcd_init();
    lcd_load_vu_chars();
    lcd_set_cursor(0, 0); lcd_print(BOOT_LINE0);
    lcd_set_cursor(0, 1); lcd_print(BOOT_LINE1);
    lcd_flush();
    sleep_ms(1200);
    lcd_cmd(0x01); sleep_ms(2);
    if (calibration_pressed_at_start && !gpio_get(BUTTON_PIN)) {
        input_calibration_state =
            input_calibration_state_t::WAIT_RELEASE;
    }

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
        if (!poll_input_calibration(now)) poll_button();

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
#ifdef LANDERVIZ_POWER_TEST
            lv_power_test_render(&power_test, frame);
#else
            if (mode == lv_viz_mode_t::FLOW && audio_present) {
                lv_flow_v3_engine_render(
                    &flow_v3, now, frame, usb_bridge_device_task);
            } else {
                lv_visualizer_render(&visualizer, mode, audio,
                                     audio_present, now, frame);
            }
#endif
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
            lcd_flush();
        }
    }
}
