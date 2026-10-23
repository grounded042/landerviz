#pragma once

#include <array>
#include <cstdint>

// Common PCF8574 HD44780 backpack wiring:
// P0=RS, P1=RW, P2=EN, P3=backlight, P4..P7=D4..D7.
constexpr uint8_t LV_PCF8574_RS = 1u << 0;
constexpr uint8_t LV_PCF8574_ENABLE = 1u << 2;
constexpr uint8_t LV_PCF8574_BACKLIGHT = 1u << 3;

constexpr std::array<uint8_t, 2> LV_PCF8574_ADDRESS_CANDIDATES = {
    0x27,
    0x3F,
};

constexpr uint8_t LV_LCD_COLUMNS = 16;
constexpr uint8_t LV_LCD_ROWS = 2;
constexpr uint8_t LV_LCD_CELL_COUNT = LV_LCD_COLUMNS * LV_LCD_ROWS;

struct lv_lcd_dirty_run_t {
    uint8_t begin;
    uint8_t end;
};

constexpr bool lv_lcd_next_dirty_run(
    const std::array<uint8_t, LV_LCD_CELL_COUNT> &desired,
    const std::array<uint8_t, LV_LCD_CELL_COUNT> &shadow,
    uint8_t from, lv_lcd_dirty_run_t *run) {
    uint8_t begin = from;
    while (begin < LV_LCD_CELL_COUNT && desired[begin] == shadow[begin]) {
        ++begin;
    }
    if (begin == LV_LCD_CELL_COUNT) return false;

    const uint8_t row_end = static_cast<uint8_t>(
        ((begin / LV_LCD_COLUMNS) + 1u) * LV_LCD_COLUMNS);
    uint8_t end = static_cast<uint8_t>(begin + 1u);
    while (end < row_end && desired[end] != shadow[end]) ++end;

    run->begin = begin;
    run->end = end;
    return true;
}

constexpr uint8_t lv_pcf8574_encode_nibble(uint8_t nibble, bool rs,
                                            bool enable) {
    return static_cast<uint8_t>(((nibble & 0x0Fu) << 4) |
                                (rs ? LV_PCF8574_RS : 0u) |
                                (enable ? LV_PCF8574_ENABLE : 0u) |
                                LV_PCF8574_BACKLIGHT);
}

constexpr std::array<uint8_t, 6> lv_pcf8574_encode_byte(uint8_t value,
                                                        bool rs) {
    const uint8_t high = static_cast<uint8_t>(value >> 4);
    const uint8_t low = static_cast<uint8_t>(value & 0x0F);
    return {
        lv_pcf8574_encode_nibble(high, rs, false),
        lv_pcf8574_encode_nibble(high, rs, true),
        lv_pcf8574_encode_nibble(high, rs, false),
        lv_pcf8574_encode_nibble(low, rs, false),
        lv_pcf8574_encode_nibble(low, rs, true),
        lv_pcf8574_encode_nibble(low, rs, false),
    };
}
