// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct { bool held, pending, double_tap; uint32_t released_at; } codex_layer_button_t;
static inline int codex_layer_button_tick(codex_layer_button_t *s, uint32_t now) {
    if (s->pending && !s->held && (uint32_t)(now - s->released_at) >= 250) {
        s->pending = false;
        return 1;
    }
    return 0;
}
// Caller ticks before each event. Commit only on release/timeout, never while held.
static inline int codex_layer_button_event(codex_layer_button_t *s, bool down, uint32_t now) {
    if (down) {
        if (s->held) return 0;
        s->held = true;
        s->double_tap = s->pending;
        s->pending = false;
    } else if (s->held) {
        s->held = false;
        if (s->double_tap) { s->double_tap = false; return -1; }
        s->pending = true;
        s->released_at = now;
    }
    return 0;
}
static inline uint8_t codex_layer_destination(uint8_t layer, int direction) {
    if (layer >= 5) return 0;
    return (layer + (direction < 0 ? 4 : 1)) % 5;
}
