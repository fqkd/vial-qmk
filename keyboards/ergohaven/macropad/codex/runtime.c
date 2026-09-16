// SPDX-License-Identifier: GPL-2.0-or-later
#include QMK_KEYBOARD_H
#include "protocol.h"
#include "raw_hid.h"
#include "usb_device_state.h"
#include "src/display/eh_display.h"
#include <stdio.h>

static lv_obj_t *screen, *status, *cells[6];
static bool was_configured;
static uint32_t render_time;
void notify_usb_device_state_change_user(struct usb_device_state state) {
    if (state.configure_state != USB_DEVICE_STATE_CONFIGURED) codex_reset();
}
static void transmit(const uint8_t report[64]) {
    if (usb_device_state_get_configure_state() == USB_DEVICE_STATE_CONFIGURED)
        raw_hid_send((uint8_t *)report, 64);
}
void codex_receive_report(uint8_t *data, uint8_t length) {
    codex_receive(data, length, timer_read32());
}
void codex_setup(void) {
    codex_init(transmit);
    rgb_matrix_enable_noeeprom();
    rgb_matrix_mode_noeeprom(RGB_MATRIX_SOLID_COLOR);
    rgb_matrix_sethsv_noeeprom(0, 0, 0);
    if (!display_init_kb()) return;
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "CODEX / TEST");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);
    status = lv_label_create(screen);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_20, 0);
    lv_label_set_text(status, "Waiting for host");
    lv_obj_align(status, LV_ALIGN_TOP_MID, 0, 45);
    for (int i = 0; i < 6; ++i) {
        cells[i] = lv_obj_create(screen);
        lv_obj_set_size(cells[i], 67, 65);
        lv_obj_set_pos(cells[i], 12 + (i % 3) * 74, 95 + (i / 3) * 74);
        lv_obj_set_style_bg_opa(cells[i], LV_OPA_COVER, 0);
        lv_obj_t *label = lv_label_create(cells[i]);
        char text[2] = {(char)('1' + i), 0};
        lv_label_set_text(label, text);
        lv_obj_center(label);
    }
    lv_scr_load(screen);
    display_turn_on();
}
void codex_housekeeping(void) {
    bool configured = usb_device_state_get_configure_state() == USB_DEVICE_STATE_CONFIGURED;
    if (was_configured && !configured) codex_reset();
    was_configured = configured;
    uint32_t now = timer_read32();
    codex_tick(now);
    if (!screen || now - render_time < 100) return;
    render_time = now;
    // Host messages are event-driven: silence is not proof of disconnection.
    char text[40];
    if (!configured) snprintf(text, sizeof(text), "USB inactive");
    else if (!codex_seen_host()) snprintf(text, sizeof(text), "Waiting for host");
    else snprintf(text, sizeof(text), "RX %lus ago", (unsigned long)((now - codex_last_rx()) / 1000));
    lv_label_set_text(status, text);
    for (int i = 0; i < 6; ++i) {
        const codex_light_t *l = &codex_slots()[i];
        lv_obj_set_style_bg_color(cells[i], lv_color_hex(l->effect && l->brightness ? l->color : 0x101010), 0);
    }
}
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    // rev3 LED chain snakes across the physical rows.
    static const uint8_t physical[12] = {0,1,2,5,4,3,6,7,8,11,10,9};
    uint32_t now = timer_read32();
    for (unsigned i = 0; i < 12; ++i) {
        uint8_t led = physical[i];
        if (led < led_min || led >= led_max) continue;
        const codex_light_t *l = i < 6 ? &codex_slots()[i] : codex_keys_light();
        unsigned gain = l->effect ? l->brightness : 0;
        // Limit current; approximate breathing effects without flash writes.
        if (gain > 100) gain = 100;
        if (l->effect == 4 || l->effect == 6) {
            unsigned phase = (now % 2000) * 255 / 2000;
            unsigned wave = phase < 128 ? phase * 2 : (255 - phase) * 2;
            gain = gain * (l->effect == 6 ? 128 + wave / 2 : wave) / 255;
        }
        rgb_matrix_set_color(led, ((l->color >> 16) & 255) * gain / 255, ((l->color >> 8) & 255) * gain / 255, (l->color & 255) * gain / 255);
    }
    return false;
}
