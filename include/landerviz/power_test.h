#pragma once

#include <cstdint>

#include "landerviz/protocol.h"

struct lv_power_test_t {
  uint8_t level_index;
};

uint8_t lv_power_test_level(const lv_power_test_t *test);
void lv_power_test_next(lv_power_test_t *test);
void lv_power_test_previous(lv_power_test_t *test);
void lv_power_test_render(const lv_power_test_t *test,
                          lv_rgb_t frame[LV_KEY_COUNT]);
