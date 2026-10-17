#include <SDL.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "landerviz/audio_analysis.h"
#include "landerviz/flow_v2.h"
#include "landerviz/flow_v3.h"
#include "landerviz/flow_v3_mid.h"
#include "landerviz/moonlander_map.h"
#include "landerviz/visualizer.h"

namespace {

constexpr int kSampleRate = 15625;
constexpr int kWindowWidth = 900;
constexpr int kWindowHeight = 540;
constexpr uint8_t kBrightnessCap = 175;
constexpr int kGlowTextureSize = 112;

struct Options {
    std::string input;
    std::string trace_path;
    int fps = 30;
    int frame_limit = 0;
    int flow_travel_ms = LV_SPECTRAL_FLOW_TRAVEL_MS;
    bool demo = false;
    bool kick_demo = false;
    int flow_version = 3;
    bool bass_contrast = true;
    bool mid_lab = true;
    bool analyze_only = false;
};

struct KeyRect {
    SDL_Rect rect{};
    bool valid = false;
};

void usage(const char *program) {
    std::fprintf(stderr,
                 "usage: %s [--fps N] [--travel-ms N] [--frames N] [--trace CSV] [--flow-v1|--flow-v2|--flow-v3] [--bass-lab|--mid-lab] [--bass-a|--bass-b] [--analyze-only] FILE\n"
                 "       %s [--fps N] [--travel-ms N] [--frames N] [--trace CSV] [--flow-v1|--flow-v2|--flow-v3] [--bass-lab|--mid-lab] [--bass-a|--bass-b] --demo\n"
                 "       %s [--fps N] [--travel-ms N] [--frames N] --demo-kicks\n",
                 program, program, program);
}

bool parse_positive_int(const char *text, int *value) {
    char *end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (!text[0] || *end || parsed <= 0 || parsed > 1000000) return false;
    *value = (int)parsed;
    return true;
}

bool parse_options(int argc, char **argv, Options *options) {
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--demo") {
            options->demo = true;
        } else if (argument == "--demo-kicks") {
            options->kick_demo = true;
        } else if (argument == "--flow-v1") {
            options->flow_version = 1;
        } else if (argument == "--flow-v2") {
            options->flow_version = 2;
        } else if (argument == "--flow-v3") {
            options->flow_version = 3;
        } else if (argument == "--bass-a") {
            options->bass_contrast = false;
        } else if (argument == "--bass-b") {
            options->bass_contrast = true;
        } else if (argument == "--bass-lab") {
            options->mid_lab = false;
        } else if (argument == "--mid-lab") {
            options->mid_lab = true;
        } else if (argument == "--analyze-only") {
            options->analyze_only = true;
        } else if (argument == "--trace") {
            if (++i >= argc || !options->trace_path.empty()) return false;
            options->trace_path = argv[i];
        } else if (argument == "--fps" || argument == "--frames" ||
                   argument == "--travel-ms") {
            if (++i >= argc) return false;
            int value = 0;
            if (!parse_positive_int(argv[i], &value)) return false;
            if (argument == "--fps") options->fps = value;
            else if (argument == "--frames") options->frame_limit = value;
            else {
                if (value < 50 || value > 1000) return false;
                options->flow_travel_ms = value;
            }
        } else if (argument == "--help" || argument == "-h") {
            usage(argv[0]);
            std::exit(0);
        } else if (!argument.empty() && argument[0] == '-') {
            return false;
        } else if (options->input.empty()) {
            options->input = argument;
        } else {
            return false;
        }
    }
    const int sources = (options->demo ? 1 : 0) +
                        (options->kick_demo ? 1 : 0) +
                        (!options->input.empty() ? 1 : 0);
    return sources == 1 && (!options->analyze_only ||
                            !options->trace_path.empty());
}

std::vector<float> demo_audio() {
    constexpr int seconds = 20;
    std::vector<float> audio((size_t)seconds * kSampleRate * 2);
    constexpr float pi = 3.14159265358979323846f;
    for (int sample = 0; sample < seconds * kSampleRate; ++sample) {
        const float time = (float)sample / kSampleRate;
        const float kick_phase = std::fmod(time, 0.5f);
        const float kick = std::exp(-kick_phase * 13.0f) *
                           std::sin(2.0f * pi * 74.0f * time);
        const float sweep_frequency = 180.0f +
                                      1500.0f *
                                          (0.5f + 0.5f * std::sin(time * 0.7f));
        const float sweep = 0.32f *
                            std::sin(2.0f * pi * sweep_frequency * time);
        audio[(size_t)sample * 2] = 0.62f * kick + sweep;
        audio[(size_t)sample * 2 + 1] =
            0.52f * kick + 0.35f * std::sin(2.0f * pi * 880.0f * time);
    }
    return audio;
}

std::vector<float> kick_demo_audio() {
    constexpr int seconds = 20;
    constexpr float pi = 3.14159265358979323846f;
    std::vector<float> audio((size_t)seconds * kSampleRate * 2);
    for (int sample = 0; sample < seconds * kSampleRate; ++sample) {
        const float time = (float)sample / kSampleRate;
        float kick = 0.0f;
        if (time >= 0.5f) {
            const float phase = std::fmod(time - 0.5f, 0.5f);
            if (phase < 0.18f) {
                kick = 0.75f * std::exp(-phase * 18.0f) *
                       std::sin(2.0f * pi * 74.0f * phase);
            }
        }
        audio[(size_t)sample * 2] = kick;
        audio[(size_t)sample * 2 + 1] = kick;
    }
    return audio;
}

std::vector<float> decode_audio(const std::string &path) {
    int output_pipe[2];
    if (pipe(output_pipe) != 0) {
        std::perror("pipe");
        return {};
    }

    const pid_t child = fork();
    if (child == 0) {
        close(output_pipe[0]);
        if (dup2(output_pipe[1], STDOUT_FILENO) < 0) _exit(126);
        close(output_pipe[1]);
        execlp("ffmpeg", "ffmpeg", "-v", "error", "-i", path.c_str(),
               "-f", "f32le", "-acodec", "pcm_f32le", "-ac", "2",
               "-ar", "15625", "pipe:1", (char *)nullptr);
        _exit(127);
    }
    close(output_pipe[1]);
    if (child < 0) {
        std::perror("fork");
        close(output_pipe[0]);
        return {};
    }

    std::vector<uint8_t> bytes;
    std::array<uint8_t, 65536> buffer{};
    while (true) {
        const ssize_t count = read(output_pipe[0], buffer.data(), buffer.size());
        if (count > 0) {
            bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + count);
        } else if (count == 0) {
            break;
        } else if (errno != EINTR) {
            std::perror("read");
            break;
        }
    }
    close(output_pipe[0]);

    int status = 0;
    if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) {
        std::fprintf(stderr, "ffmpeg could not decode %s\n", path.c_str());
        return {};
    }
    bytes.resize(bytes.size() - bytes.size() % sizeof(float));
    std::vector<float> audio(bytes.size() / sizeof(float));
    std::memcpy(audio.data(), bytes.data(), bytes.size());
    if (audio.size() % 2 != 0) audio.pop_back();
    return audio;
}

std::array<KeyRect, LV_KEY_COUNT> keyboard_geometry() {
    std::array<KeyRect, LV_KEY_COUNT> keys{};
    constexpr int key_size = 38;
    constexpr int key_inset = 2;
    constexpr int spacing = 49;
    constexpr int column_offset[6] = {20, 9, 0, 5, 15, 25};

    for (int half = 0; half < 2; ++half) {
        const int base_x = half == 0 ? 20 : 548;
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < 6; ++col) {
                const int led = lv_key_at(half, col, row);
                if (led < 0) continue;
                keys[led] = {{base_x + col * spacing + key_inset,
                              30 + row * 58 + column_offset[col] + key_inset,
                              key_size, key_size},
                             true};
            }
        }
    }

    for (int key = 0; key < 3; ++key) {
        keys[lv_utility_key_at(0, key)] =
            {{320 + key_inset, 68 + key * 60 + key_inset,
              key_size, key_size}, true};
        keys[lv_utility_key_at(1, key)] =
            {{498 + key_inset, 68 + key * 60 + key_inset,
              key_size, key_size}, true};
    }

    constexpr int left_thumb_x[4] = {240, 282, 320, 352};
    constexpr int right_thumb_x[4] = {618, 576, 538, 506};
    constexpr int thumb_y[4] = {350, 378, 414, 450};
    for (int key = 0; key < 4; ++key) {
        keys[lv_thumb_key_at(0, key)] =
            {{left_thumb_x[key] + key_inset, thumb_y[key] + key_inset,
              key_size, key_size}, true};
        keys[lv_thumb_key_at(1, key)] =
            {{right_thumb_x[key] + key_inset, thumb_y[key] + key_inset,
              key_size, key_size}, true};
    }
    return keys;
}

lv_rgb_t cap_color(lv_rgb_t color) {
    return {
        (uint8_t)(((uint16_t)color.r * kBrightnessCap + 127) / 255),
        (uint8_t)(((uint16_t)color.g * kBrightnessCap + 127) / 255),
        (uint8_t)(((uint16_t)color.b * kBrightnessCap + 127) / 255),
    };
}

lv_rgb_t preview_light_color(lv_rgb_t color) {
    const lv_rgb_t capped = cap_color(color);
    const auto lift = [](uint8_t channel) {
        const float normalized = (float)channel / 255.0f;
        return (uint8_t)std::lround(
            255.0f * std::pow(normalized, 0.72f));
    };
    return {lift(capped.r), lift(capped.g), lift(capped.b)};
}

SDL_Texture *create_glow_texture(SDL_Renderer *renderer) {
    SDL_PixelFormat *format = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888);
    if (!format) return nullptr;

    std::array<uint32_t, kGlowTextureSize * kGlowTextureSize> pixels{};
    const float center = ((float)kGlowTextureSize - 1.0f) * 0.5f;
    for (int y = 0; y < kGlowTextureSize; ++y) {
        for (int x = 0; x < kGlowTextureSize; ++x) {
            const float dx = ((float)x - center) / center;
            const float dy = ((float)y - center) / center;
            const float distance = std::sqrt(dx * dx + dy * dy);
            const float fade = std::clamp((distance - 0.18f) / 0.82f,
                                          0.0f, 1.0f);
            const float smooth = fade * fade * (3.0f - 2.0f * fade);
            const uint8_t alpha =
                (uint8_t)std::lround(78.0f * (1.0f - smooth));
            pixels[(size_t)y * kGlowTextureSize + x] =
                SDL_MapRGBA(format, 255, 255, 255, alpha);
        }
    }
    SDL_FreeFormat(format);

    SDL_Texture *texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC,
        kGlowTextureSize, kGlowTextureSize);
    if (!texture) return nullptr;
    if (SDL_UpdateTexture(texture, nullptr, pixels.data(),
                          kGlowTextureSize * (int)sizeof(uint32_t)) != 0 ||
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_ADD) != 0) {
        SDL_DestroyTexture(texture);
        return nullptr;
    }
    return texture;
}

void draw_keyboard_flat(SDL_Renderer *renderer,
                        const std::array<KeyRect, LV_KEY_COUNT> &keys,
                        const lv_rgb_t frame[LV_KEY_COUNT]) {
    SDL_SetRenderDrawColor(renderer, 10, 12, 18, 255);
    SDL_RenderClear(renderer);

    for (int key = 0; key < LV_KEY_COUNT; ++key) {
        if (!keys[key].valid) continue;
        SDL_Rect shadow = keys[key].rect;
        shadow.x += 4;
        shadow.y += 5;
        SDL_SetRenderDrawColor(renderer, 1, 2, 4, 255);
        SDL_RenderFillRect(renderer, &shadow);

        SDL_SetRenderDrawColor(renderer, 35, 38, 48, 255);
        SDL_RenderFillRect(renderer, &keys[key].rect);
        SDL_Rect light = keys[key].rect;
        light.x += 7;
        light.y += 7;
        light.w -= 14;
        light.h -= 14;
        const lv_rgb_t color = cap_color(frame[key]);
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
        SDL_RenderFillRect(renderer, &light);
    }
}

void draw_keyboard_realistic(
    SDL_Renderer *renderer, SDL_Texture *glow,
    const std::array<KeyRect, LV_KEY_COUNT> &keys,
    const lv_rgb_t frame[LV_KEY_COUNT]) {
    SDL_SetRenderDrawColor(renderer, 7, 9, 14, 255);
    SDL_RenderClear(renderer);

    // Overlapping expanded rectangles suggest the two keyboard chassis while
    // preserving the real staggered and thumb-cluster geometry.
    for (int key = 0; key < LV_KEY_COUNT; ++key) {
        if (!keys[key].valid) continue;
        SDL_Rect shadow = keys[key].rect;
        shadow.x -= 7;
        shadow.y -= 5;
        shadow.w += 18;
        shadow.h += 18;
        SDL_SetRenderDrawColor(renderer, 1, 2, 4, 255);
        SDL_RenderFillRect(renderer, &shadow);
    }
    for (int key = 0; key < LV_KEY_COUNT; ++key) {
        if (!keys[key].valid) continue;
        SDL_Rect plate = keys[key].rect;
        plate.x -= 5;
        plate.y -= 5;
        plate.w += 10;
        plate.h += 10;
        SDL_SetRenderDrawColor(renderer, 17, 20, 28, 255);
        SDL_RenderFillRect(renderer, &plate);
    }

    for (int key = 0; key < LV_KEY_COUNT; ++key) {
        if (!keys[key].valid) continue;
        const lv_rgb_t color = preview_light_color(frame[key]);
        if (color.r == 0 && color.g == 0 && color.b == 0) continue;
        SDL_SetTextureColorMod(glow, color.r, color.g, color.b);
        SDL_Rect destination{
            keys[key].rect.x + (keys[key].rect.w - kGlowTextureSize) / 2,
            keys[key].rect.y + (keys[key].rect.h - kGlowTextureSize) / 2,
            kGlowTextureSize,
            kGlowTextureSize,
        };
        SDL_RenderCopy(renderer, glow, nullptr, &destination);
    }

    for (int key = 0; key < LV_KEY_COUNT; ++key) {
        if (!keys[key].valid) continue;
        SDL_Rect key_shadow = keys[key].rect;
        key_shadow.x += 3;
        key_shadow.y += 4;
        SDL_SetRenderDrawColor(renderer, 2, 3, 6, 230);
        SDL_RenderFillRect(renderer, &key_shadow);

        const lv_rgb_t color = preview_light_color(frame[key]);
        SDL_SetRenderDrawColor(renderer, 37, 40, 49, 255);
        SDL_RenderFillRect(renderer, &keys[key].rect);

        SDL_Rect face = keys[key].rect;
        face.x += 6;
        face.y += 6;
        face.w -= 12;
        face.h -= 12;
        const auto face_channel = [](uint8_t channel) {
            return (uint8_t)std::min(255, 23 + (int)channel * 3 / 5);
        };
        SDL_SetRenderDrawColor(renderer, face_channel(color.r),
                               face_channel(color.g), face_channel(color.b),
                               255);
        SDL_RenderFillRect(renderer, &face);

        SDL_SetRenderDrawColor(renderer,
                               (uint8_t)std::min(255, 52 + color.r / 3),
                               (uint8_t)std::min(255, 55 + color.g / 3),
                               (uint8_t)std::min(255, 64 + color.b / 3), 255);
        SDL_RenderDrawLine(renderer, face.x, face.y,
                           face.x + face.w - 1, face.y);
    }
}

void draw_keyboard(SDL_Renderer *renderer, SDL_Texture *glow,
                   bool realistic_lighting,
                   const std::array<KeyRect, LV_KEY_COUNT> &keys,
                   const lv_rgb_t frame[LV_KEY_COUNT]) {
    if (realistic_lighting) {
        draw_keyboard_realistic(renderer, glow, keys, frame);
    } else {
        draw_keyboard_flat(renderer, keys, frame);
    }
    SDL_RenderPresent(renderer);
}

uint16_t adc_sample(float sample) {
    // Firmware applies an 8x compensation gain after its passive mixer input.
    // Divide here so the shared analyzer sees the original file amplitude.
    const float scaled = std::clamp(sample / 8.0f, -0.999f, 0.999f);
    return (uint16_t)(LV_ADC_MID + scaled * LV_ADC_MID);
}

std::FILE *open_trace(const std::string &path) {
    std::FILE *trace = std::fopen(path.c_str(), "w");
    if (!trace) {
        std::fprintf(stderr, "cannot open trace %s: %s\n", path.c_str(),
                     std::strerror(errno));
        return nullptr;
    }
    std::fprintf(
        trace,
        "audio_ms,gate_open,gain,raw_left,raw_right,value_left,value_right,raw_centroid_left_hz,raw_centroid_right_hz,centroid_left_hz,centroid_right_hz,palette_left,palette_right,dominance_left,dominance_right,flux_low,threshold_low,onset_low,triggered_low,flux_mid,threshold_mid,onset_mid,triggered_mid,flux_high,threshold_high,onset_high,triggered_high,v3_raw_bass,v3a_body,v3b_body,v3_attack,v3_hue,mid_raw,mid_centroid_hz,mid_contrast_centroid_hz,mid_contrast_strength,mid_base_hue,mid_target_hue,mid_hue,mid_presence\n");
    return trace;
}

void write_trace_row(std::FILE *trace,
                     const lv_flow_v2_features_t &features,
                     const lv_flow_v3_features_t &v3a,
                     const lv_flow_v3_features_t &v3b,
                     const lv_flow_v3_mid_features_t &mids) {
    std::fprintf(
        trace,
        "%u,%d,%.6f,%.6f,%.6f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%.6f,%.6f,%.6f,%d,%.6f,%.6f,%.6f,%d,%.6f,%.6f,%.6f,%.6f,%.3f,%.6f,%.3f,%.3f,%.6f,%.3f,%.3f,%.3f,%.6f\n",
        features.audio_time_ms, features.gate_open ? 1 : 0, features.gain,
        features.channel[0].raw_level, features.channel[1].raw_level,
        features.channel[0].value, features.channel[1].value,
        features.channel[0].raw_centroid_hz,
        features.channel[1].raw_centroid_hz,
        features.channel[0].centroid_hz,
        features.channel[1].centroid_hz,
        features.channel[0].palette_position,
        features.channel[1].palette_position,
        features.channel[0].dominance, features.channel[1].dominance,
        features.onset_flux[0], features.onset_threshold[0],
        features.onset_low, features.onset_triggered[0] ? 1 : 0,
        features.onset_flux[1], features.onset_threshold[1],
        features.onset_mid, features.onset_triggered[1] ? 1 : 0,
        features.onset_flux[2], features.onset_threshold[2],
        features.onset_high, features.onset_triggered[2] ? 1 : 0,
        v3a.raw_bass, v3a.body, v3b.body, v3b.attack, v3b.hue,
        mids.raw_level, mids.centroid_hz, mids.contrast_centroid_hz,
        mids.contrast_strength, mids.base_hue, mids.target_hue, mids.hue,
        mids.presence);
}

bool write_offline_trace(const std::vector<float> &audio,
                         const std::string &path) {
    std::FILE *trace = open_trace(path);
    if (!trace) return false;

    lv_channel_analysis_t analysis[LV_AUDIO_CHANNEL_COUNT]{};
    lv_flow_v2_t flow_v2{};
    lv_flow_v3_t flow_v3_a{};
    lv_flow_v3_t flow_v3_b{};
    lv_flow_v3_mid_t flow_v3_mid{};
    lv_flow_v3_set_bass_contrast(&flow_v3_b, true);
    uint32_t onset_counts[3]{};
    uint32_t trace_rows = 0;
    const size_t total_audio_frames = audio.size() / 2;
    size_t analyzed_frames = 0;
    while (analyzed_frames + LV_FFT_N <= total_audio_frames) {
        uint16_t samples[LV_AUDIO_CHANNEL_COUNT][LV_FFT_N];
        for (int sample = 0; sample < LV_FFT_N; ++sample) {
            const size_t offset = (analyzed_frames + sample) * 2;
            samples[0][sample] = adc_sample(audio[offset]);
            samples[1][sample] = adc_sample(audio[offset + 1]);
        }

        const size_t window_index = analyzed_frames / LV_FFT_N;
        analyzed_frames += LV_FFT_N;
        if (window_index % LV_ANALYSIS_DECIMATION != 0) continue;

        bool above_gate = false;
        for (int channel = 0; channel < LV_AUDIO_CHANNEL_COUNT; ++channel) {
            lv_analyze_channel(&analysis[channel], samples[channel], 8.0f);
            above_gate |=
                lv_meter_level(analysis[channel].rms_level * 1.41421356f) >
                0.0f;
        }
        const uint32_t audio_midpoint_ms = (uint32_t)(
            (analyzed_frames - LV_FFT_N / 2) * 1000 / kSampleRate);
        lv_flow_v2_update(&flow_v2, analysis, above_gate,
                          audio_midpoint_ms);
        lv_flow_v3_update(&flow_v3_a, analysis, above_gate,
                          audio_midpoint_ms);
        lv_flow_v3_update(&flow_v3_b, analysis, above_gate,
                          audio_midpoint_ms);
        lv_flow_v3_mid_update(&flow_v3_mid, analysis, above_gate,
                              audio_midpoint_ms);
        for (int band = 0; band < 3; ++band) {
            if (flow_v2.features.onset_triggered[band]) {
                ++onset_counts[band];
            }
        }
        write_trace_row(trace, flow_v2.features, flow_v3_a.features,
                        flow_v3_b.features, flow_v3_mid.features);
        ++trace_rows;
    }
    const bool closed = std::fclose(trace) == 0;
    std::fprintf(stderr,
                 "analyzed %u frames; onsets low/mid/high: %u/%u/%u\n",
                 trace_rows, onset_counts[0], onset_counts[1],
                 onset_counts[2]);
    return closed;
}

void reset_playback(SDL_AudioDeviceID device, const std::vector<float> &audio,
                    lv_channel_analysis_t analysis[LV_AUDIO_CHANNEL_COUNT],
                    lv_audio_activity_t *activity, lv_visualizer_t *visualizer,
                    lv_flow_v2_t *flow_v2, lv_flow_v3_t *flow_v3_a,
                    lv_flow_v3_t *flow_v3_b,
                    lv_flow_v3_mid_t *flow_v3_mid,
                    size_t *analyzed_frames,
                    bool kick_packets_enabled) {
    const uint16_t flow_travel_ms =
        lv_visualizer_flow_travel_ms(visualizer);
    const uint16_t flow_v2_travel_ms =
        flow_v2->ribbon[0].travel_ms > 0
            ? flow_v2->ribbon[0].travel_ms
            : (uint16_t)LV_SPECTRAL_FLOW_TRAVEL_MS;
    const uint16_t flow_v3_travel_ms =
        flow_v3_a->travel_ms > 0
            ? flow_v3_a->travel_ms
            : (uint16_t)LV_FLOW_V3_DEFAULT_TRAVEL_MS;
    SDL_ClearQueuedAudio(device);
    SDL_QueueAudio(device, audio.data(),
                   (Uint32)(audio.size() * sizeof(float)));
    std::memset(analysis, 0,
                sizeof(lv_channel_analysis_t) * LV_AUDIO_CHANNEL_COUNT);
    *activity = {};
    *visualizer = {};
    *flow_v2 = {};
    *flow_v3_a = {};
    *flow_v3_b = {};
    *flow_v3_mid = {};
    lv_flow_v3_set_bass_contrast(flow_v3_b, true);
    lv_visualizer_set_flow_travel_ms(visualizer, flow_travel_ms);
    lv_visualizer_set_flow_kick_packets(visualizer, kick_packets_enabled);
    lv_flow_v2_set_travel_ms(flow_v2, flow_v2_travel_ms);
    lv_flow_v3_set_travel_ms(flow_v3_a, flow_v3_travel_ms);
    lv_flow_v3_set_travel_ms(flow_v3_b, flow_v3_travel_ms);
    lv_flow_v3_mid_set_travel_ms(flow_v3_mid, flow_v3_travel_ms);
    *analyzed_frames = 0;
}

}  // namespace

int main(int argc, char **argv) {
    Options options;
    if (!parse_options(argc, argv, &options)) {
        usage(argv[0]);
        return 2;
    }

    std::vector<float> audio = options.demo
                                   ? demo_audio()
                                   : (options.kick_demo
                                          ? kick_demo_audio()
                                          : decode_audio(options.input));
    if (audio.empty()) {
        std::fprintf(stderr, "no audio samples available\n");
        return 1;
    }
    if (options.analyze_only) {
        return write_offline_trace(audio, options.trace_path) ? 0 : 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *window = SDL_CreateWindow(
        "LanderViz preview", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kWindowWidth, kWindowHeight,
        SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer =
        window ? SDL_CreateRenderer(window, -1,
                                    SDL_RENDERER_ACCELERATED |
                                        SDL_RENDERER_PRESENTVSYNC)
               : nullptr;
    if (window && !renderer) {
        // Headless test drivers and some remote sessions expose only the
        // software renderer. Interactive macOS runs use synchronized Metal.
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!window || !renderer) {
        std::fprintf(stderr, "SDL window failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_RenderSetLogicalSize(renderer, kWindowWidth, kWindowHeight);
    SDL_Texture *glow = create_glow_texture(renderer);
    if (!glow) {
        std::fprintf(stderr, "SDL glow texture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_AudioSpec desired{};
    desired.freq = kSampleRate;
    desired.format = AUDIO_F32SYS;
    desired.channels = 2;
    desired.samples = 512;
    SDL_AudioSpec obtained{};
    const SDL_AudioDeviceID device =
        SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (!device) {
        std::fprintf(stderr, "SDL audio failed: %s\n", SDL_GetError());
        SDL_DestroyTexture(glow);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const auto keys = keyboard_geometry();
    for (int key = 0; key < LV_KEY_COUNT; ++key) {
        if (!keys[key].valid) {
            std::fprintf(stderr, "preview geometry is missing LED %d\n", key);
            return 1;
        }
    }

    std::FILE *trace = nullptr;
    if (!options.trace_path.empty()) {
        trace = open_trace(options.trace_path);
        if (!trace) {
            SDL_CloseAudioDevice(device);
            SDL_DestroyTexture(glow);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
    }

    lv_channel_analysis_t analysis[LV_AUDIO_CHANNEL_COUNT]{};
    lv_audio_activity_t activity{};
    lv_visualizer_t visualizer{};
    lv_flow_v2_t flow_v2{};
    lv_flow_v3_t flow_v3_a{};
    lv_flow_v3_t flow_v3_b{};
    lv_flow_v3_mid_t flow_v3_mid{};
    lv_flow_v3_set_bass_contrast(&flow_v3_b, true);
    lv_visualizer_set_flow_travel_ms(
        &visualizer, (uint16_t)options.flow_travel_ms);
    lv_flow_v2_set_travel_ms(&flow_v2,
                             (uint16_t)options.flow_travel_ms);
    lv_flow_v3_set_travel_ms(&flow_v3_a,
                             (uint16_t)options.flow_travel_ms);
    lv_flow_v3_set_travel_ms(&flow_v3_b,
                             (uint16_t)options.flow_travel_ms);
    lv_flow_v3_mid_set_travel_ms(&flow_v3_mid,
                                 (uint16_t)options.flow_travel_ms);
    lv_rgb_t frame[LV_KEY_COUNT]{};
    size_t analyzed_frames = 0;
    reset_playback(device, audio, analysis, &activity, &visualizer, &flow_v2,
                   &flow_v3_a, &flow_v3_b, &flow_v3_mid, &analyzed_frames,
                   options.kick_demo);
    SDL_PauseAudioDevice(device, 0);

    lv_viz_mode_t mode = lv_viz_mode_t::FLOW;
    bool running = true;
    bool paused = false;
    bool realistic_lighting = true;
    int flow_version = options.flow_version;
    bool bass_contrast = options.bass_contrast;
    bool mid_lab = options.mid_lab;
    bool visualization_dirty = false;
    int rendered_frames = 0;
    uint32_t detected_kicks = 0;
    uint32_t onset_counts[3]{};
    uint32_t observed_kick_ms = 0;
    bool observed_kick = false;
    uint32_t trace_rows = 0;
    const uint64_t performance_frequency = SDL_GetPerformanceFrequency();
    uint64_t playback_started_at = SDL_GetPerformanceCounter();
    uint64_t paused_at = 0;
    uint64_t total_paused_ticks = 0;
    double next_render_counter = (double)playback_started_at;
    const size_t total_audio_frames = audio.size() / 2;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type != SDL_KEYDOWN || event.key.repeat) continue;
            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                case SDLK_q:
                    running = false;
                    break;
                case SDLK_SPACE:
                    if (!paused) {
                        paused_at = SDL_GetPerformanceCounter();
                        paused = true;
                    } else {
                        const uint64_t resumed_at = SDL_GetPerformanceCounter();
                        total_paused_ticks += resumed_at - paused_at;
                        paused = false;
                    }
                    SDL_PauseAudioDevice(device, paused ? 1 : 0);
                    break;
                case SDLK_r:
                    reset_playback(device, audio, analysis, &activity,
                                   &visualizer, &flow_v2, &flow_v3_a,
                                   &flow_v3_b, &flow_v3_mid,
                                   &analyzed_frames, options.kick_demo);
                    detected_kicks = 0;
                    std::fill(onset_counts, onset_counts + 3, 0);
                    observed_kick_ms = 0;
                    observed_kick = false;
                    playback_started_at = SDL_GetPerformanceCounter();
                    total_paused_ticks = 0;
                    paused_at = paused ? playback_started_at : 0;
                    SDL_PauseAudioDevice(device, paused ? 1 : 0);
                    break;
                case SDLK_LEFT:
                    mode = (lv_viz_mode_t)(
                        ((int)mode + (int)lv_viz_mode_t::COUNT - 1) %
                        (int)lv_viz_mode_t::COUNT);
                    break;
                case SDLK_RIGHT:
                    mode = (lv_viz_mode_t)(
                        ((int)mode + 1) % (int)lv_viz_mode_t::COUNT);
                    break;
                case SDLK_1: mode = lv_viz_mode_t::FLOW; break;
                case SDLK_2: mode = lv_viz_mode_t::SPECTRUM; break;
                case SDLK_3: mode = lv_viz_mode_t::PULSE; break;
                case SDLK_4: mode = lv_viz_mode_t::VU; break;
                case SDLK_5: mode = lv_viz_mode_t::IDLE; break;
                case SDLK_g:
                    realistic_lighting = !realistic_lighting;
                    break;
                case SDLK_t:
                    flow_version = flow_version % 3 + 1;
                    visualization_dirty = true;
                    break;
                case SDLK_b:
                    bass_contrast = !bass_contrast;
                    visualization_dirty = true;
                    break;
                case SDLK_m:
                    mid_lab = !mid_lab;
                    visualization_dirty = true;
                    break;
                case SDLK_LEFTBRACKET:
                    options.fps = std::max(1, options.fps - 1);
                    next_render_counter = (double)SDL_GetPerformanceCounter();
                    break;
                case SDLK_RIGHTBRACKET:
                    options.fps = std::min(120, options.fps + 1);
                    next_render_counter = (double)SDL_GetPerformanceCounter();
                    break;
                case SDLK_COMMA:
                    {
                        const uint16_t travel_ms =
                            (uint16_t)std::min<int>(
                                1000,
                                lv_visualizer_flow_travel_ms(&visualizer) +
                                    10);
                        lv_visualizer_set_flow_travel_ms(&visualizer,
                                                         travel_ms);
                        lv_flow_v2_set_travel_ms(&flow_v2, travel_ms);
                        lv_flow_v3_set_travel_ms(&flow_v3_a, travel_ms);
                        lv_flow_v3_set_travel_ms(&flow_v3_b, travel_ms);
                        lv_flow_v3_mid_set_travel_ms(&flow_v3_mid,
                                                     travel_ms);
                    }
                    break;
                case SDLK_PERIOD:
                    {
                        const uint16_t travel_ms =
                            (uint16_t)std::max<int>(
                                50,
                                lv_visualizer_flow_travel_ms(&visualizer) -
                                    10);
                        lv_visualizer_set_flow_travel_ms(&visualizer,
                                                         travel_ms);
                        lv_flow_v2_set_travel_ms(&flow_v2, travel_ms);
                        lv_flow_v3_set_travel_ms(&flow_v3_a, travel_ms);
                        lv_flow_v3_set_travel_ms(&flow_v3_b, travel_ms);
                        lv_flow_v3_mid_set_travel_ms(&flow_v3_mid,
                                                     travel_ms);
                    }
                    break;
                default: break;
            }
        }

        const uint64_t counter = SDL_GetPerformanceCounter();
        if ((double)counter < next_render_counter) {
            SDL_Delay(1);
            continue;
        }
        const double render_period =
            (double)performance_frequency / options.fps;
        do {
            next_render_counter += render_period;
        } while (next_render_counter <= (double)counter);

        const uint64_t timeline_counter = paused ? paused_at : counter;
        const uint64_t elapsed_ticks =
            timeline_counter - playback_started_at - total_paused_ticks;
        const uint32_t now_ms = (uint32_t)(
            elapsed_ticks * 1000u / performance_frequency);
        const size_t played_frames = std::min(
            total_audio_frames,
            (size_t)((long double)elapsed_ticks * kSampleRate /
                     performance_frequency));

        if ((options.demo || options.kick_demo) && !paused &&
            played_frames >= total_audio_frames && analyzed_frames > 0) {
            reset_playback(device, audio, analysis, &activity, &visualizer,
                           &flow_v2, &flow_v3_a, &flow_v3_b, &flow_v3_mid,
                           &analyzed_frames, options.kick_demo);
            detected_kicks = 0;
            std::fill(onset_counts, onset_counts + 3, 0);
            observed_kick_ms = 0;
            observed_kick = false;
            playback_started_at = SDL_GetPerformanceCounter();
            total_paused_ticks = 0;
            SDL_PauseAudioDevice(device, 0);
            continue;
        }
        while (analyzed_frames + LV_FFT_N <= played_frames &&
               analyzed_frames + LV_FFT_N <= total_audio_frames) {
            uint16_t samples[LV_AUDIO_CHANNEL_COUNT][LV_FFT_N];
            for (int sample = 0; sample < LV_FFT_N; ++sample) {
                const size_t offset = (analyzed_frames + sample) * 2;
                samples[0][sample] = adc_sample(audio[offset]);
                samples[1][sample] = adc_sample(audio[offset + 1]);
            }

            const size_t window_index = analyzed_frames / LV_FFT_N;
            analyzed_frames += LV_FFT_N;
            if (window_index % LV_ANALYSIS_DECIMATION == 0) {
                bool above_gate = false;
                for (int channel = 0; channel < LV_AUDIO_CHANNEL_COUNT;
                     ++channel) {
                    lv_analyze_channel(&analysis[channel], samples[channel],
                                       8.0f);
                    above_gate |=
                        lv_meter_level(analysis[channel].rms_level *
                                       1.41421356f) > 0.0f;
                }
                const uint32_t audio_ms =
                    (uint32_t)(analyzed_frames * 1000 / kSampleRate);
                lv_audio_activity_update(&activity, above_gate, audio_ms);
                const uint32_t audio_midpoint_ms = (uint32_t)(
                    (analyzed_frames - LV_FFT_N / 2) * 1000 / kSampleRate);
                lv_flow_v2_update(&flow_v2, analysis, above_gate,
                                  audio_midpoint_ms);
                lv_flow_v3_update(&flow_v3_a, analysis, above_gate,
                                  audio_midpoint_ms);
                lv_flow_v3_update(&flow_v3_b, analysis, above_gate,
                                  audio_midpoint_ms);
                lv_flow_v3_mid_update(&flow_v3_mid, analysis, above_gate,
                                      audio_midpoint_ms);
                for (int band = 0; band < 3; ++band) {
                    if (flow_v2.features.onset_triggered[band]) {
                        ++onset_counts[band];
                    }
                }
                if (trace) {
                    write_trace_row(trace, flow_v2.features,
                                    flow_v3_a.features,
                                    flow_v3_b.features,
                                    flow_v3_mid.features);
                    if (++trace_rows % 30 == 0) std::fflush(trace);
                }
            }
        }

        if (!paused || rendered_frames == 0 || visualization_dirty) {
            lv_rgb_t v1_frame[LV_KEY_COUNT]{};
            lv_visualizer_render(&visualizer, mode, analysis, activity.active,
                                 now_ms, v1_frame);
            if (mode == lv_viz_mode_t::FLOW && flow_version == 3 &&
                activity.active) {
                if (mid_lab) {
                    lv_flow_v3_mid_render(&flow_v3_mid, now_ms, frame);
                } else {
                    lv_flow_v3_render(
                        bass_contrast ? &flow_v3_b : &flow_v3_a,
                        now_ms, frame);
                }
            } else if (mode == lv_viz_mode_t::FLOW && flow_version == 2 &&
                       activity.active) {
                lv_flow_v2_render(&flow_v2, now_ms, frame);
            } else {
                std::memcpy(frame, v1_frame, sizeof(frame));
            }
            if (options.kick_demo && visualizer.flow_kick.kick_seen &&
                (!observed_kick || visualizer.flow_kick.last_kick_ms !=
                                       observed_kick_ms)) {
                ++detected_kicks;
                observed_kick = true;
                observed_kick_ms = visualizer.flow_kick.last_kick_ms;
            }
            visualization_dirty = false;
        }
        draw_keyboard(renderer, glow, realistic_lighting, keys, frame);

        char title[512];
        char source_status[256];
        if (options.kick_demo) {
            const uint32_t expected_kicks =
                now_ms < 500 ? 0 : 1 + (now_ms - 500) / 500;
            std::snprintf(source_status, sizeof(source_status),
                          "kick test %u/%u detected", detected_kicks,
                          expected_kicks);
        } else {
            std::snprintf(source_status, sizeof(source_status), "%s",
                          options.demo ? "demo" : options.input.c_str());
        }
        char mode_status[64];
        if (mode == lv_viz_mode_t::FLOW) {
            if (flow_version == 3) {
                if (mid_lab) {
                    std::snprintf(mode_status, sizeof(mode_status),
                                  "FLOW v3 MID %.0f Hz hue %.0f",
                                  flow_v3_mid.features.centroid_hz,
                                  flow_v3_mid.features.hue);
                } else {
                    const auto &bass = bass_contrast ? flow_v3_b.features
                                                     : flow_v3_a.features;
                    std::snprintf(mode_status, sizeof(mode_status),
                                  "FLOW v3%c bass %.2f body %.2f",
                                  bass_contrast ? 'B' : 'A', bass.raw_bass,
                                  bass.body);
                }
            } else if (flow_version == 2) {
                std::snprintf(mode_status, sizeof(mode_status),
                              "FLOW v2 L/M/H %u/%u/%u",
                              onset_counts[0], onset_counts[1],
                              onset_counts[2]);
            } else {
                std::snprintf(mode_status, sizeof(mode_status), "FLOW v1");
            }
        } else {
            std::snprintf(mode_status, sizeof(mode_status), "%s",
                          lv_viz_mode_name(mode));
        }
        std::snprintf(
            title, sizeof(title),
            "LanderViz — %s — %d fps — %u ms/column — %s light — [T v1/v2/v3] [M bass/mids] [B bass A/B] [1-5 mode] [G light] [,/. speed] [brackets: fps] [space pause] — %s",
            mode_status, options.fps,
            lv_visualizer_flow_travel_ms(&visualizer),
            realistic_lighting ? "realistic" : "flat",
            source_status);
        SDL_SetWindowTitle(window, title);

        ++rendered_frames;
        if (options.frame_limit > 0 && rendered_frames >= options.frame_limit) {
            running = false;
        }
    }

    if (trace) std::fclose(trace);
    SDL_CloseAudioDevice(device);
    SDL_DestroyTexture(glow);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
