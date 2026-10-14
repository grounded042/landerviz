#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    LV_KEY_COUNT = 72,
    LV_PACKET_SIZE = 32,
    LV_KEYS_PER_CHUNK = 9,
    LV_CHUNK_COUNT = 8,
    LV_MAGIC_0 = 'L',
    LV_MAGIC_1 = 'V',
    LV_COMMAND_RELEASE = 0x01,
    LV_COMMAND_FRAME_BASE = 0x80,
};

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} lv_rgb_t;

typedef enum {
    LV_PACKET_IGNORED = 0,
    LV_PACKET_ACCEPTED,
    LV_FRAME_READY,
    LV_RELEASED,
} lv_packet_result_t;

typedef struct {
    lv_rgb_t pending[LV_KEY_COUNT];
    lv_rgb_t active[LV_KEY_COUNT];
    uint8_t frame_id;
    uint8_t chunks_received;
    bool receiving;
    bool active_valid;
} lv_decoder_t;

static inline uint8_t lv_checksum(const uint8_t packet[LV_PACKET_SIZE]) {
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < LV_PACKET_SIZE - 1; ++i) {
        checksum ^= packet[i];
    }
    return checksum;
}

static inline void lv_encode_chunk(uint8_t packet[LV_PACKET_SIZE], uint8_t frame_id,
                                   uint8_t chunk, const lv_rgb_t pixels[LV_KEY_COUNT]) {
    memset(packet, 0, LV_PACKET_SIZE);
    packet[0] = LV_MAGIC_0;
    packet[1] = LV_MAGIC_1;
    packet[2] = (uint8_t)(LV_COMMAND_FRAME_BASE | (chunk & 0x07));
    packet[3] = frame_id;

    const uint8_t first = (uint8_t)(chunk * LV_KEYS_PER_CHUNK);
    for (uint8_t i = 0; i < LV_KEYS_PER_CHUNK; ++i) {
        const uint8_t key = (uint8_t)(first + i);
        packet[4 + i * 3] = pixels[key].r;
        packet[5 + i * 3] = pixels[key].g;
        packet[6 + i * 3] = pixels[key].b;
    }
    packet[LV_PACKET_SIZE - 1] = lv_checksum(packet);
}

static inline void lv_encode_release(uint8_t packet[LV_PACKET_SIZE]) {
    memset(packet, 0, LV_PACKET_SIZE);
    packet[0] = LV_MAGIC_0;
    packet[1] = LV_MAGIC_1;
    packet[2] = LV_COMMAND_RELEASE;
    packet[LV_PACKET_SIZE - 1] = lv_checksum(packet);
}

static inline lv_packet_result_t lv_decode_packet(lv_decoder_t *decoder,
                                                   const uint8_t packet[LV_PACKET_SIZE]) {
    if (packet[0] != LV_MAGIC_0 || packet[1] != LV_MAGIC_1 ||
        packet[LV_PACKET_SIZE - 1] != lv_checksum(packet)) {
        return LV_PACKET_IGNORED;
    }

    if (packet[2] == LV_COMMAND_RELEASE) {
        decoder->receiving = false;
        decoder->active_valid = false;
        decoder->chunks_received = 0;
        return LV_RELEASED;
    }

    if ((packet[2] & 0xF8) != LV_COMMAND_FRAME_BASE) {
        return LV_PACKET_IGNORED;
    }

    const uint8_t chunk = (uint8_t)(packet[2] & 0x07);
    const uint8_t frame_id = packet[3];
    if (!decoder->receiving || decoder->frame_id != frame_id) {
        decoder->receiving = true;
        decoder->frame_id = frame_id;
        decoder->chunks_received = 0;
    }

    const uint8_t first = (uint8_t)(chunk * LV_KEYS_PER_CHUNK);
    for (uint8_t i = 0; i < LV_KEYS_PER_CHUNK; ++i) {
        const uint8_t key = (uint8_t)(first + i);
        decoder->pending[key].r = packet[4 + i * 3];
        decoder->pending[key].g = packet[5 + i * 3];
        decoder->pending[key].b = packet[6 + i * 3];
    }
    decoder->chunks_received |= (uint8_t)(1u << chunk);

    if (decoder->chunks_received == 0xFF) {
        memcpy(decoder->active, decoder->pending, sizeof(decoder->active));
        decoder->active_valid = true;
        decoder->receiving = false;
        return LV_FRAME_READY;
    }
    return LV_PACKET_ACCEPTED;
}

#ifdef __cplusplus
}
#endif
