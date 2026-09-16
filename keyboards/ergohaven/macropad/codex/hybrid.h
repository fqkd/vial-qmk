// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dynamic_keymap.h"

enum codex_hybrid_keycodes {
    CD_TASK1 = QK_USER_0, CD_TASK2, CD_TASK3, CD_TASK4, CD_TASK5, CD_TASK6,
    CD_FAST, CD_OK, CD_NO, CD_NEW, CD_MIC, CD_SEND, CD_CLICK, CD_CCW, CD_CW, CD_MODE
};
static inline uint16_t codex_hybrid_keycode(uint8_t key) {
    // Resolve transparent assignments just as QMK does for ordinary layers.
    layer_state_t active = layer_state | default_layer_state;
    for (int8_t layer = DYNAMIC_KEYMAP_LAYER_COUNT - 1; layer >= 0; --layer) {
        if (!(active & ((layer_state_t)1 << layer))) continue;
        uint16_t code = dynamic_keymap_get_keycode(layer, key / 3 + 1, key % 3);
        if (code != KC_TRNS) return code;
    }
    return KC_NO;
}
