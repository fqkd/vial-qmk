// Copyright 2026 Ergohaven (@ergohaven)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rev1.h"
#include QMK_KEYBOARD_H
#include "ergohaven.h"

// phenom-micro-layout-v0.0.1: chordal hold map follows the 42-key Micro layout.
const char chordal_hold_layout[MATRIX_ROWS][MATRIX_COLS] PROGMEM = LAYOUT(
    'L', 'L', 'L', 'L', 'L',                'R', 'R', 'R', 'R', 'R',
    'L', 'L', 'L', 'L', 'L',                'R', 'R', 'R', 'R', 'R',
    'L', 'L', 'L', 'L', 'L',                'R', 'R', 'R', 'R', 'R',
              'L', 'L', '*', '*', '*',      '*', '*', '*', 'R', 'R',
                                  '*',      '*'
);
