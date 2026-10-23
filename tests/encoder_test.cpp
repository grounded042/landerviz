#include <cassert>

#include "landerviz/encoder.h"

static void test_one_full_cycle_is_one_detent() {
    lv_encoder_t encoder{};
    lv_encoder_init(&encoder, false, false);

    assert(lv_encoder_update(&encoder, false, true) == 0);
    assert(lv_encoder_update(&encoder, true, true) == 0);
    assert(lv_encoder_update(&encoder, true, false) == 0);
    assert(lv_encoder_update(&encoder, false, false) == 1);
    assert(encoder.position == 1);
    assert(encoder.last_direction == 1);
}

static void test_reverse_cycle_decrements() {
    lv_encoder_t encoder{};
    lv_encoder_init(&encoder, false, false);

    assert(lv_encoder_update(&encoder, true, false) == 0);
    assert(lv_encoder_update(&encoder, true, true) == 0);
    assert(lv_encoder_update(&encoder, false, true) == 0);
    assert(lv_encoder_update(&encoder, false, false) == -1);
    assert(encoder.position == -1);
    assert(encoder.last_direction == -1);
}

static void test_contact_bounce_cancels_without_false_detent() {
    lv_encoder_t encoder{};
    lv_encoder_init(&encoder, false, false);

    assert(lv_encoder_update(&encoder, false, true) == 0);
    assert(lv_encoder_update(&encoder, false, false) == 0);
    assert(lv_encoder_update(&encoder, false, true) == 0);
    assert(lv_encoder_update(&encoder, true, true) == 0);
    assert(lv_encoder_update(&encoder, true, false) == 0);
    assert(lv_encoder_update(&encoder, false, false) == 1);
    assert(encoder.position == 1);
    assert(encoder.invalid_transitions == 0);
}

static void test_skipped_gray_state_is_rejected() {
    lv_encoder_t encoder{};
    lv_encoder_init(&encoder, false, false);

    assert(lv_encoder_update(&encoder, true, true) == 0);
    assert(encoder.position == 0);
    assert(encoder.invalid_transitions == 1);

    assert(lv_encoder_update(&encoder, false, false) == 0);
    assert(encoder.invalid_transitions == 2);
}

int main() {
    test_one_full_cycle_is_one_detent();
    test_reverse_cycle_decrements();
    test_contact_bounce_cancels_without_false_detent();
    test_skipped_gray_state_is_rejected();
    return 0;
}
