// SPDX-License-Identifier: GPL-2.0-or-later
#include <assert.h>
#include "task_navigation.h"
int main(void) {
    codex_navigation_t nav = {.held = -1};
    codex_navigation_rotate(&nav, false);
    assert(nav.selected == 5);
    codex_navigation_rotate(&nav, true);
    assert(nav.selected == 0);
    assert(codex_navigation_press(&nav, false) == -1);
    assert(codex_navigation_press(&nav, true) == 0);
    assert(codex_navigation_press(&nav, true) == -1);
    codex_navigation_rotate(&nav, true);
    assert(nav.selected == 1);
    assert(codex_navigation_press(&nav, false) == 0);
    assert(codex_navigation_press(&nav, true) == 1);
    assert(codex_navigation_press(&nav, false) == 1);
    return 0;
}
