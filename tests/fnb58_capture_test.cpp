#include "../tools/fnb58_capture_core.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace {

using landerviz::fnb58::kReportSize;
using landerviz::fnb58::Sample;
using landerviz::fnb58::Summary;

bool near(double actual, double expected) {
  return std::fabs(actual - expected) < 0.000001;
}

void write_u32_le(std::uint8_t *bytes, std::uint32_t value) {
  bytes[0] = static_cast<std::uint8_t>(value);
  bytes[1] = static_cast<std::uint8_t>(value >> 8U);
  bytes[2] = static_cast<std::uint8_t>(value >> 16U);
  bytes[3] = static_cast<std::uint8_t>(value >> 24U);
}

void decode_report_reads_all_four_samples() {
  std::array<std::uint8_t, kReportSize> report{};
  report[0] = 0xaaU;
  report[1] = 0x04U;
  for (std::size_t index = 0; index < 4U; ++index) {
    auto *sample = report.data() + 2U + index * 15U;
    write_u32_le(sample, 500000U + static_cast<std::uint32_t>(index * 1000U));
    write_u32_le(sample + 4U,
                 25000U + static_cast<std::uint32_t>(index * 1000U));
  }

  std::array<Sample, 4> samples{};
  assert(
      landerviz::fnb58::decode_report(report.data(), report.size(), samples));
  assert(near(samples[0].voltage_v, 5.0));
  assert(near(samples[0].current_a, 0.25));
  assert(near(samples[0].power_w, 1.25));
  assert(near(samples[3].voltage_v, 5.03));
  assert(near(samples[3].current_a, 0.28));
  assert(near(samples[3].power_w, 1.4084));
}

void decode_report_rejects_wrong_size_or_header() {
  std::array<std::uint8_t, kReportSize> report{};
  std::array<Sample, 4> samples{};
  report[0] = 0xaaU;
  report[1] = 0x04U;
  assert(!landerviz::fnb58::decode_report(report.data(), report.size() - 1U,
                                          samples));
  report[1] = 0x03U;
  assert(
      !landerviz::fnb58::decode_report(report.data(), report.size(), samples));
}

void summary_tracks_extrema_and_sample_weighted_averages() {
  Summary summary;
  assert(summary.count() == 0U);
  assert(near(summary.voltage_min_v(), 0.0));
  summary.add(Sample{5.2, 0.2, 1.04});
  summary.add(Sample{4.9, 0.5, 2.45});
  summary.add(Sample{5.1, 0.3, 1.53});

  assert(summary.count() == 3U);
  assert(near(summary.voltage_min_v(), 4.9));
  assert(near(summary.voltage_average_v(), 5.066666666666667));
  assert(near(summary.voltage_max_v(), 5.2));
  assert(near(summary.current_average_a(), 1.0 / 3.0));
  assert(near(summary.current_max_a(), 0.5));
  assert(near(summary.power_average_w(), 5.02 / 3.0));
  assert(near(summary.power_max_w(), 2.45));
}

} // namespace

int main() {
  decode_report_reads_all_four_samples();
  decode_report_rejects_wrong_size_or_header();
  summary_tracks_extrema_and_sample_weighted_averages();
  return 0;
}
