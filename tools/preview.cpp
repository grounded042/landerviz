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
#include "landerviz/moonlander_map.h"
#include "landerviz/visualizer.h"

namespace {

constexpr int kSampleRate = 15625;
constexpr int kWindowWidth = 900;
constexpr int kWindowHeight = 540;
constexpr uint8_t kBrightnessCap = 175;

struct Options {
    std::string input;
    int fps = 25;
    int frame_limit = 0;
    int flow_travel_ms = LV_SPECTRAL_FLOW_TRAVEL_MS;
    bool demo = false;
};

struct KeyRect {
    SDL_Rect rect{};
    bool valid = false;
};

void usage(const char *program) {
    std::fprintf(stderr,
                 "usage: %s [--fps N] [--travel-ms N] [--frames N] FILE\n"
                 "       %s [--fps N] [--travel-ms N] [--frames N] --demo\n",
                 program, program);
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
    return options->demo == options->input.empty();
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
    constexpr int key_size = 42;
    constexpr int spacing = 49;
    constexpr int column_offset[6] = {20, 9, 0, 5, 15, 25};

    for (int half = 0; half < 2; ++half) {
        const int base_x = half == 0 ? 20 : 548;
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < 6; ++col) {
                const int led = lv_key_at(half, col, row);
                if (led < 0) continue;
                keys[led] = {{base_x + col * spacing,
                              30 + row * 58 + column_offset[col],
                              key_size, key_size},
                             true};
            }
        }
    }

    for (int key = 0; key < 3; ++key) {
        keys[lv_utility_key_at(0, key)] =
            {{320, 68 + key * 60, key_size, key_size}, true};
        keys[lv_utility_key_at(1, key)] =
            {{498, 68 + key * 60, key_size, key_size}, true};
    }

    constexpr int left_thumb_x[4] = {240, 282, 320, 352};
    constexpr int right_thumb_x[4] = {618, 576, 538, 506};
    constexpr int thumb_y[4] = {350, 378, 414, 450};
    for (int key = 0; key < 4; ++key) {
        keys[lv_thumb_key_at(0, key)] =
            {{left_thumb_x[key], thumb_y[key], key_size, key_size}, true};
        keys[lv_thumb_key_at(1, key)] =
            {{right_thumb_x[key], thumb_y[key], key_size, key_size}, true};
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

void draw_keyboard(SDL_Renderer *renderer,
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
    SDL_RenderPresent(renderer);
}

uint16_t adc_sample(float sample) {
    // Firmware applies an 8x compensation gain after its passive mixer input.
    // Divide here so the shared analyzer sees the original file amplitude.
    const float scaled = std::clamp(sample / 8.0f, -0.999f, 0.999f);
    return (uint16_t)(LV_ADC_MID + scaled * LV_ADC_MID);
}

void reset_playback(SDL_AudioDeviceID device, const std::vector<float> &audio,
                    lv_channel_analysis_t analysis[LV_AUDIO_CHANNEL_COUNT],
                    lv_audio_activity_t *activity, lv_visualizer_t *visualizer,
                    size_t *analyzed_frames) {
    const uint16_t flow_travel_ms =
        lv_visualizer_flow_travel_ms(visualizer);
    SDL_ClearQueuedAudio(device);
    SDL_QueueAudio(device, audio.data(),
                   (Uint32)(audio.size() * sizeof(float)));
    std::memset(analysis, 0,
                sizeof(lv_channel_analysis_t) * LV_AUDIO_CHANNEL_COUNT);
    *activity = {};
    *visualizer = {};
    lv_visualizer_set_flow_travel_ms(visualizer, flow_travel_ms);
    *analyzed_frames = 0;
}

}  // namespace

int main(int argc, char **argv) {
    Options options;
    if (!parse_options(argc, argv, &options)) {
        usage(argv[0]);
        return 2;
    }

    std::vector<float> audio =
        options.demo ? demo_audio() : decode_audio(options.input);
    if (audio.empty()) {
        std::fprintf(stderr, "no audio samples available\n");
        return 1;
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

    lv_channel_analysis_t analysis[LV_AUDIO_CHANNEL_COUNT]{};
    lv_audio_activity_t activity{};
    lv_visualizer_t visualizer{};
    lv_visualizer_set_flow_travel_ms(
        &visualizer, (uint16_t)options.flow_travel_ms);
    lv_rgb_t frame[LV_KEY_COUNT]{};
    size_t analyzed_frames = 0;
    reset_playback(device, audio, analysis, &activity, &visualizer,
                   &analyzed_frames);
    SDL_PauseAudioDevice(device, 0);

    lv_viz_mode_t mode = lv_viz_mode_t::FLOW;
    bool running = true;
    bool paused = false;
    int rendered_frames = 0;
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
                                   &visualizer, &analyzed_frames);
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
                case SDLK_LEFTBRACKET:
                    options.fps = std::max(1, options.fps - 1);
                    next_render_counter = (double)SDL_GetPerformanceCounter();
                    break;
                case SDLK_RIGHTBRACKET:
                    options.fps = std::min(120, options.fps + 1);
                    next_render_counter = (double)SDL_GetPerformanceCounter();
                    break;
                case SDLK_COMMA:
                    lv_visualizer_set_flow_travel_ms(
                        &visualizer,
                        (uint16_t)std::min<int>(
                            1000,
                            lv_visualizer_flow_travel_ms(&visualizer) + 10));
                    break;
                case SDLK_PERIOD:
                    lv_visualizer_set_flow_travel_ms(
                        &visualizer,
                        (uint16_t)std::max<int>(
                            50,
                            lv_visualizer_flow_travel_ms(&visualizer) - 10));
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

        if (options.demo && !paused &&
            played_frames >= total_audio_frames && analyzed_frames > 0) {
            reset_playback(device, audio, analysis, &activity, &visualizer,
                           &analyzed_frames);
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
            }
        }

        if (!paused || rendered_frames == 0) {
            lv_visualizer_render(&visualizer, mode, analysis, activity.active,
                                 now_ms, frame);
        }
        draw_keyboard(renderer, keys, frame);

        char title[512];
        std::snprintf(
            title, sizeof(title),
            "LanderViz — %s — %d fps — %u ms/column — [1-5 mode] [,/. speed] [brackets: fps] [space pause] — %s",
            lv_viz_mode_name(mode), options.fps,
            lv_visualizer_flow_travel_ms(&visualizer),
            options.demo ? "demo" : options.input.c_str());
        SDL_SetWindowTitle(window, title);

        ++rendered_frames;
        if (options.frame_limit > 0 && rendered_frames >= options.frame_limit) {
            running = false;
        }
    }

    SDL_CloseAudioDevice(device);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
