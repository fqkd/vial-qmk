// SPDX-License-Identifier: GPL-2.0-or-later
#include "layer_button.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    // Each next-key press advances once; two rapid presses advance twice.
    assert(codex_layer_destination(codex_layer_destination(0, 1), 1) == 2);
    for (uint8_t i = 0; i < 5; ++i) {
        assert(codex_layer_destination(i, 1) == (i + 1) % 5);
        assert(codex_layer_destination(i, -1) == (i + 4) % 5);
    }
    assert(codex_layer_destination(15, -1) == 0);
    puts("Layer button tests passed");
}
