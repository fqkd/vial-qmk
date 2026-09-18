// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dynamic_keymap.h"

enum codex_hybrid_keycodes {
    CD_TASK1 = QK_USER_0, CD_TASK2, CD_TASK3, CD_TASK4, CD_TASK5, CD_TASK6,
    CD_FAST, CD_OK, CD_NO, CD_NEW, CD_MIC, CD_SEND, CD_CLICK, CD_CCW, CD_CW, CD_MODE
};
_Static_assert(CD_TASK1 == 0x7E00 + 64 && CD_MODE == 0x7E00 + 79,
               "Codex codes must match the appended Vial customKeycodes");
static inline uint16_t codex_hybrid_matrix_keycode(uint8_t row, uint8_t col) {
    // Resolve transparent assignments just as QMK does for ordinary layers.
    layer_state_t active = layer_state | default_layer_state;
    for (int8_t layer = DYNAMIC_KEYMAP_LAYER_COUNT - 1; layer >= 0; --layer) {
        if (!(active & ((layer_state_t)1 << layer))) continue;
        uint16_t code = dynamic_keymap_get_keycode(layer, row, col);
        if (code != KC_TRNS) return code;
    }
    return KC_NO;
}
static inline uint16_t codex_hybrid_keycode(uint8_t key) {
    return codex_hybrid_matrix_keycode(key / 3 + 1, key % 3);
}
static inline uint16_t codex_hybrid_encoder_keycode(bool clockwise) {
    layer_state_t active = layer_state | default_layer_state;
    for (int8_t layer = DYNAMIC_KEYMAP_LAYER_COUNT - 1; layer >= 0; --layer) {
        if (!(active & ((layer_state_t)1 << layer))) continue;
        uint16_t code = dynamic_keymap_get_encoder(layer, 0, clockwise);
        if (code != KC_TRNS) return code;
    }
    return KC_NO;
}
