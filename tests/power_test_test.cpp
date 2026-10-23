#include "landerviz/power_test.h"

#include <cassert>
#include <cstddef>

int main() {
  lv_power_test_t test{};
  assert(lv_power_test_level(&test) == 0U);

  constexpr unsigned char expected[] = {64U,  128U, 175U, 200U,
                                        225U, 255U, 255U};
  for (const auto level : expected) {
    lv_power_test_next(&test);
    assert(lv_power_test_level(&test) == level);
  }

  lv_power_test_previous(&test);
  assert(lv_power_test_level(&test) == 225U);
  for (std::size_t index = 0; index < 10U; ++index) {
    lv_power_test_previous(&test);
  }
  assert(lv_power_test_level(&test) == 0U);

  lv_rgb_t frame[LV_KEY_COUNT]{};
  lv_power_test_next(&test);
  lv_power_test_render(&test, frame);
  for (const auto &color : frame) {
    assert(color.r == 64U);
    assert(color.g == 64U);
    assert(color.b == 64U);
  }
}
