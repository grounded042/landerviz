#include <array>
#include <cassert>
#include <cstdint>

#include "landerviz/pcf8574_lcd.h"

static void test_nibble_mapping() {
    assert(lv_pcf8574_encode_nibble(0x0A, false, false) == 0xA8);
    assert(lv_pcf8574_encode_nibble(0x05, true, true) == 0x5D);
}

static void test_byte_sequence_has_setup_enable_and_hold_states() {
    const std::array<uint8_t, 6> encoded =
        lv_pcf8574_encode_byte(0xAB, true);
    const std::array<uint8_t, 6> expected = {
        0xA9, 0xAD, 0xA9,
        0xB9, 0xBD, 0xB9,
    };
    assert(encoded == expected);
}

static void test_probe_candidates_cover_common_backpacks() {
    assert(LV_PCF8574_ADDRESS_CANDIDATES.size() == 2);
    assert(LV_PCF8574_ADDRESS_CANDIDATES[0] == 0x27);
    assert(LV_PCF8574_ADDRESS_CANDIDATES[1] == 0x3F);
}

static void test_dirty_runs_do_not_cross_rows() {
    std::array<uint8_t, LV_LCD_CELL_COUNT> desired{};
    std::array<uint8_t, LV_LCD_CELL_COUNT> shadow{};
    desired.fill(' ');
    shadow.fill(' ');

    desired[3] = 'A';
    desired[4] = 'B';
    desired[7] = 'C';
    desired[15] = 'D';
    desired[16] = 'E';

    lv_lcd_dirty_run_t run{};
    assert(lv_lcd_next_dirty_run(desired, shadow, 0, &run));
    assert(run.begin == 3);
    assert(run.end == 5);

    assert(lv_lcd_next_dirty_run(desired, shadow, run.end, &run));
    assert(run.begin == 7);
    assert(run.end == 8);

    assert(lv_lcd_next_dirty_run(desired, shadow, run.end, &run));
    assert(run.begin == 15);
    assert(run.end == 16);

    assert(lv_lcd_next_dirty_run(desired, shadow, run.end, &run));
    assert(run.begin == 16);
    assert(run.end == 17);
    assert(!lv_lcd_next_dirty_run(desired, desired, run.end, &run));
}

int main() {
    test_nibble_mapping();
    test_byte_sequence_has_setup_enable_and_hold_states();
    test_probe_candidates_cover_common_backpacks();
    test_dirty_runs_do_not_cross_rows();
    return 0;
}
