// SPDX-License-Identifier: GPL-2.0-or-later
#include "layer_button.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    codex_layer_button_t s = {0};
    assert(!codex_layer_button_event(&s, true, 0));
    assert(!codex_layer_button_tick(&s, 1000)); // Hold never changes layer.
    assert(!codex_layer_button_event(&s, false, 1000));
    assert(!codex_layer_button_tick(&s, 1249));
    assert(codex_layer_button_tick(&s, 1250) == 1);
    assert(!codex_layer_button_tick(&s, 1500));
    assert(!codex_layer_button_event(&s, true, 2000));
    assert(!codex_layer_button_event(&s, false, 2010));
    assert(!codex_layer_button_tick(&s, 2259));
    assert(!codex_layer_button_event(&s, true, 2259));
    assert(!codex_layer_button_tick(&s, 3000));
    assert(codex_layer_button_event(&s, false, 3001) == -1);
    assert(!codex_layer_button_tick(&s, 4000));
    assert(!codex_layer_button_event(&s, false, 4010)); // Stray release.
    assert(!codex_layer_button_event(&s, true, UINT32_MAX - 200));
    assert(!codex_layer_button_event(&s, false, UINT32_MAX - 100));
    assert(!codex_layer_button_tick(&s, 148));
    assert(codex_layer_button_tick(&s, 149) == 1);
    for (uint8_t i = 0; i < 5; ++i) {
        assert(codex_layer_destination(i, 1) == (i + 1) % 5);
        assert(codex_layer_destination(i, -1) == (i + 4) % 5);
    }
    assert(codex_layer_destination(15, -1) == 0);
    puts("Layer button tests passed");
}
