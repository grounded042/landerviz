#include "landerviz/usb_bridge.h"

#include <cstring>

#include "hardware/clocks.h"
#include "pico/critical_section.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pio_usb.h"
#include "tusb.h"

extern "C" {
extern root_port_t pio_usb_root_port[PIO_USB_ROOT_PORT_CNT];
}

namespace {
constexpr uint8_t kKeyboardReportId = 1;
constexpr uint8_t kMouseReportId = 2;
constexpr uint8_t kSystemReportId = 3;
constexpr uint8_t kConsumerReportId = 4;
constexpr uint8_t kNkroReportId = 6;
// TinyUSB hcd_eventid_t::HCD_EVENT_DEVICE_ATTACH. Avoid including the private
// hcd.h from C++, because its inline C initializers fail our -Werror build.
constexpr uint32_t kHostAttachEvent = 0;
constexpr size_t kUpstreamQueueSize = 64;
constexpr size_t kUpstreamReportSize = 32;

struct UpstreamReport {
    uint8_t id;
    uint8_t length;
    uint8_t data[kUpstreamReportSize];
};

critical_section_t state_lock;
lv_rgb_t requested_frame[LV_KEY_COUNT]{};
lv_rgb_t sending_frame[LV_KEY_COUNT]{};
volatile uint32_t requested_generation = 0;
uint32_t sending_generation = 0;
uint8_t sending_frame_id = 0;
uint8_t sending_chunk = LV_CHUNK_COUNT;

volatile uint8_t raw_device = 0;
volatile uint8_t raw_instance = 0;
volatile bool raw_ready = false;
volatile uint8_t host_status = 0;
volatile uint8_t host_status_history = 0;

volatile uint8_t keyboard_device = 0;
volatile uint8_t keyboard_instance = 0;
uint8_t keyboard_output_queued = 0;
uint8_t keyboard_output_active = 0;
volatile bool keyboard_output_pending = false;
volatile bool keyboard_output_busy = false;

UpstreamReport upstream_queue[kUpstreamQueueSize]{};
size_t upstream_read = 0;
size_t upstream_write = 0;

void record_host_status(uint8_t flags) {
    host_status |= flags;
    host_status_history |= flags;
}

bool queue_upstream_report(uint8_t id, const uint8_t *data, uint16_t length) {
    if (length > kUpstreamReportSize) return false;

    bool queued = false;
    critical_section_enter_blocking(&state_lock);
    const size_t next = (upstream_write + 1) % kUpstreamQueueSize;
    if (next != upstream_read) {
        UpstreamReport &slot = upstream_queue[upstream_write];
        slot.id = id;
        slot.length = static_cast<uint8_t>(length);
        std::memcpy(slot.data, data, length);
        upstream_write = next;
        queued = true;
    }
    critical_section_exit(&state_lock);
    return queued;
}

void queue_upstream_releases() {
    static constexpr uint16_t kNkroPayloadSize = 31;
    const uint8_t released[kUpstreamReportSize]{};

    // Drop stale downstream reports before releasing every collection. This
    // prevents a key, mouse button, or media key from remaining logically held
    // on the Mac if the Moonlander is unplugged mid-report.
    critical_section_enter_blocking(&state_lock);
    upstream_read = 0;
    upstream_write = 0;
    critical_section_exit(&state_lock);

    queue_upstream_report(kKeyboardReportId, released, sizeof(hid_keyboard_report_t));
    queue_upstream_report(kMouseReportId, released, sizeof(hid_mouse_report_t));
    queue_upstream_report(kSystemReportId, released, 2);
    queue_upstream_report(kConsumerReportId, released, 2);
    queue_upstream_report(kNkroReportId, released, kNkroPayloadSize);
}

bool is_qmk_raw_hid(const uint8_t *descriptor, uint16_t length) {
    // QMK's default Raw HID collection uses Usage Page 0xFF60, Usage 0x61.
    static constexpr uint8_t usage[] = {0x06, 0x60, 0xFF, 0x09, 0x61};
    if (length < sizeof(usage)) return false;
    for (uint16_t i = 0; i <= length - sizeof(usage); ++i) {
        if (std::memcmp(descriptor + i, usage, sizeof(usage)) == 0) return true;
    }
    return false;
}

void begin_latest_frame_if_idle() {
    if (sending_chunk < LV_CHUNK_COUNT || sending_generation == requested_generation) return;

    critical_section_enter_blocking(&state_lock);
    std::memcpy(sending_frame, requested_frame, sizeof(sending_frame));
    sending_generation = requested_generation;
    critical_section_exit(&state_lock);
    ++sending_frame_id;
    sending_chunk = 0;
}

void service_keyboard_output() {
    if (!keyboard_device || keyboard_output_busy || !keyboard_output_pending) return;

    critical_section_enter_blocking(&state_lock);
    keyboard_output_active = keyboard_output_queued;
    keyboard_output_pending = false;
    keyboard_output_busy = true;
    critical_section_exit(&state_lock);

    if (!tuh_hid_set_report(keyboard_device, keyboard_instance, 0,
                            HID_REPORT_TYPE_OUTPUT, &keyboard_output_active, 1)) {
        critical_section_enter_blocking(&state_lock);
        keyboard_output_busy = false;
        if (!keyboard_output_pending) {
            keyboard_output_queued = keyboard_output_active;
            keyboard_output_pending = true;
        }
        critical_section_exit(&state_lock);
    }
}

void service_rgb_sender() {
    if (!raw_ready) return;
    begin_latest_frame_if_idle();
    if (sending_chunk >= LV_CHUNK_COUNT) return;

    uint8_t packet[LV_PACKET_SIZE];
    lv_encode_chunk(packet, sending_frame_id, sending_chunk, sending_frame);
    if (tuh_hid_send_report(raw_device, raw_instance, 0, packet, sizeof(packet))) {
        ++sending_chunk;
    }
}

void record_root_port_attach() {
    const root_port_t *root = &pio_usb_root_port[0];
    if (!root->connected) return;

    record_host_status(LV_USB_DEVICE_ATTACHED);
    if (root->is_fullspeed) record_host_status(LV_USB_DEVICE_FULL_SPEED);
}

void host_core() {
    sleep_ms(10);
    pio_usb_configuration_t config = PIO_USB_DEFAULT_CONFIG;
    tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &config);
    if (!tuh_init(1)) {
        host_status = LV_USB_HOST_ERROR;
        host_status_history = LV_USB_HOST_ERROR;
        while (true) tight_loop_contents();
    }
    host_status = LV_USB_HOST_STARTED;
    host_status_history = LV_USB_HOST_STARTED;
    while (true) {
        record_root_port_attach();
        tuh_task();
        record_root_port_attach();
        service_rgb_sender();
        service_keyboard_output();
    }
}
}  // namespace

void usb_bridge_init() {
    critical_section_init(&state_lock);
    set_sys_clock_khz(120000, true);
    multicore_reset_core1();
    multicore_launch_core1(host_core);
    tud_init(0);
}

void usb_bridge_device_task() {
    tud_task();

    UpstreamReport report{};
    bool available = false;
    critical_section_enter_blocking(&state_lock);
    if (upstream_read != upstream_write) {
        report = upstream_queue[upstream_read];
        available = true;
    }
    critical_section_exit(&state_lock);

    if (!available) return;
    if (tud_suspended()) {
        tud_remote_wakeup();
        return;
    }
    if (!tud_hid_ready()) return;
    if (tud_hid_report(report.id, report.data, report.length)) {
        critical_section_enter_blocking(&state_lock);
        upstream_read = (upstream_read + 1) % kUpstreamQueueSize;
        critical_section_exit(&state_lock);
    }
}

void usb_bridge_submit_frame(const lv_rgb_t frame[LV_KEY_COUNT]) {
    critical_section_enter_blocking(&state_lock);
    std::memcpy(requested_frame, frame, sizeof(requested_frame));
    ++requested_generation;
    critical_section_exit(&state_lock);
}

uint8_t usb_bridge_host_status() {
    return host_status;
}

uint8_t usb_bridge_host_status_history() {
    return host_status_history;
}

extern "C" void tuh_event_hook_cb(uint8_t rhport, uint32_t eventid, bool) {
    if (rhport == 1 && eventid == kHostAttachEvent) {
        record_host_status(LV_USB_ATTACH_EVENT);
    }
}

extern "C" void tuh_mount_cb(uint8_t) {
    record_host_status(LV_USB_DEVICE_MOUNTED);
}

extern "C" void tuh_umount_cb(uint8_t) {
    raw_device = 0;
    raw_instance = 0;
    raw_ready = false;
    keyboard_device = 0;
    keyboard_instance = 0;
    critical_section_enter_blocking(&state_lock);
    keyboard_output_queued = 0;
    keyboard_output_active = 0;
    keyboard_output_pending = false;
    keyboard_output_busy = false;
    critical_section_exit(&state_lock);
    queue_upstream_releases();
    host_status = LV_USB_HOST_STARTED;
    host_status_history = LV_USB_HOST_STARTED;
}

extern "C" void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance,
                                  const uint8_t *report_desc, uint16_t desc_len) {
    if (is_qmk_raw_hid(report_desc, desc_len)) {
        raw_device = dev_addr;
        raw_instance = instance;
        raw_ready = true;
        record_host_status(LV_USB_RAW_MOUNTED);
    } else if (tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_KEYBOARD) {
        keyboard_device = dev_addr;
        keyboard_instance = instance;
        record_host_status(LV_USB_KEYBOARD_MOUNTED);
    }
    tuh_hid_receive_report(dev_addr, instance);
}

extern "C" void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    if (dev_addr == raw_device && instance == raw_instance) {
        raw_ready = false;
        host_status &= (uint8_t)~LV_USB_RAW_MOUNTED;
    }
    if (dev_addr == keyboard_device && instance == keyboard_instance) {
        keyboard_device = 0;
        host_status &= (uint8_t)~LV_USB_KEYBOARD_MOUNTED;
    }
}

extern "C" void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                            const uint8_t *report, uint16_t len) {
    if (!(dev_addr == raw_device && instance == raw_instance)) {
        const uint8_t protocol = tuh_hid_interface_protocol(dev_addr, instance);
        if (protocol == HID_ITF_PROTOCOL_KEYBOARD && len == sizeof(hid_keyboard_report_t)) {
            queue_upstream_report(kKeyboardReportId, report, len);
        } else if (protocol == HID_ITF_PROTOCOL_MOUSE && len == sizeof(hid_mouse_report_t)) {
            queue_upstream_report(kMouseReportId, report, len);
        } else if (len >= 2) {
            // QMK composite reports include their report ID as byte zero.
            const uint8_t report_id = report[0];
            if (report_id == kNkroReportId && len == 32) {
                queue_upstream_report(report_id, report + 1, (uint16_t)(len - 1));
            } else if (report_id == kConsumerReportId || report_id == kSystemReportId) {
                queue_upstream_report(report_id, report + 1, (uint16_t)(len - 1));
            }
        }
    }
    tuh_hid_receive_report(dev_addr, instance);
}

extern "C" void tud_hid_set_report_cb(uint8_t, uint8_t report_id,
                                       hid_report_type_t report_type,
                                       const uint8_t *buffer, uint16_t bufsize) {
    (void)report_id;
    if (!keyboard_device || report_type != HID_REPORT_TYPE_OUTPUT || bufsize != 1) return;

    critical_section_enter_blocking(&state_lock);
    keyboard_output_queued = buffer[0];
    keyboard_output_pending = true;
    critical_section_exit(&state_lock);
}

extern "C" void tuh_hid_set_report_complete_cb(uint8_t, uint8_t, uint8_t,
                                                uint8_t, uint16_t) {
    critical_section_enter_blocking(&state_lock);
    keyboard_output_busy = false;
    critical_section_exit(&state_lock);
}

extern "C" uint16_t tud_hid_get_report_cb(uint8_t, uint8_t, hid_report_type_t, uint8_t *,
                                           uint16_t) {
    return 0;
}
