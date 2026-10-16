// SPDX-License-Identifier: GPL-2.0-or-later
#include QMK_KEYBOARD_H
#include "protocol.h"
#include "raw_hid.h"
#include "usb_device_state.h"
#include "src/display/eh_display.h"
#include <math.h>
#ifdef CODEX_HYBRID_ENABLE
#include "hybrid.h"
#include "ergohaven.h"
#include "src/eh_settings.h"
#include "src/display/eh_keycode_str.h"
#include "task_navigation.h"
#include "entropy_tasks.h"
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(eh_font_montserrat_20);
static lv_obj_t *layer_title;
static lv_obj_t *task_rows[6], *task_labels[6];
static codex_navigation_t navigation = {.held = -1};
static bool ui_dirty = true;
static uint32_t task_selection_time;
// LVGL's DOT mode can modify its text buffer. Keep the full title separately
// and invalidate only rows whose appearance changed, so SPI doesn't starve input.
static char task_displayed_titles[6][128];
static uint32_t task_backgrounds[6];
static bool task_selected[6], task_painted[6];
void codex_display_theme_changed(void);
void display_process_matrix_press(void);
extern void codex_hid_send(uint8_t *data, uint8_t length);
#endif

static lv_obj_t *screen;
#ifndef CODEX_HYBRID_ENABLE
static lv_obj_t *cells[12], *labels[12];
#endif
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
#ifdef CODEX_HYBRID_ENABLE
        navigation.held = -1;
        ui_dirty = true;
#endif
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
    lv_obj_set_style_text_font(title, &eh_font_montserrat_20, 0);
    lv_obj_set_width(title, 216);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(title, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(title, layer_name(get_highest_layer(layer_state | default_layer_state)));
#endif
#ifndef CODEX_HYBRID_ENABLE
    static const char *const names[12] = {"1", "2", "3", "4", "5", "6", "FAST", "OK", "NO", "NEW", "MIC", "SEND"};
    for (int i = 0; i < 12; ++i) {
        cells[i] = lv_obj_create(screen);
        lv_obj_set_size(cells[i], 68, 48);
        lv_obj_set_pos(cells[i], 12 + (i % 3) * 74, 48 + (i / 3) * 54);
        lv_obj_set_style_pad_all(cells[i], 0, 0);
        lv_obj_set_style_radius(cells[i], 6, 0);
        lv_obj_set_style_border_width(cells[i], 2, 0);
        lv_obj_set_style_bg_opa(cells[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(cells[i], lv_color_hex(0x142030), 0);
        lv_obj_clear_flag(cells[i], LV_OBJ_FLAG_SCROLLABLE);
        labels[i] = label_at(cells[i], names[i], 0, 0);
        lv_obj_set_style_text_font(labels[i], i < 6 ? &lv_font_montserrat_28 : &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_align(labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(labels[i]);
    }
#endif
#ifdef CODEX_HYBRID_ENABLE
    for (unsigned i = 0; i < 6; ++i) {
        task_rows[i] = lv_obj_create(screen);
        lv_obj_set_size(task_rows[i], 216, 35);
        lv_obj_set_pos(task_rows[i], 12, 36 + i * 39);
        lv_obj_set_style_pad_all(task_rows[i], 0, 0);
        lv_obj_set_style_radius(task_rows[i], 3, 0);
        lv_obj_set_style_border_width(task_rows[i], 1, 0);
        lv_obj_set_style_border_color(task_rows[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(task_rows[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(task_rows[i], LV_OBJ_FLAG_SCROLLABLE);
        char text[32];
        // Native Micro packets contain slot lighting, not task titles.
        snprintf(text, sizeof(text), "Задача %u", i + 1);
        task_labels[i] = label_at(task_rows[i], text, 0, 0);
        lv_obj_set_style_text_font(task_labels[i], &eh_font_montserrat_20, 0);
        lv_obj_set_width(task_labels[i], 196);
        lv_obj_align(task_labels[i], LV_ALIGN_LEFT_MID, 8, 0);
        lv_label_set_long_mode(task_labels[i], LV_LABEL_LONG_DOT);
    }
    codex_display_theme_changed();
    display_process_matrix_press();
#else
    lv_scr_load(screen);
    display_turn_on();
#endif
}
static uint32_t light_color(codex_light_t light) {
    if (!light.effect || !light.brightness) return 0;
    // Preserve status hue; Entropy controls notification brightness too.
    // Zero host brightness still represents a blink's off phase.
    unsigned r = (light.color >> 16) & 255, g = (light.color >> 8) & 255, b = light.color & 255;
    unsigned peak = r > g ? r : g;
    if (b > peak) peak = b;
    if (!peak) return 0;
#ifdef CODEX_HYBRID_ENABLE
    unsigned gain = rgb_matrix_get_val();
#else
    unsigned gain = 255;
#endif
    return ((r * gain / peak) << 16) | ((g * gain / peak) << 8) | (b * gain / peak);
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
void codex_ui_select_task(uint8_t slot) {
    if (slot >= CODEX_SLOT_COUNT) return;
    display_process_matrix_press();
    navigation.selected = slot;
    entropy_tasks_interaction();
    task_selection_time = timer_read32();
    ui_dirty = true;
}
void codex_ui_rotate_task(bool clockwise) {
    display_process_matrix_press();
    for (unsigned i = 0; i < 6; ++i) {
        codex_navigation_rotate(&navigation, clockwise);
        if (!entropy_tasks_active() || entropy_tasks_occupied(navigation.selected)) break;
    }
    entropy_tasks_interaction();
    task_selection_time = timer_read32();
    ui_dirty = true;
}
bool codex_ui_task_held(void) { return navigation.held >= 0; }
void codex_ui_open_task(bool pressed) {
    if (pressed) display_process_matrix_press();
    int8_t slot = codex_navigation_press(&navigation, pressed);
    if (slot >= 0) codex_key(slot, pressed);
}
static void task_list_render(uint32_t now) {
    for (unsigned i = 0; i < CODEX_SLOT_COUNT; ++i) {
        codex_light_t light = codex_slots()[i];
        codex_light_t notification;
        if (codex_slot_notification_light(i, now, &notification)) light = notification;
        uint32_t background = (codex_seen_host() || entropy_tasks_active()) && light.effect && light.brightness ? light.color : (lv_color_to32(lv_color_mix(accent_color_blue, display_background_color, 24)) & 0xFFFFFF);
        char fallback[32]; snprintf(fallback, sizeof(fallback), "Задача %u", i + 1);
        const char *title = entropy_tasks_active() ? entropy_tasks_title(i) : fallback;
        char display[128];
        if (entropy_tasks_active()) snprintf(display, sizeof(display), "%u  %s", i + 1, title[0] ? title : "—");
        else snprintf(display, sizeof(display), "%s", title);
        if (strcmp(task_displayed_titles[i], display)) {
            memcpy(task_displayed_titles[i], display, strlen(display) + 1);
            lv_label_set_text(task_labels[i], display);
            if (i == navigation.selected) task_selection_time = now;
        }
        lv_label_long_mode_t mode = i == navigation.selected && now-task_selection_time >= 800 ? LV_LABEL_LONG_SCROLL_CIRCULAR : LV_LABEL_LONG_DOT;
        if (lv_label_get_long_mode(task_labels[i]) != mode) lv_label_set_long_mode(task_labels[i], mode);
        if (!task_painted[i] || task_backgrounds[i] != background) {
            uint32_t foreground = text_color(background);
            lv_obj_set_style_bg_color(task_rows[i], lv_color_hex(background), 0);
            lv_obj_set_style_text_color(task_labels[i], lv_color_hex(foreground), 0);
            lv_obj_set_style_border_color(task_rows[i], lv_color_hex(foreground), 0);
            task_backgrounds[i] = background;
        }
        bool selected = i == navigation.selected;
        if (!task_painted[i] || task_selected[i] != selected) {
            lv_obj_set_style_border_opa(task_rows[i], selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
            task_selected[i] = selected;
        }
        task_painted[i] = true;
    }
}
void codex_display_theme_changed(void) {
    if (!screen) return;
    lv_obj_set_style_bg_color(screen, display_background_color, 0);
    lv_obj_set_style_text_color(layer_title, accent_color_blue, 0);
    memset(task_painted, 0, sizeof(task_painted));
    ui_dirty = true;
}
static void codex_screen_render(void) {
    uint32_t now = timer_read32();
    if (!screen || (!ui_dirty && now - render_time < 16)) return;
    render_time = now;
    ui_dirty = false;
    const char *heading = layer_name(0);
    if (strcmp(lv_label_get_text(layer_title), heading) != 0)
        lv_label_set_text(layer_title, heading);
    task_list_render(now);
}
static void codex_screen_load(void) {
    if (!screen) return;
    ui_dirty = true;
    codex_screen_render();
    lv_scr_load(screen);
    display_apply_brightness();
}
const eh_screen_t eh_screen_codex = {.load = codex_screen_load, .housekeep = codex_screen_render};
#endif
void codex_housekeeping(void) {
    bool configured = usb_device_state_get_configure_state() == USB_DEVICE_STATE_CONFIGURED;
    if (was_configured && !configured) { codex_reset(); pressed_keys = 0; }
    was_configured = configured;
    uint32_t now = timer_read32();
    codex_tick(now);
#ifdef CODEX_HYBRID_ENABLE
    // Transport and notifications keep running while a stock/standby screen is visible.
    entropy_tasks_tick(now);
#else
    if (!screen || now - render_time < 100) return;
    render_time = now;
    for (unsigned i = 0; i < 12; ++i) {
        codex_light_t light = i < 6 ? codex_slots()[i] : codex_key_light(i);
        bool task = i < 6;
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
#endif
}
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    uint32_t now = timer_read32();
    codex_light_t notification;
    bool notifying = codex_notification_light(now, &notification);
#ifdef CODEX_HYBRID_ENABLE
    RGB base = hsv_to_rgb(rgb_matrix_get_hsv());
    bool codex_layer = get_highest_layer(layer_state | default_layer_state) == 0;
#endif
    for (unsigned i = 0; i < 12; ++i) {
        uint8_t led = physical[i];
        if (led < led_min || led >= led_max) continue;
        // Notification overrides every physical LED, including remapped keys
        // and non-Codex layers. Normal RGB resumes after the fifth pulse.
        if (notifying) {
            uint32_t color = light_color(notification);
            rgb_matrix_set_color(led, color >> 16, color >> 8, color);
            continue;
        }
#ifdef CODEX_HYBRID_ENABLE
        uint16_t code = codex_hybrid_keycode(i);
        if (!codex_layer && (code < CD_TASK1 || code > CD_SEND)) continue;
        // Steady user-selected color between notifications, even for empty slots.
        uint32_t color = ((uint32_t)base.r << 16) | ((uint32_t)base.g << 8) | base.b;
        if (codex_layer && code >= CD_TASK1 && code <= CD_TASK6 && entropy_tasks_completed(code - CD_TASK1))
            color = light_color(codex_slots()[code - CD_TASK1]);
#else
        uint32_t color = light_color(codex_animated_key_light(i, now));
#endif
        rgb_matrix_set_color(led, color >> 16, color >> 8, color);
    }
    return false;
}
