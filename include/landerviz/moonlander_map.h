#pragma once

#include <stdint.h>

// Oryx layout positions (0..71) to QMK RGB-matrix LED indices. This is the
// physical ordering used by layout z9wlG / revision ARDpK.
static constexpr uint8_t LV_ORYX_TO_LED[72] = {
    0,  5,  10, 15, 20, 25, 29, 1,  6,  11, 16, 21, 26, 30, 2,  7,  12, 17,
    22, 27, 31, 3,  8,  13, 18, 23, 28, 4,  9,  14, 19, 24, 35, 32, 33, 34,
    65, 61, 56, 51, 46, 41, 36, 66, 62, 57, 52, 47, 42, 37, 67, 63, 58, 53,
    48, 43, 38, 64, 59, 54, 49, 44, 39, 60, 55, 50, 45, 40, 71, 70, 69, 68,
};

static constexpr uint8_t LV_NO_LED = 0xFF;

// Five spectrum levels, from the number row down to the modifier row. The
// innermost column has no modifier-row key. Columns run left-to-right as the
// user sees them.
static constexpr uint8_t LV_SPECTRUM_GRID_LED[2][5][6] = {
    {
        {0, 5, 10, 15, 20, 25},
        {1, 6, 11, 16, 21, 26},
        {2, 7, 12, 17, 22, 27},
        {3, 8, 13, 18, 23, 28},
        {4, 9, 14, 19, 24, LV_NO_LED},
    },
    {
        {61, 56, 51, 46, 41, 36},
        {62, 57, 52, 47, 42, 37},
        {63, 58, 53, 48, 43, 38},
        {64, 59, 54, 49, 44, 39},
        {60, 55, 50, 45, 40, LV_NO_LED},
    },
};

static constexpr uint8_t LV_UTILITY_LED[2][3] = {
    {29, 30, 31},
    {65, 66, 67},
};

static constexpr uint8_t LV_THUMB_LED[2][4] = {
    {35, 32, 33, 34},
    {71, 70, 69, 68},
};

static constexpr int lv_key_at(int half, int col, int row) {
    if (half < 0 || half >= 2 || col < 0 || col >= 6 || row < 0 || row >= 5) {
        return -1;
    }
    const uint8_t led = LV_SPECTRUM_GRID_LED[half][row][col];
    return led == LV_NO_LED ? -1 : led;
}

static constexpr int lv_utility_key_at(int half, int key) {
    return half >= 0 && half < 2 && key >= 0 && key < 3
               ? LV_UTILITY_LED[half][key]
               : -1;
}

static constexpr int lv_thumb_key_at(int half, int key) {
    return half >= 0 && half < 2 && key >= 0 && key < 4
               ? LV_THUMB_LED[half][key]
               : -1;
}
