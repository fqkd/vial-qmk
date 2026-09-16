// SPDX-License-Identifier: GPL-2.0-or-later
#include QMK_KEYBOARD_H
#include "../../codex/protocol.h"
#include "../../codex/hybrid.h"

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(CD_MODE, CD_TASK1, CD_TASK2, CD_TASK3, CD_TASK4, CD_TASK5, CD_TASK6,
                 CD_FAST, CD_OK, CD_NO, CD_NEW, CD_MIC, CD_SEND),
    [1] = LAYOUT(CD_MODE, KC_7, KC_8, KC_9, KC_4, KC_5, KC_6,
                 KC_1, KC_2, KC_3, KC_0, KC_DOT, KC_ENTER),
    [2] = LAYOUT(CD_MODE, C(KC_X), C(KC_C), C(KC_V), KC_HOME, KC_UP, KC_END,
                 KC_LEFT, KC_DOWN, KC_RIGHT, KC_PGUP, KC_PGDN, KC_DEL),
    [3] = LAYOUT(CD_MODE, KC_MPRV, KC_MPLY, KC_MNXT, KC_F13, KC_F14, KC_F15,
                 KC_F16, KC_F17, KC_F18, KC_F19, KC_F20, KC_F21),
};
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [0] = { ENCODER_CCW_CW(CD_CCW, CD_CW) },
    [1] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [2] = { ENCODER_CCW_CW(KC_PGUP, KC_PGDN) },
    [3] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
};

static uint32_t mode_time;
static bool mode_held;
bool pre_process_record_user(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    uint8_t row = record->event.key.row, col = record->event.key.col;
    if (row >= 1 && row <= 4 && col < 3) codex_ui_key((row - 1) * 3 + col, record->event.pressed);
    return true; // Let Vial, macros, tap dance and normal QMK layers run.
}
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (keycode >= CD_TASK1 && keycode <= CD_CLICK) {
        codex_key(keycode - CD_TASK1, record->event.pressed);
        return false;
    }
    if (keycode == CD_CCW || keycode == CD_CW) {
        if (record->event.pressed) codex_encoder(keycode == CD_CW);
        return false;
    }
    if (keycode == CD_MODE) {
        if (record->event.pressed) { mode_time = timer_read32(); mode_held = true; }
        else if (mode_held) {
            mode_held = false;
            if (timer_elapsed32(mode_time) >= 600) layer_move((get_highest_layer(layer_state | default_layer_state) + 1) % 4);
            else if (get_highest_layer(layer_state | default_layer_state) == 0) {
                codex_key(12, true); codex_key(12, false);
            } else tap_code(KC_MUTE);
        }
        return false;
    }
    return true;
}
