#include "display.h"
#include "ergohaven.h"
#include "ergohaven_display.h"

void housekeeping_task_user(void) {
    if (is_display_enabled()) {
        display_housekeeping_task();
    }
}

void keyboard_post_init_user(void) {
    display_init_kb();
}
