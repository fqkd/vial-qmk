// Copyright 2026 Ergohaven (@ergohaven)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rev1.h"
#include QMK_KEYBOARD_H
#include "ergohaven.h"

// phenom-mini-v0.0.7: align Phenom Vial halves after removing top row; Velvet v3 keycode layout.
const char chordal_hold_layout[MATRIX_ROWS][MATRIX_COLS] PROGMEM = LAYOUT(
    'L', 'L', 'L', 'L', 'L', 'L',                'R', 'R', 'R', 'R', 'R', 'R',
    'L', 'L', 'L', 'L', 'L', 'L',                'R', 'R', 'R', 'R', 'R', 'R',
    'L', 'L', 'L', 'L', 'L', 'L',                'R', 'R', 'R', 'R', 'R', 'R',
              'L', 'L', '*', '*', '*',      '*', '*', '*', 'R', 'R',
                                  '*',      '*'
);
