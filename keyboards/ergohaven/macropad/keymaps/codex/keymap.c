// SPDX-License-Identifier: GPL-2.0-or-later
#include QMK_KEYBOARD_H
#include "../../codex/protocol.h"

_Static_assert(VENDOR_ID == 0x303A && PRODUCT_ID == 0x8360, "Native discovery identity was overridden");

// Physical matrix events are authoritative; no ordinary keyboard events escape
// the native mode, including when old Vial mappings remain in EEPROM.
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO,
                KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO),
};

bool pre_process_record_user(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    uint8_t row = record->event.key.row, col = record->event.key.col;
    if (row == 0 && col == 2) codex_key(12, record->event.pressed);
    else if (row >= 1 && row <= 4 && col < 3) {
        uint8_t key = (row - 1) * 3 + col;
        codex_ui_key(key, record->event.pressed);
        codex_key(key, record->event.pressed);
    }
    return false;
}
bool encoder_update_user(uint8_t index, bool clockwise) {
    if (index == 0) codex_encoder(clockwise);
    return false;
}
