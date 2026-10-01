// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t selected;
    int8_t held;
} codex_navigation_t;

static inline void codex_navigation_rotate(codex_navigation_t *nav, bool clockwise) {
    nav->selected = (nav->selected + (clockwise ? 1 : 5)) % 6;
}

// Latch the slot until release, even if the encoder turns or the layer changes.
static inline int8_t codex_navigation_press(codex_navigation_t *nav, bool pressed) {
    if (pressed) {
        if (nav->held >= 0) return -1;
        nav->held = nav->selected;
        return nav->held;
    }
    int8_t slot = nav->held;
    nav->held = -1;
    return slot;
}
