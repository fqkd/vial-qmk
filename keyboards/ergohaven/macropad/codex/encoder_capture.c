// SPDX-License-Identifier: GPL-2.0-or-later
#include QMK_KEYBOARD_H
#include "hal.h"
#include "encoder_capture.h"

static codex_encoder_capture_t capture;

static void encoder_edge(void *arg) {
    (void)arg;
    // Macropad rev3: A = GP24, B = GP25. Read both pins in one operation.
    codex_encoder_capture_edge(&capture, (palReadPort(IOPORT1) >> 24) & 3);
}

void encoder_quadrature_post_init_kb(void) {
    // The standard driver has already enabled input pull-ups.
    codex_encoder_capture_init(&capture, (palReadPort(IOPORT1) >> 24) & 3);
    palSetLineCallback(GP24, encoder_edge, NULL);
    palSetLineCallback(GP25, encoder_edge, NULL);
    palEnableLineEvent(GP24, PAL_EVENT_MODE_BOTH_EDGES);
    palEnableLineEvent(GP25, PAL_EVENT_MODE_BOTH_EDGES);
}

void encoder_driver_task(void) {
    // USB/key handling stays on the main loop. GPIO IRQs only capture rotation,
    // including while LVGL flushes the screen over SPI.
    int8_t direction;
    while ((direction = codex_encoder_capture_peek(&capture)) != 0) {
        bool clockwise = direction > 0;
#ifdef ENCODER_DIRECTION_FLIP
        clockwise = !clockwise;
#endif
        if (!encoder_queue_event(0, clockwise)) break;
        codex_encoder_capture_pop(&capture);
    }
}
