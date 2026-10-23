#include "landerviz/controls.h"

#include <cstring>

namespace {
constexpr uint32_t debounce_ms = 30;
constexpr uint32_t long_press_ms = 800;

int wrap(int value, int count) {
    value %= count;
    return value < 0 ? value + count : value;
}
}  // namespace

void lv_button_init(lv_button_t *button, bool pressed, uint32_t now_ms) {
    std::memset(button, 0, sizeof(*button));
    button->stable_pressed = pressed;
    button->candidate_pressed = pressed;
    button->candidate_since_ms = now_ms;
    button->pressed_since_ms = now_ms;
}

lv_button_event_t lv_button_update(lv_button_t *button, bool pressed,
                                   uint32_t now_ms) {
    if (pressed != button->candidate_pressed) {
        button->candidate_pressed = pressed;
        button->candidate_since_ms = now_ms;
    }

    if (button->candidate_pressed != button->stable_pressed &&
        now_ms - button->candidate_since_ms >= debounce_ms) {
        button->stable_pressed = button->candidate_pressed;
        if (button->stable_pressed) {
            button->pressed_since_ms = now_ms;
            button->long_fired = false;
        } else if (!button->long_fired) {
            return lv_button_event_t::SHORT_PRESS;
        }
    }

    if (button->stable_pressed && !button->long_fired &&
        now_ms - button->pressed_since_ms > long_press_ms) {
        button->long_fired = true;
        return lv_button_event_t::LONG_PRESS;
    }
    return lv_button_event_t::NONE;
}

void lv_controls_init(lv_controls_t *controls) {
    std::memset(controls, 0, sizeof(*controls));
    controls->mode = lv_viz_mode_t::FLOW;
    controls->screen = lv_control_screen_t::HOME;
    controls->menu_item = lv_menu_item_t::DIAGNOSTICS;
    controls->diagnostic_page = lv_diagnostic_page_t::AUDIO;
}

lv_control_result_t lv_controls_update(lv_controls_t *controls,
                                       int rotation,
                                       lv_button_event_t button_event) {
    lv_control_result_t result{};

    if (rotation != 0) {
        switch (controls->screen) {
            case lv_control_screen_t::HOME:
                controls->mode = static_cast<lv_viz_mode_t>(wrap(
                    (int)controls->mode + rotation,
                    (int)lv_viz_mode_t::COUNT));
                result.mode_changed = true;
                break;
            case lv_control_screen_t::MENU:
                controls->menu_item = static_cast<lv_menu_item_t>(wrap(
                    (int)controls->menu_item + rotation,
                    (int)lv_menu_item_t::COUNT));
                break;
            case lv_control_screen_t::DIAGNOSTICS:
                controls->diagnostic_page =
                    static_cast<lv_diagnostic_page_t>(wrap(
                        (int)controls->diagnostic_page + rotation,
                        (int)lv_diagnostic_page_t::COUNT));
                break;
        }
    }

    if (button_event == lv_button_event_t::NONE) return result;

    if (button_event == lv_button_event_t::LONG_PRESS) {
        if (controls->screen == lv_control_screen_t::HOME) {
            controls->screen = lv_control_screen_t::MENU;
        } else {
            controls->screen = lv_control_screen_t::HOME;
        }
        result.screen_changed = true;
        return result;
    }

    switch (controls->screen) {
        case lv_control_screen_t::HOME: {
            uint8_t &scheme =
                controls->color_scheme[(int)controls->mode];
            scheme = (uint8_t)((scheme + 1) % LV_COLOR_SCHEME_COUNT);
            result.color_scheme_changed = true;
            break;
        }
        case lv_control_screen_t::MENU:
            switch (controls->menu_item) {
                case lv_menu_item_t::DIAGNOSTICS:
                    controls->screen = lv_control_screen_t::DIAGNOSTICS;
                    break;
                case lv_menu_item_t::CALIBRATE_INPUT:
                    controls->screen = lv_control_screen_t::HOME;
                    result.start_input_calibration = true;
                    break;
                case lv_menu_item_t::EXIT:
                    controls->screen = lv_control_screen_t::HOME;
                    break;
                case lv_menu_item_t::COUNT:
                    break;
            }
            result.screen_changed = true;
            break;
        case lv_control_screen_t::DIAGNOSTICS:
            controls->screen = lv_control_screen_t::MENU;
            result.screen_changed = true;
            break;
    }
    return result;
}

uint8_t lv_controls_color_scheme(const lv_controls_t *controls) {
    return controls->color_scheme[(int)controls->mode];
}

const char *lv_menu_item_name(lv_menu_item_t item) {
    static constexpr const char *names[] = {
        "DIAGNOSTICS", "CALIBRATE INPUT", "EXIT"};
    const int index = (int)item;
    return index >= 0 && index < (int)lv_menu_item_t::COUNT
               ? names[index]
               : "UNKNOWN";
}

const char *lv_diagnostic_page_name(lv_diagnostic_page_t page) {
    static constexpr const char *names[] = {
        "AUDIO", "GAIN", "FLOW", "PERFORMANCE", "USB"};
    const int index = (int)page;
    return index >= 0 && index < (int)lv_diagnostic_page_t::COUNT
               ? names[index]
               : "UNKNOWN";
}
