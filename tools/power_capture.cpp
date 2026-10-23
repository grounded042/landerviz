#include "fnb58_capture_core.h"

#include <hidapi.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

using Clock = std::chrono::steady_clock;
using landerviz::fnb58::Sample;
using landerviz::fnb58::Summary;

constexpr unsigned short kVendorId = 0x2e3cU;
constexpr unsigned short kProductId = 0x5558U;
constexpr int kMeasurementInterface = 3;

// The measurement-only command and report format is documented by OpenFNB58:
// https://github.com/yeckel/OpenFNB58. This tool intentionally implements no
// fast-charge or voltage-trigger commands.
constexpr std::array<std::uint8_t, 64> make_command(std::uint8_t command,
                                                    std::uint8_t checksum) {
  std::array<std::uint8_t, 64> result{};
  result[0] = 0xaaU;
  result[1] = command;
  result[63] = checksum;
  return result;
}

constexpr auto kInit1 = make_command(0x81U, 0x8eU);
constexpr auto kInit2 = make_command(0x82U, 0x96U);
constexpr auto kPoll = make_command(0x83U, 0x9eU);

struct Options {
  double duration_seconds = 30.0;
  std::string csv_path;
};

void print_usage(const char *program) {
  std::cout << "Usage: " << program << " [--duration SECONDS] [--csv FILE]\n"
            << "\n"
            << "Capture FNIRSI FNB58 USB measurements and report extrema.\n"
            << "The default capture duration is 30 seconds.\n";
}

Options parse_options(int argc, char **argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--help" || argument == "-h") {
      print_usage(argv[0]);
      std::exit(0);
    }
    if (argument == "--duration") {
      if (++index >= argc) {
        throw std::runtime_error("--duration requires a value");
      }
      options.duration_seconds = std::stod(argv[index]);
      if (!std::isfinite(options.duration_seconds) ||
          options.duration_seconds <= 0.0) {
        throw std::runtime_error("--duration must be greater than zero");
      }
      continue;
    }
    if (argument == "--csv") {
      if (++index >= argc) {
        throw std::runtime_error("--csv requires a file path");
      }
      options.csv_path = argv[index];
      continue;
    }
    throw std::runtime_error("unknown argument: " + argument);
  }
  return options;
}

class HidApi {
public:
  HidApi() {
    if (hid_init() != 0) {
      throw std::runtime_error("could not initialize hidapi");
    }
  }
  HidApi(const HidApi &) = delete;
  HidApi &operator=(const HidApi &) = delete;
  ~HidApi() { hid_exit(); }
};

class HidDevice {
public:
  explicit HidDevice(hid_device *device) : device_(device) {}
  HidDevice(const HidDevice &) = delete;
  HidDevice &operator=(const HidDevice &) = delete;
  ~HidDevice() {
    if (device_ != nullptr) {
      hid_close(device_);
    }
  }
  hid_device *get() const { return device_; }

private:
  hid_device *device_;
};

HidDevice open_meter() {
  hid_device_info *devices = hid_enumerate(kVendorId, kProductId);
  const hid_device_info *selected = nullptr;
  for (const hid_device_info *device = devices; device != nullptr;
       device = device->next) {
    if (selected == nullptr ||
        device->interface_number == kMeasurementInterface) {
      selected = device;
    }
    if (device->interface_number == kMeasurementInterface) {
      break;
    }
  }

  hid_device *handle = nullptr;
  if (selected != nullptr) {
    handle = hid_open_path(selected->path);
  }
  hid_free_enumeration(devices);
  if (handle == nullptr) {
    throw std::runtime_error(
        "FNB58 USB interface not found (VID 2e3c, PID 5558); check that "
        "the meter's PC port and cable both carry data");
  }
  return HidDevice(handle);
}

bool send_command(hid_device *device,
                  const std::array<std::uint8_t, 64> &command) {
  std::array<std::uint8_t, 65> report{};
  for (std::size_t index = 0; index < command.size(); ++index) {
    report[index + 1U] = command[index];
  }
  for (int attempt = 0; attempt < 5; ++attempt) {
    if (hid_write(device, report.data(), report.size()) > 0) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
  }
  return false;
}

const std::uint8_t *report_data(const std::array<unsigned char, 65> &buffer,
                                int size) {
  if (size == 64) {
    return buffer.data();
  }
  if (size == 65 && buffer[0] == 0U) {
    return buffer.data() + 1U;
  }
  return nullptr;
}

bool wait_for_measurement(hid_device *device, int timeout_ms) {
  const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
  std::array<unsigned char, 65> buffer{};
  while (Clock::now() < deadline) {
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline -
                                                              Clock::now())
            .count();
    const int bytes_read =
        hid_read_timeout(device, buffer.data(), buffer.size(),
                         static_cast<int>(remaining > 200 ? 200 : remaining));
    if (bytes_read < 0) {
      return false;
    }
    const auto *report = report_data(buffer, bytes_read);
    if (report == nullptr) {
      continue;
    }
    std::array<Sample, landerviz::fnb58::kSamplesPerReport> samples{};
    if (landerviz::fnb58::decode_report(report, landerviz::fnb58::kReportSize,
                                        samples)) {
      return true;
    }
  }
  return false;
}

void initialize_meter(hid_device *device) {
  // A previous capture normally leaves the FNB58 streaming. Reuse that stream
  // before sending commands: INIT1 may reboot some firmware revisions and
  // invalidate the HID handle that issued it.
  if (wait_for_measurement(device, 750)) {
    return;
  }

  if (send_command(device, kPoll) && wait_for_measurement(device, 1500)) {
    return;
  }

  if (send_command(device, kInit2)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (send_command(device, kInit2) && wait_for_measurement(device, 4000)) {
      return;
    }
  }

  if (!send_command(device, kInit1)) {
    throw std::runtime_error("FNB58 did not accept the initialization command");
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  if (!send_command(device, kInit2)) {
    throw std::runtime_error("FNB58 did not accept the stream command");
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  if (!send_command(device, kInit2) || !wait_for_measurement(device, 4000)) {
    throw std::runtime_error(
        "FNB58 did not start streaming; reconnect only its PC cable and retry");
  }
}

void print_summary(const Summary &summary, double elapsed_seconds) {
  std::cout << std::fixed << std::setprecision(5)
            << "duration:       " << std::setprecision(2) << elapsed_seconds
            << " s\n"
            << "samples:        " << summary.count() << "\n"
            << "sample rate:    " << std::setprecision(1)
            << static_cast<double>(summary.count()) / elapsed_seconds << " /s\n"
            << "voltage min:    " << std::setprecision(5)
            << summary.voltage_min_v() << " V\n"
            << "voltage avg:    " << summary.voltage_average_v() << " V\n"
            << "voltage max:    " << summary.voltage_max_v() << " V\n"
            << "current avg:    " << summary.current_average_a() << " A\n"
            << "current max:    " << summary.current_max_a() << " A\n"
            << "power avg:      " << summary.power_average_w() << " W\n"
            << "power max:      " << summary.power_max_w() << " W\n";
}

int capture(const Options &options) {
  HidApi api;
  auto meter = open_meter();
  initialize_meter(meter.get());

  std::ofstream csv;
  if (!options.csv_path.empty()) {
    csv.open(options.csv_path);
    if (!csv) {
      throw std::runtime_error("could not open CSV output: " +
                               options.csv_path);
    }
    csv << "elapsed_s,voltage_v,current_a,power_w\n";
    csv << std::fixed << std::setprecision(6);
  }

  std::cout << "FNB58 connected; capturing for " << options.duration_seconds
            << " seconds...\n";
  std::array<unsigned char, 65> buffer{};
  Summary summary;
  const auto start = Clock::now();
  auto next_poll = start;
  auto last_sample = start;

  while (std::chrono::duration<double>(Clock::now() - start).count() <
         options.duration_seconds) {
    const auto now = Clock::now();
    if (now >= next_poll) {
      if (!send_command(meter.get(), kPoll)) {
        throw std::runtime_error("could not poll FNB58");
      }
      next_poll = now + std::chrono::milliseconds(900);
    }

    const int bytes_read =
        hid_read_timeout(meter.get(), buffer.data(), buffer.size(), 200);
    if (bytes_read < 0) {
      throw std::runtime_error("FNB58 disconnected during capture");
    }
    const auto *report = report_data(buffer, bytes_read);
    if (report == nullptr) {
      if (std::chrono::duration<double>(Clock::now() - last_sample).count() >
          5.0) {
        throw std::runtime_error(
            "no measurement reports received for 5 seconds");
      }
      continue;
    }

    std::array<Sample, landerviz::fnb58::kSamplesPerReport> samples{};
    if (!landerviz::fnb58::decode_report(report, landerviz::fnb58::kReportSize,
                                         samples)) {
      continue;
    }
    last_sample = Clock::now();
    const double elapsed =
        std::chrono::duration<double>(last_sample - start).count();
    for (const auto &sample : samples) {
      summary.add(sample);
      if (csv) {
        csv << elapsed << ',' << sample.voltage_v << ',' << sample.current_a
            << ',' << sample.power_w << '\n';
      }
    }
  }

  const double elapsed =
      std::chrono::duration<double>(Clock::now() - start).count();
  if (summary.count() == 0U) {
    throw std::runtime_error(
        "capture completed without any measurement samples");
  }
  print_summary(summary, elapsed);
  if (csv) {
    std::cout << "csv:            " << options.csv_path << '\n';
  }
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  try {
    return capture(parse_options(argc, argv));
  } catch (const std::exception &error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
