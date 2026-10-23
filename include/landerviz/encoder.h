#pragma once

#include <cstdint>

struct lv_encoder_t {
    int32_t position;
    uint32_t invalid_transitions;
    int8_t quarter_steps;
    int8_t last_direction;
    uint8_t previous_state;
};

void lv_encoder_init(lv_encoder_t *encoder, bool a, bool b);

// Returns +1 or -1 after a complete four-transition quadrature cycle, or 0
// while a cycle is incomplete. Reversing during contact bounce cancels the
// partial movement rather than creating a false detent.
int8_t lv_encoder_update(lv_encoder_t *encoder, bool a, bool b);
