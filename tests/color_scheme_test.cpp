#include <cassert>

#include "landerviz/color_scheme.h"

static bool equal(lv_rgb_t left, lv_rgb_t right) {
    return left.r == right.r && left.g == right.g && left.b == right.b;
}

static void test_native_scheme_is_unchanged() {
    assert(equal(lv_color_scheme_apply({210, 40, 10}, 0),
                 {210, 40, 10}));
}

static void test_rotations_preserve_vividness_and_white() {
    assert(equal(lv_color_scheme_apply({255, 0, 0}, 1),
                 {0, 255, 0}));
    assert(equal(lv_color_scheme_apply({255, 0, 0}, 2),
                 {0, 0, 255}));
    assert(equal(lv_color_scheme_apply({255, 255, 255}, 1),
                 {255, 255, 255}));
}

static void test_out_of_range_scheme_wraps() {
    assert(equal(lv_color_scheme_apply({9, 8, 7}, LV_COLOR_SCHEME_COUNT),
                 {9, 8, 7}));
}

int main() {
    test_native_scheme_is_unchanged();
    test_rotations_preserve_vividness_and_white();
    test_out_of_range_scheme_wraps();
    return 0;
}
