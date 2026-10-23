#include "landerviz/encoder.h"

#include <cstring>

namespace {
constexpr int8_t transition_delta[16] = {
     0,  1, -1,  0,
    -1,  0,  0,  1,
     1,  0,  0, -1,
     0, -1,  1,  0,
};
}

void lv_encoder_init(lv_encoder_t *encoder, bool a, bool b) {
    std::memset(encoder, 0, sizeof(*encoder));
    encoder->previous_state = static_cast<uint8_t>((a ? 2u : 0u) |
                                                    (b ? 1u : 0u));
}

int8_t lv_encoder_update(lv_encoder_t *encoder, bool a, bool b) {
    const uint8_t state = static_cast<uint8_t>((a ? 2u : 0u) |
                                               (b ? 1u : 0u));
    const uint8_t changed = encoder->previous_state ^ state;
    if (changed == 0) return 0;

    if (changed == 3) {
        ++encoder->invalid_transitions;
        encoder->quarter_steps = 0;
        encoder->previous_state = state;
        return 0;
    }

    const uint8_t transition = static_cast<uint8_t>(
        (encoder->previous_state << 2u) | state);
    encoder->quarter_steps = static_cast<int8_t>(
        encoder->quarter_steps + transition_delta[transition]);
    encoder->previous_state = state;

    if (encoder->quarter_steps >= 4) {
        encoder->quarter_steps = 0;
        ++encoder->position;
        encoder->last_direction = 1;
        return 1;
    }
    if (encoder->quarter_steps <= -4) {
        encoder->quarter_steps = 0;
        --encoder->position;
        encoder->last_direction = -1;
        return -1;
    }
    return 0;
}
