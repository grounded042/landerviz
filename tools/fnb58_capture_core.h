#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace landerviz::fnb58 {

constexpr std::size_t kReportSize = 64;
constexpr std::size_t kSamplesPerReport = 4;

struct Sample {
  double voltage_v = 0.0;
  double current_a = 0.0;
  double power_w = 0.0;
};

inline std::uint16_t read_u16_le(const std::uint8_t *bytes) {
  return static_cast<std::uint16_t>(bytes[0]) |
         (static_cast<std::uint16_t>(bytes[1]) << 8U);
}

inline std::uint32_t read_u32_le(const std::uint8_t *bytes) {
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1]) << 8U) |
         (static_cast<std::uint32_t>(bytes[2]) << 16U) |
         (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

inline bool decode_report(const std::uint8_t *report, std::size_t size,
                          std::array<Sample, kSamplesPerReport> &samples) {
  if (size != kReportSize || report[0] != 0xaaU || report[1] != 0x04U) {
    return false;
  }

  for (std::size_t index = 0; index < samples.size(); ++index) {
    const auto *data = report + 2U + (index * 15U);
    const double voltage = static_cast<double>(read_u32_le(data)) / 100000.0;
    const double current =
        static_cast<double>(read_u32_le(data + 4U)) / 100000.0;
    samples[index] = Sample{voltage, current, voltage * current};
  }
  return true;
}

class Summary {
public:
  void add(const Sample &sample) {
    ++count_;
    voltage_sum_ += sample.voltage_v;
    current_sum_ += sample.current_a;
    power_sum_ += sample.power_w;
    if (sample.voltage_v < voltage_min_) {
      voltage_min_ = sample.voltage_v;
    }
    if (sample.voltage_v > voltage_max_) {
      voltage_max_ = sample.voltage_v;
    }
    if (sample.current_a > current_max_) {
      current_max_ = sample.current_a;
    }
    if (sample.power_w > power_max_) {
      power_max_ = sample.power_w;
    }
  }

  std::size_t count() const { return count_; }
  double voltage_min_v() const { return count_ == 0U ? 0.0 : voltage_min_; }
  double voltage_average_v() const {
    return count_ == 0U ? 0.0 : voltage_sum_ / static_cast<double>(count_);
  }
  double voltage_max_v() const { return count_ == 0U ? 0.0 : voltage_max_; }
  double current_average_a() const {
    return count_ == 0U ? 0.0 : current_sum_ / static_cast<double>(count_);
  }
  double current_max_a() const { return count_ == 0U ? 0.0 : current_max_; }
  double power_average_w() const {
    return count_ == 0U ? 0.0 : power_sum_ / static_cast<double>(count_);
  }
  double power_max_w() const { return count_ == 0U ? 0.0 : power_max_; }

private:
  std::size_t count_ = 0U;
  double voltage_sum_ = 0.0;
  double current_sum_ = 0.0;
  double power_sum_ = 0.0;
  double voltage_min_ = std::numeric_limits<double>::infinity();
  double voltage_max_ = 0.0;
  double current_max_ = 0.0;
  double power_max_ = 0.0;
};

} // namespace landerviz::fnb58
