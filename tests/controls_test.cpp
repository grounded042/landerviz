#include <cassert>

#include "landerviz/controls.h"

static void test_button_debounces_short_press_on_release() {
    lv_button_t button{};
    lv_button_init(&button, false, 0);

    assert(lv_button_update(&button, true, 10) ==
           lv_button_event_t::NONE);
    assert(lv_button_update(&button, true, 39) ==
           lv_button_event_t::NONE);
    assert(lv_button_update(&button, true, 40) ==
           lv_button_event_t::NONE);
    assert(lv_button_update(&button, false, 100) ==
           lv_button_event_t::NONE);
    assert(lv_button_update(&button, false, 129) ==
           lv_button_event_t::NONE);
    assert(lv_button_update(&button, false, 130) ==
           lv_button_event_t::SHORT_PRESS);
}

static void test_button_fires_long_press_once_without_short_press() {
    lv_button_t button{};
    lv_button_init(&button, false, 0);

    lv_button_update(&button, true, 10);
    lv_button_update(&button, true, 40);
    assert(lv_button_update(&button, true, 840) ==
           lv_button_event_t::NONE);
    assert(lv_button_update(&button, true, 841) ==
           lv_button_event_t::LONG_PRESS);
    assert(lv_button_update(&button, true, 900) ==
           lv_button_event_t::NONE);
    lv_button_update(&button, false, 910);
    assert(lv_button_update(&button, false, 940) ==
           lv_button_event_t::NONE);
}

static void test_home_rotation_wraps_visualizations() {
    lv_controls_t controls{};
    lv_controls_init(&controls);

    lv_controls_update(&controls, -1, lv_button_event_t::NONE);
    assert(controls.mode == lv_viz_mode_t::IDLE);
    lv_controls_update(&controls, 2, lv_button_event_t::NONE);
    assert(controls.mode == lv_viz_mode_t::SPECTRUM);
}

static void test_short_press_cycles_scheme_per_visualization() {
    lv_controls_t controls{};
    lv_controls_init(&controls);

    lv_controls_update(&controls, 0, lv_button_event_t::SHORT_PRESS);
    assert(lv_controls_color_scheme(&controls) == 1);

    lv_controls_update(&controls, 1, lv_button_event_t::NONE);
    assert(controls.mode == lv_viz_mode_t::SPECTRUM);
    assert(lv_controls_color_scheme(&controls) == 0);
    lv_controls_update(&controls, 0, lv_button_event_t::SHORT_PRESS);
    assert(lv_controls_color_scheme(&controls) == 1);

    lv_controls_update(&controls, -1, lv_button_event_t::NONE);
    assert(controls.mode == lv_viz_mode_t::FLOW);
    assert(lv_controls_color_scheme(&controls) == 1);
}

static void test_menu_and_diagnostic_navigation() {
    lv_controls_t controls{};
    lv_controls_init(&controls);

    lv_controls_update(&controls, 0, lv_button_event_t::LONG_PRESS);
    assert(controls.screen == lv_control_screen_t::MENU);
    assert(controls.menu_item == lv_menu_item_t::DIAGNOSTICS);

    lv_controls_update(&controls, 0, lv_button_event_t::SHORT_PRESS);
    assert(controls.screen == lv_control_screen_t::DIAGNOSTICS);
    assert(controls.diagnostic_page == lv_diagnostic_page_t::AUDIO);

    lv_controls_update(&controls, -1, lv_button_event_t::NONE);
    assert(controls.diagnostic_page == lv_diagnostic_page_t::USB);
    lv_controls_update(&controls, 0, lv_button_event_t::SHORT_PRESS);
    assert(controls.screen == lv_control_screen_t::MENU);

    lv_controls_update(&controls, 0, lv_button_event_t::LONG_PRESS);
    assert(controls.screen == lv_control_screen_t::HOME);
}

static void test_calibration_menu_item_emits_action_and_returns_home() {
    lv_controls_t controls{};
    lv_controls_init(&controls);
    lv_controls_update(&controls, 0, lv_button_event_t::LONG_PRESS);
    lv_controls_update(&controls, 1, lv_button_event_t::NONE);
    assert(controls.menu_item == lv_menu_item_t::CALIBRATE_INPUT);

    const lv_control_result_t result = lv_controls_update(
        &controls, 0, lv_button_event_t::SHORT_PRESS);
    assert(result.start_input_calibration);
    assert(controls.screen == lv_control_screen_t::HOME);
}

static void test_menu_exit_item_returns_home() {
    lv_controls_t controls{};
    lv_controls_init(&controls);
    lv_controls_update(&controls, 0, lv_button_event_t::LONG_PRESS);
    lv_controls_update(&controls, 2, lv_button_event_t::NONE);
    assert(controls.menu_item == lv_menu_item_t::EXIT);

    const lv_control_result_t result = lv_controls_update(
        &controls, 0, lv_button_event_t::SHORT_PRESS);
    assert(!result.start_input_calibration);
    assert(controls.screen == lv_control_screen_t::HOME);
}

int main() {
    test_button_debounces_short_press_on_release();
    test_button_fires_long_press_once_without_short_press();
    test_home_rotation_wraps_visualizations();
    test_short_press_cycles_scheme_per_visualization();
    test_menu_and_diagnostic_navigation();
    test_calibration_menu_item_emits_action_and_returns_home();
    test_menu_exit_item_returns_home();
    return 0;
}
