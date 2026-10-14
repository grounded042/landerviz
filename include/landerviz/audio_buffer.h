#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t fill;
    int8_t ready;
} lv_audio_buffer_state_t;

// Called after fill reaches the end of a buffer. If the consumer has not taken
// the previous buffer, keep overwriting the current one instead of corrupting
// the buffer that is waiting to be analyzed.
static inline bool lv_audio_buffer_complete(volatile lv_audio_buffer_state_t *state) {
    if (state->ready >= 0) return false;
    state->ready = (int8_t)state->fill;
    state->fill ^= 1u;
    return true;
}

// The caller must exclude the producer interrupt while taking a buffer.
static inline int lv_audio_buffer_take(volatile lv_audio_buffer_state_t *state) {
    const int ready = state->ready;
    state->ready = -1;
    return ready;
}
