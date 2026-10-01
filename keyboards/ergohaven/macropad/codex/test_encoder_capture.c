// SPDX-License-Identifier: GPL-2.0-or-later
#include "encoder_capture.h"
#include <assert.h>
#include <stdio.h>

static void turn(codex_encoder_capture_t *c, bool clockwise) {
    const uint8_t cw[] = {2,0,1,3}, ccw[] = {1,0,2,3};
    for (unsigned i = 0; i < 4; ++i) codex_encoder_capture_edge(c, clockwise ? cw[i] : ccw[i]);
}
static int8_t pop(codex_encoder_capture_t *c) {
    int8_t value = codex_encoder_capture_peek(c);
    if (value) codex_encoder_capture_pop(c);
    return value;
}
int main(void) {
    codex_encoder_capture_t c;
    codex_encoder_capture_init(&c, 3);
    turn(&c, true); turn(&c, false);
    assert(pop(&c) == 1); assert(pop(&c) == -1); assert(pop(&c) == 0);
    // Bounce at the detent, then reverse a half turn: neither is a click.
    const uint8_t noise[] = {1,3,1,0,1,3,3};
    for (unsigned i = 0; i < sizeof(noise); ++i) codex_encoder_capture_edge(&c, noise[i]);
    assert(pop(&c) == 0);
    // Bounce inside a full turn still produces exactly one event.
    const uint8_t bounce[] = {1,3,1,0,1,0,2,0,2,3,2,3};
    for (unsigned i = 0; i < sizeof(bounce); ++i) codex_encoder_capture_edge(&c, bounce[i]);
    assert(pop(&c) == -1); assert(pop(&c) == 0);
    // Invalid two-bit transition loses sync, then recovers at the detent.
    codex_encoder_capture_edge(&c, 0); codex_encoder_capture_edge(&c, 2); codex_encoder_capture_edge(&c, 3);
    assert(pop(&c) == 0); turn(&c, false); assert(pop(&c) == -1);
    codex_encoder_capture_init(&c, 0);
    codex_encoder_capture_edge(&c, 1); codex_encoder_capture_edge(&c, 3);
    assert(pop(&c) == 0); turn(&c, true); assert(pop(&c) == 1);
    // Rotation continues while display flush blocks the consumer; preserve order.
    for (unsigned i = 0; i < 24; ++i) turn(&c, i % 3 != 0);
    for (unsigned i = 0; i < 24; ++i) assert(pop(&c) == (i % 3 != 0 ? 1 : -1));
    assert(pop(&c) == 0);
    // Overflow cannot corrupt queued events or prevent later direction changes.
    for (unsigned i = 0; i < 40; ++i) turn(&c, true);
    for (unsigned i = 0; i < CODEX_ENCODER_QUEUE_SIZE - 1; ++i) assert(pop(&c) == 1);
    assert(pop(&c) == 0); turn(&c, false); assert(pop(&c) == -1);
    puts("encoder capture tests passed");
}
