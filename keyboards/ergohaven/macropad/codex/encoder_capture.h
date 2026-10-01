// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define CODEX_ENCODER_QUEUE_SIZE 32
typedef struct {
    uint8_t state;
    int8_t pulses;
    bool synced;
    // Single ISR producer, single main-loop consumer. Publish the entry before
    // head, and release it only after consumption; RP2040 byte accesses are atomic.
    volatile int8_t events[CODEX_ENCODER_QUEUE_SIZE];
    volatile uint8_t head, tail;
} codex_encoder_capture_t;

static inline void codex_encoder_capture_init(codex_encoder_capture_t *capture, uint8_t state) {
    capture->state = state & 3;
    capture->pulses = 0;
    capture->synced = capture->state == 3;
    capture->head = capture->tail = 0;
}

// Called for each GPIO edge. A complete cycle back to the detent emits one
// event; a bounce or a reversed partial turn emits none. Direction matches QMK.
static inline void codex_encoder_capture_edge(codex_encoder_capture_t *capture, uint8_t state) {
    static const int8_t delta[16] = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
    state &= 3;
    uint8_t previous = capture->state;
    if (state == previous) return;
    capture->state = state;
    if ((state ^ previous) == 3) {
        capture->pulses = 0;
        capture->synced = state == 3;
        return;
    }
    if (!capture->synced) {
        capture->synced = state == 3;
        return;
    }
    capture->pulses += delta[(previous << 2) | state];
    if (state != 3) return;
    int8_t direction = capture->pulses == -4 ? 1 : capture->pulses == 4 ? -1 : 0;
    capture->pulses = 0;
    if (!direction) return;
    uint8_t next = (capture->head + 1) % CODEX_ENCODER_QUEUE_SIZE;
    if (next == capture->tail) return; // Full: drop the new event, never reorder.
    capture->events[capture->head] = direction;
    capture->head = next;
}

static inline int8_t codex_encoder_capture_peek(const codex_encoder_capture_t *capture) {
    return capture->head == capture->tail ? 0 : capture->events[capture->tail];
}
static inline void codex_encoder_capture_pop(codex_encoder_capture_t *capture) {
    capture->tail = (capture->tail + 1) % CODEX_ENCODER_QUEUE_SIZE;
}
