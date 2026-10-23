#pragma once

#include <cstdint>

#include "landerviz/color_scheme.h"
#include "landerviz/visualizer.h"

enum class lv_button_event_t : uint8_t {
    NONE,
    SHORT_PRESS,
    LONG_PRESS,
};

struct lv_button_t {
    uint32_t candidate_since_ms;
    uint32_t pressed_since_ms;
    bool stable_pressed;
    bool candidate_pressed;
    bool long_fired;
};

void lv_button_init(lv_button_t *button, bool pressed, uint32_t now_ms);
lv_button_event_t lv_button_update(lv_button_t *button, bool pressed,
                                   uint32_t now_ms);

enum class lv_control_screen_t : uint8_t {
    HOME,
    MENU,
    DIAGNOSTICS,
};

enum class lv_menu_item_t : uint8_t {
    DIAGNOSTICS,
    CALIBRATE_INPUT,
    EXIT,
    COUNT,
};

enum class lv_diagnostic_page_t : uint8_t {
    AUDIO,
    GAIN,
    FLOW,
    PERFORMANCE,
    USB,
    COUNT,
};

struct lv_controls_t {
    lv_viz_mode_t mode;
    lv_control_screen_t screen;
    lv_menu_item_t menu_item;
    lv_diagnostic_page_t diagnostic_page;
    uint8_t color_scheme[(int)lv_viz_mode_t::COUNT];
};

struct lv_control_result_t {
    bool mode_changed;
    bool color_scheme_changed;
    bool screen_changed;
    bool start_input_calibration;
};

void lv_controls_init(lv_controls_t *controls);
lv_control_result_t lv_controls_update(lv_controls_t *controls,
                                       int rotation,
                                       lv_button_event_t button_event);
uint8_t lv_controls_color_scheme(const lv_controls_t *controls);
const char *lv_menu_item_name(lv_menu_item_t item);
const char *lv_diagnostic_page_name(lv_diagnostic_page_t page);
