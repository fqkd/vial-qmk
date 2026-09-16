// SPDX-License-Identifier: GPL-2.0-or-later
#include QMK_KEYBOARD_H
#include "protocol.h"
#include "raw_hid.h"
#include "usb_device_state.h"
#include "src/display/eh_display.h"
#include <math.h>
#ifdef CODEX_HYBRID_ENABLE
#include "hybrid.h"
#include <stdio.h>
static lv_obj_t *layer_title;
extern void codex_hid_send(uint8_t *data, uint8_t length);
#endif

static lv_obj_t *screen, *cells[12], *labels[12];
static bool was_configured;
static uint16_t pressed_keys;
static uint32_t render_time;
static const uint8_t physical[12] = {0,1,2,5,4,3,6,7,8,11,10,9};
_Static_assert(RGB_MATRIX_LED_COUNT == 12, "This layout requires Macropad rev3's 12 LEDs");

void codex_ui_key(uint8_t key, bool pressed) {
    if (key >= 12) return;
    if (pressed) pressed_keys |= 1u << key;
    else pressed_keys &= ~(1u << key);
}
void notify_usb_device_state_change_user(struct usb_device_state state) {
    if (state.configure_state != USB_DEVICE_STATE_CONFIGURED) {
        codex_reset(); pressed_keys = 0;
    }
}
static void transmit(const uint8_t report[64]) {
    if (usb_device_state_get_configure_state() == USB_DEVICE_STATE_CONFIGURED)
#ifdef CODEX_HYBRID_ENABLE
        codex_hid_send((uint8_t *)report, 64);
#else
        raw_hid_send((uint8_t *)report, 64);
#endif
}
void codex_receive_report(uint8_t *data, uint8_t length) {
    codex_receive(data, length, timer_read32());
}
static lv_obj_t *label_at(lv_obj_t *parent, const char *text, int x, int y) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_color(label, lv_color_hex(0xE8EEF5), 0);
    return label;
}
void codex_setup(void) {
    codex_init(transmit);
#ifndef CODEX_HYBRID_ENABLE
    rgb_matrix_enable_noeeprom();
    rgb_matrix_mode_noeeprom(RGB_MATRIX_SOLID_COLOR);
    rgb_matrix_sethsv_noeeprom(0, 0, 0);
#endif
    if (!display_init_kb()) return;
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x080E16), 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *title = label_at(screen, "Macropad", 0, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);
#ifdef CODEX_HYBRID_ENABLE
    layer_title = title;
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
#endif
    static const char *const names[12] = {"1", "2", "3", "4", "5", "6", "FAST", "OK", "NO", "NEW", "MIC", "SEND"};
    for (int i = 0; i < 12; ++i) {
        cells[i] = lv_obj_create(screen);
        lv_obj_set_size(cells[i], 68, i < 6 ? 58 : 42);
        lv_obj_set_pos(cells[i], 12 + (i % 3) * 74, i < 6 ? 48 + (i / 3) * 64 : 180 + ((i - 6) / 3) * 48);
        lv_obj_set_style_pad_all(cells[i], 0, 0);
        lv_obj_set_style_radius(cells[i], 6, 0);
        lv_obj_set_style_border_width(cells[i], 2, 0);
        lv_obj_set_style_bg_opa(cells[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(cells[i], lv_color_hex(0x142030), 0);
        lv_obj_clear_flag(cells[i], LV_OBJ_FLAG_SCROLLABLE);
        labels[i] = label_at(cells[i], names[i], 0, 0);
        if (i < 6) lv_obj_set_style_text_font(labels[i], &lv_font_montserrat_40, 0);
        lv_obj_center(labels[i]);
    }
    lv_scr_load(screen);
    display_turn_on();
}
static uint32_t light_color(codex_light_t light, unsigned key, uint32_t now) {
    if (!light.effect || !light.brightness) return 0;
    // Proportional 0..100 brightness (not a clamp that loses the upper range).
    unsigned gain = ((unsigned)light.brightness * 100 + 127) / 255;
    unsigned phase = light.speed ? ((now % 60000) * (unsigned)light.speed / 600) % 256 : 0;
    unsigned wave = phase < 128 ? phase * 2 : (255 - phase) * 2;
    uint32_t color = light.color;
    if (light.effect == 4) gain = gain * wave / 255;
    else if (light.effect == 6) gain = gain * (128 + wave / 2) / 255;
    else if (light.effect == 2) {
        unsigned distance = (key + 12 - phase * 12 / 256) % 12;
        gain = gain * (distance == 0 ? 255 : distance == 1 ? 100 : 24) / 255;
    } else if (light.effect == 3 || light.effect == 5) {
        HSV hsv = {light.effect == 3 ? phase : (phase + key * 21) % 256, 255, 255};
        RGB rgb = hsv_to_rgb(hsv);
        color = ((uint32_t)rgb.r << 16) | ((uint32_t)rgb.g << 8) | rgb.b;
    }
    return ((((color >> 16) & 255) * gain / 255) << 16) |
           ((((color >> 8) & 255) * gain / 255) << 8) | ((color & 255) * gain / 255);
}
static float linear_channel(uint8_t channel) {
    float c = channel / 255.0f;
    return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
}
static uint32_t text_color(uint32_t background) {
    float luminance = 0.2126f * linear_channel(background >> 16) +
                      0.7152f * linear_channel(background >> 8) +
                      0.0722f * linear_channel(background);
    // Choose whichever of pure black and white gives the higher contrast.
    return luminance > 0.179f ? 0x000000 : 0xFFFFFF;
}
#ifdef CODEX_HYBRID_ENABLE
static void hybrid_label(uint16_t code, char text[12]) {
    static const char *const names[] = {"1", "2", "3", "4", "5", "6", "FAST", "OK", "NO", "NEW", "MIC", "SEND", "DIAL", "CCW", "CW", "MODE"};
    if (code >= CD_TASK1 && code <= CD_MODE) snprintf(text, 12, "%s", names[code - CD_TASK1]);
    else if (code >= KC_A && code <= KC_Z) snprintf(text, 12, "%c", 'A' + code - KC_A);
    else if (code >= KC_1 && code <= KC_9) snprintf(text, 12, "%c", '1' + code - KC_1);
    else if (code == KC_0) snprintf(text, 12, "0");
    else if (code >= KC_F1 && code <= KC_F12) snprintf(text, 12, "F%u", code - KC_F1 + 1);
    else if (code >= KC_F13 && code <= KC_F24) snprintf(text, 12, "F%u", code - KC_F13 + 13);
    else {
        const char *name = "KEY";
        switch (code) {
            case KC_NO: name = "-"; break;
            case KC_ENTER: name = "ENT"; break;
            case KC_DOT: name = "."; break;
            case KC_UP: name = "UP"; break;
            case KC_DOWN: name = "DN"; break;
            case KC_LEFT: name = "LEFT"; break;
            case KC_RIGHT: name = "RIGHT"; break;
            case KC_HOME: name = "HOME"; break;
            case KC_END: name = "END"; break;
            case KC_DEL: name = "DEL"; break;
            case KC_PGUP: name = "PGUP"; break;
            case KC_PGDN: name = "PGDN"; break;
            case C(KC_X): name = "CUT"; break;
            case C(KC_C): name = "COPY"; break;
            case C(KC_V): name = "PASTE"; break;
            case KC_MPRV: name = "PREV"; break;
            case KC_MPLY: name = "PLAY"; break;
            case KC_MNXT: name = "NEXT"; break;
        }
        snprintf(text, 12, "%s", name);
    }
}
#endif
void codex_housekeeping(void) {
    bool configured = usb_device_state_get_configure_state() == USB_DEVICE_STATE_CONFIGURED;
    if (was_configured && !configured) { codex_reset(); pressed_keys = 0; }
    was_configured = configured;
    uint32_t now = timer_read32();
    codex_tick(now);
    if (!screen || now - render_time < 100) return;
    render_time = now;
#ifdef CODEX_HYBRID_ENABLE
    char heading[32];
    snprintf(heading, sizeof(heading), "Macropad / %u", get_highest_layer(layer_state | default_layer_state));
    lv_label_set_text(layer_title, heading);
#endif
    for (unsigned i = 0; i < 12; ++i) {
#ifdef CODEX_HYBRID_ENABLE
        uint16_t code = codex_hybrid_keycode(i);
        bool task = code >= CD_TASK1 && code <= CD_TASK6;
        codex_light_t light = task ? codex_slots()[code - CD_TASK1] : (code >= CD_FAST && code <= CD_SEND ? codex_key_light(code - CD_TASK1) : (codex_light_t){0});
        char name[12]; hybrid_label(code, name);
        lv_label_set_text(labels[i], name);
        lv_obj_set_style_text_font(labels[i], task ? &lv_font_montserrat_40 : &lv_font_montserrat_20, 0);
        lv_obj_center(labels[i]);
#else
        codex_light_t light = i < 6 ? codex_slots()[i] : codex_key_light(i);
        bool task = i < 6;
#endif
        uint32_t color = light.effect && light.brightness ? light.color : 0x344153;
        bool pressed = pressed_keys & (1u << i);
        uint32_t background = task && light.effect && light.brightness ? light.color : 0x142030;
        if (!task && pressed) background = 0x30445C;
        uint32_t foreground = text_color(background);
        lv_obj_set_style_border_color(cells[i], lv_color_hex(pressed ? foreground : color), 0);
        lv_obj_set_style_border_width(cells[i], pressed ? 4 : 2, 0);
        lv_obj_set_style_bg_color(cells[i], lv_color_hex(background), 0);
        lv_obj_set_style_text_color(labels[i], lv_color_hex(foreground), 0);
    }
}
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    uint32_t now = timer_read32();
    for (unsigned i = 0; i < 12; ++i) {
        uint8_t led = physical[i];
        if (led < led_min || led >= led_max) continue;
#ifdef CODEX_HYBRID_ENABLE
        uint16_t code = codex_hybrid_keycode(i);
        if (code < CD_TASK1 || code > CD_SEND) continue; // Keep QMK/Vial RGB for ordinary keys.
        uint32_t color = light_color(codex_animated_key_light(code - CD_TASK1, now), i, now);
#else
        uint32_t color = light_color(codex_animated_key_light(i, now), i, now);
#endif
        rgb_matrix_set_color(led, color >> 16, color >> 8, color);
    }
    return false;
}
