// SPDX-License-Identifier: GPL-2.0-or-later
#include QMK_KEYBOARD_H
#include "../../codex/protocol.h"
#include "../../codex/hybrid.h"
#include "../../codex/layer_button.h"
#include "ergohaven.h"

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(CD_CLICK, CD_TASK1, CD_TASK2, CD_TASK3, CD_TASK4, CD_TASK5, CD_TASK6,
                 CD_NO, CD_OK, CD_SEND, CD_NEW, CD_MIC, CD_MODE),
    [1] = LAYOUT(KC_MUTE, KC_7, KC_8, KC_9, KC_4, KC_5, KC_6,
                 KC_1, KC_2, KC_3, KC_0, LAYER_PREV, LAYER_NEXT),
    [2] = LAYOUT(KC_MUTE, KC_HOME, KC_INS, KC_END, PREVWRD, KC_UP, NEXTWRD,
                 KC_LEFT, KC_DOWN, KC_RIGHT, KC_DEL, LAYER_PREV, LAYER_NEXT),
    [3] = LAYOUT(KC_BTN3, C(KC_X), C(KC_C), C(KC_V), KC_BTN1, KC_MS_U, KC_BTN2,
                 KC_MS_L, KC_MS_D, KC_MS_R, KC_PSCR, LAYER_PREV, LAYER_NEXT),
    [4] = LAYOUT(KC_MUTE, KC_BRID, KC_CPNL, KC_BRIU, KC_MYCM, KC_WSCH, KC_MAIL,
                 KC_MPRV, KC_MPLY, KC_MNXT, KC_CALC, LAYER_PREV, LAYER_NEXT),
};
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [0] = { ENCODER_CCW_CW(CD_CCW, CD_CW) },
    [1] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [2] = { ENCODER_CCW_CW(KC_PGDN, KC_PGUP) },
    [3] = { ENCODER_CCW_CW(KC_WH_D, KC_WH_U) },
    [4] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
};

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
    if (keycode == CD_MODE || keycode == LAYER_NEXT || keycode == LAYER_PREV) {
        if (record->event.pressed)
            layer_move(codex_layer_destination(get_highest_layer(layer_state | default_layer_state), keycode == LAYER_PREV ? -1 : 1));
        return false;
    }
    return true;
}
