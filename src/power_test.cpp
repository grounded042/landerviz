#include "landerviz/power_test.h"

#include <algorithm>
#include <cstddef>

namespace {
constexpr uint8_t kLevels[] = {0, 64, 128, 175, 200, 225, 255};
constexpr std::size_t kLevelCount = sizeof(kLevels) / sizeof(kLevels[0]);
} // namespace

uint8_t lv_power_test_level(const lv_power_test_t *test) {
  const std::size_t index =
      std::min<std::size_t>(test->level_index, kLevelCount - 1U);
  return kLevels[index];
}

void lv_power_test_next(lv_power_test_t *test) {
  if (test->level_index + 1U < kLevelCount)
    ++test->level_index;
}

void lv_power_test_previous(lv_power_test_t *test) {
  if (test->level_index > 0U)
    --test->level_index;
}

void lv_power_test_render(const lv_power_test_t *test,
                          lv_rgb_t frame[LV_KEY_COUNT]) {
  const uint8_t level = lv_power_test_level(test);
  for (std::size_t key = 0; key < LV_KEY_COUNT; ++key) {
    frame[key] = {level, level, level};
  }
}
