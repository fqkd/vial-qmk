// Copyright 2022 Ergohaven (@ergohaven)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rev1.h"

bool vial_unlock_combo_active(void) {
    const bool left_unlock  = matrix_is_on(1, 5) && matrix_is_on(1, 4);  // Esc + Q
    const bool right_unlock = matrix_is_on(6, 5) && matrix_is_on(6, 4);  // mirrored right pair

    return left_unlock || right_unlock;
}

void vial_get_unlock_combo_coords(uint8_t *rows, uint8_t *cols, size_t count) {
    if (count < 2) {
        return;
    }

    const bool left_side = is_keyboard_left();

    rows[0] = left_side ? 1 : 6;
    cols[0] = 5;
    rows[1] = left_side ? 1 : 6;
    cols[1] = 4;
}
#include QMK_KEYBOARD_H
#include "ergohaven.h"

const char chordal_hold_layout[MATRIX_ROWS][MATRIX_COLS] PROGMEM = LAYOUT(
'L', 'L', 'L', 'L', 'L', 'L',                'R', 'R', 'R', 'R', 'R', 'R',
'L', 'L', 'L', 'L', 'L', 'L',                'R', 'R', 'R', 'R', 'R', 'R',
'L', 'L', 'L', 'L', 'L', 'L',                'R', 'R', 'R', 'R', 'R', 'R',
'L', 'L', 'L', 'L', 'L', 'L',                'R', 'R', 'R', 'R', 'R', 'R',
          'L', 'L', '*', '*', '*',      '*', '*', '*', 'R', 'R',
                              '*',      '*'

);
