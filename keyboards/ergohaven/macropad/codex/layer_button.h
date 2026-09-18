// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
static inline uint8_t codex_layer_destination(uint8_t layer, int direction) {
    if (layer >= 5) return 0;
    return (layer + (direction < 0 ? 4 : 1)) % 5;
}
