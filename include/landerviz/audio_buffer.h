#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t fill;
    int8_t ready;
    uint32_t dropped;
    uint8_t decimation_phase;
} lv_audio_buffer_state_t;

// Called after fill reaches the end of a buffer. If the consumer has not taken
// the previous buffer, keep overwriting the current one instead of corrupting
// the buffer that is waiting to be analyzed.
static inline bool lv_audio_buffer_complete(volatile lv_audio_buffer_state_t *state) {
    if (state->ready >= 0) {
        ++state->dropped;
        return false;
    }
    state->ready = (int8_t)state->fill;
    state->fill ^= 1u;
    return true;
}

// Mark only the first of each group of completed capture windows as ready.
// Skipped windows remain writable and do not count as consumer drops.
static inline bool lv_audio_buffer_complete_decimated(
    volatile lv_audio_buffer_state_t *state, uint8_t decimation) {
    if (decimation <= 1) return lv_audio_buffer_complete(state);

    const bool publish = state->decimation_phase == 0;
    if (++state->decimation_phase >= decimation) {
        state->decimation_phase = 0;
    }
    return publish ? lv_audio_buffer_complete(state) : false;
}

// The caller must exclude the producer interrupt while taking a buffer.
static inline int lv_audio_buffer_take(volatile lv_audio_buffer_state_t *state) {
    const int ready = state->ready;
    state->ready = -1;
    return ready;
}
