#include "ergohaven_display.h"
#include "gpio.h"
#include "hid.h"
#include "lvgl_helpers.h"
#include "qp.h"
#include "ergohaven_ruen.h"
#include "ergohaven_symbols.h"
#include "ergohaven.h"

LV_FONT_DECLARE(ergohaven_symbols_20);
LV_FONT_DECLARE(ergohaven_symbols_28);

painter_device_t display;

/* shared styles */
lv_color_t accent_color_red;
lv_color_t accent_color_blue;

lv_style_t style_screen;
lv_style_t style_container;
lv_style_t style_button;
lv_style_t style_button_active;

static bool display_enabled = false;

bool is_display_enabled(void) {
    return display_enabled;
}

__attribute__((weak)) void display_init_styles_kb(void) {
    accent_color_red       = lv_color_make(248, 83, 107);
    accent_color_blue      = lv_color_make(84, 189, 191);
    lv_disp_t  *lv_display = lv_disp_get_default();
    lv_theme_t *lv_theme   = lv_theme_default_init(lv_display, accent_color_blue, accent_color_red, true, LV_FONT_DEFAULT);
    lv_disp_set_theme(lv_display, lv_theme);

    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, lv_color_black());

    lv_style_init(&style_container);
    lv_style_set_pad_top(&style_container, 0);
    lv_style_set_pad_bottom(&style_container, 0);
    lv_style_set_pad_left(&style_container, 0);
    lv_style_set_pad_right(&style_container, 0);
    lv_style_set_bg_opa(&style_container, 0);
    lv_style_set_border_width(&style_container, 0);
    lv_style_set_width(&style_container, lv_pct(100));
    lv_style_set_height(&style_container, LV_SIZE_CONTENT);

    lv_style_init(&style_button);
    lv_style_set_pad_top(&style_button, 4);
    lv_style_set_pad_bottom(&style_button, 4);
    lv_style_set_pad_left(&style_button, 4);
    lv_style_set_pad_right(&style_button, 4);
    lv_style_set_radius(&style_button, 6);
    lv_style_set_text_color(&style_button, accent_color_blue);

    lv_style_init(&style_button_active);
    lv_style_set_bg_color(&style_button_active, accent_color_blue);
    lv_style_set_bg_opa(&style_button_active, LV_OPA_100);
    lv_style_set_text_color(&style_button_active, lv_color_black());
}

__attribute__((weak)) void display_init_screens_kb(void);

bool display_init_kb(void) {
    display_enabled = false;
    dprint("display_init_kb - start\n");

    backlight_init();

    display = qp_st7789_make_spi_device(240, 280, LCD_CS_PIN, LCD_DC_PIN, LCD_RST_PIN, 4, 3);
    qp_set_viewport_offsets(display, 0, 20);

    if (!qp_init(display, QP_ROTATION_180) || !qp_lvgl_attach(display)) return display_enabled;

    display_enabled = true;
    dprint("display_init_kb - initialised\n");

    display_init_styles_kb();
    display_init_screens_kb();

    return display_enabled;
}

static bool is_display_on = false;

void display_turn_on(void) {
    if (!is_display_on) {
        backlight_init();
        qp_power(display, true);
        is_display_on = true;
    }
}

void display_turn_off(void) {
    if (is_display_on) {
        is_display_on = false;
        qp_power(display, false);
        backlight_level_noeeprom(0);
    }
}

/* Common helpers */

const char *get_layer_label(uint8_t layer) {
    const char *layer_name = layer_upper_name(layer);
    static char buf[32];
    sprintf(buf, EH_SYMBOL_LAYER " %s", layer_name);
    return buf;
}

const char *get_layout_label(uint8_t layout) {
    switch (layout) {
        default:
        case LANG_EN:
            return EH_SYMBOL_GLOBE " EN";
            break;

        case LANG_RU:
            return EH_SYMBOL_GLOBE " RU";
            break;
    }
}

/* Screen splash */

static lv_obj_t *screen_splash;
static lv_obj_t *label_version;

LV_IMG_DECLARE(ergohaven_logo);

void splash_screen_init(void) {
    screen_splash = lv_obj_create(NULL);
    lv_obj_add_style(screen_splash, &style_screen, 0);
    use_flex_column(screen_splash);

    lv_obj_t *img = lv_img_create(screen_splash);
    lv_img_set_src(img, &ergohaven_logo);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_top(img, 60, 0);
    lv_obj_set_style_pad_bottom(img, 60, 0);

    label_version = lv_label_create(screen_splash);
    lv_label_set_text(label_version, "v" EH_VERSION_STR);
}

void splash_screen_load(void) {
    lv_scr_load(screen_splash);
}

void splash_screen_housekeep(void) {
    lv_scr_load(screen_splash);
}

const eh_screen_t eh_screen_splash = {
    .init      = splash_screen_init,
    .load      = splash_screen_load,
    .housekeep = splash_screen_housekeep,
};

/* Screen volume */

static lv_obj_t *screen_volume;
static lv_obj_t *arc_volume;
static lv_obj_t *label_volume_arc;

void screen_volume_init(void) {
    screen_volume = lv_obj_create(NULL);
    lv_obj_add_style(screen_volume, &style_screen, 0);

    arc_volume = lv_arc_create(screen_volume);
    lv_obj_set_size(arc_volume, 200, 200);
    lv_obj_center(arc_volume);

    label_volume_arc = lv_label_create(screen_volume);
    lv_label_set_text(label_volume_arc, "00");
    lv_obj_set_style_text_font(label_volume_arc, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_center(label_volume_arc);

    lv_obj_t *volume_text_label = lv_label_create(screen_volume);
    lv_label_set_text(volume_text_label, "Volume");
    lv_obj_align(volume_text_label, LV_ALIGN_BOTTOM_MID, 0, -50);
}

void screen_volume_load(void) {
    lv_scr_load(screen_volume);
}

void screen_volume_housekeep(void) {
    static uint8_t prev_volume = -1;
    hid_data_t    *hid         = get_hid_data();
    if (hid->volume != prev_volume) {
        prev_volume = hid->volume;
        lv_arc_set_value(arc_volume, hid->volume);
        lv_label_set_text_fmt(label_volume_arc, "%02d", hid->volume);
    }
    hid->volume_changed = false;
}

const eh_screen_t eh_screen_volume = {
    .init      = screen_volume_init,
    .load      = screen_volume_load,
    .housekeep = screen_volume_housekeep,
};

/* Screen home */

static lv_obj_t *screen_home;
static lv_obj_t *label_product;
static lv_obj_t *label_time;
static lv_obj_t *label_layer;
static lv_obj_t *label_mac;
static lv_obj_t *label_layout;
static lv_obj_t *label_shift;
static lv_obj_t *label_ctrl;
static lv_obj_t *label_alt;
static lv_obj_t *label_gui;
static lv_obj_t *label_num;
static lv_obj_t *label_caps;
static lv_obj_t *label_scroll;
static lv_obj_t *label_hid_media_artist;
static lv_obj_t *label_hid_media_title;
static lv_obj_t *screen_home_mods;
static lv_obj_t *screen_home_media;

void screen_home_init(void) {
    screen_home = lv_obj_create(NULL);
    lv_obj_add_style(screen_home, &style_screen, 0);

    label_product = lv_label_create(screen_home);
    lv_obj_set_style_text_font(label_product, &lv_font_montserrat_40, LV_PART_MAIN);
    lv_obj_set_style_text_align(label_product, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label_product, 0, 40);
    lv_obj_set_size(label_product, 240, 50);
    lv_label_set_text(label_product, EH_SHORT_PRODUCT_NAME);

    label_time = lv_label_create(screen_home);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_align(label_time, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label_time, 0, 36);
    lv_obj_set_size(label_time, 240, 50);
    lv_label_set_text(label_time, "00:00");
    lv_obj_add_flag(label_time, LV_OBJ_FLAG_HIDDEN);

    label_layer = lv_label_create(screen_home);
    lv_label_set_text(label_layer, "");
    lv_obj_set_pos(label_layer, 0, 100);
    lv_obj_set_size(label_layer, 110, 20);
    lv_obj_set_style_text_align(label_layer, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(label_layer, &ergohaven_symbols_20, LV_PART_MAIN);

    label_mac = lv_label_create(screen_home);
    lv_label_set_text(label_mac, EH_SYMBOL_MAC);
    lv_obj_set_pos(label_mac, 110, 100);
    lv_obj_set_size(label_mac, 46, 20);
    lv_obj_set_style_text_align(label_mac, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_mac, &ergohaven_symbols_20, LV_PART_MAIN);
    toggle_hidden(label_mac, false);

    label_layout = lv_label_create(screen_home);
    lv_label_set_text(label_layout, "");
    lv_obj_set_pos(label_layout, 160, 100);
    lv_obj_set_size(label_layout, 80, 20);
    lv_obj_set_style_text_align(label_layout, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(label_layout, &ergohaven_symbols_20, LV_PART_MAIN);

    screen_home_mods = lv_obj_create(screen_home);
    lv_obj_add_style(screen_home_mods, &style_container, 0);
    use_flex_row(screen_home_mods);
    lv_obj_set_pos(screen_home_mods, 0, 130);
    lv_obj_set_size(screen_home_mods, 240, 100);

    label_gui   = create_button(screen_home_mods, "GUI", &style_button, &style_button_active);
    label_alt   = create_button(screen_home_mods, "ALT", &style_button, &style_button_active);
    label_ctrl  = create_button(screen_home_mods, "CTL", &style_button, &style_button_active);
    label_shift = create_button(screen_home_mods, "SFT", &style_button, &style_button_active);

    label_num    = create_button(screen_home_mods, "NUM", &style_button, &style_button_active);
    label_caps   = create_button(screen_home_mods, "CAPS", &style_button, &style_button_active);
    label_scroll = create_button(screen_home_mods, "SCRL", &style_button, &style_button_active);

    screen_home_media = lv_obj_create(screen_home);
    lv_obj_add_style(screen_home_media, &style_container, 0);
    toggle_hidden(screen_home_media, false);
    use_flex_row(screen_home_media);
    lv_obj_set_pos(screen_home_media, 0, 130);
    lv_obj_set_size(screen_home_media, 240, 130);

    label_hid_media_title = lv_label_create(screen_home_media);
    lv_label_set_text(label_hid_media_title, "");
    lv_label_set_long_mode(label_hid_media_title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label_hid_media_title, lv_pct(95));
    lv_obj_set_style_text_align(label_hid_media_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_hid_media_title, &ergohaven_symbols_28, LV_PART_MAIN);
    lv_obj_set_style_pad_top(label_hid_media_title, 0, 0);
    lv_obj_set_style_pad_bottom(label_hid_media_title, 0, 0);

    label_hid_media_artist = lv_label_create(screen_home_media);
    lv_label_set_text(label_hid_media_artist, "");
    lv_label_set_long_mode(label_hid_media_artist, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label_hid_media_artist, lv_pct(95));
    lv_obj_set_style_text_align(label_hid_media_artist, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_hid_media_artist, &ergohaven_symbols_20, LV_PART_MAIN);

    lv_obj_set_style_pad_top(label_hid_media_artist, 10, 0);
    lv_obj_set_style_text_color(label_hid_media_artist, accent_color_blue, 0);
}

void screen_home_load(void) {
    lv_scr_load(screen_home);
}

void screen_home_housekeep(void) {
    static uint8_t prev_layer      = 255;
    static uint8_t prev_lang       = -1;
    static led_t   prev_led_state  = {.raw = 0};
    static uint8_t prev_mods       = 255;
    static bool    prev_show_mods  = false;
    static bool    prev_hid_active = false;
    static bool    prev_mac        = false;

    uint8_t cur_layer = get_current_layer();
    uint8_t cur_lang  = split_get_lang();
    led_t   led_state = host_keyboard_led_state();
    led_state.caps_lock |= split_get_caps_word();
    uint8_t mods = get_mods() | get_oneshot_mods();
    bool    mac  = split_get_mac();

    static uint32_t mods_timer = 0;
    if (mods != 0 || prev_led_state.raw != led_state.raw) {
        mods_timer = timer_read32();
    }

    bool show_mods  = timer_elapsed32(mods_timer) < EH_DISPLAY_TIMEOUT_ACTIVITY;
    bool hid_active = is_hid_active();

    uint32_t activity_elapsed = last_input_activity_elapsed();
    if (activity_elapsed > __UINT32_MAX__ - 1000) // possible overflow on split
        activity_elapsed = 0;
    bool typing = activity_elapsed < 500; // prevent display updates when typing

    if (prev_hid_active != hid_active || (prev_show_mods != show_mods && hid_active)) {
        if (typing) return;
        toggle_hidden(label_product, !hid_active);
        toggle_hidden(label_time, hid_active);
        toggle_hidden(screen_home_mods, !hid_active || show_mods);
        toggle_hidden(screen_home_media, hid_active && !show_mods);
        prev_show_mods  = show_mods;
        prev_hid_active = hid_active;
        return;
    }

    if (prev_layer != cur_layer) {
        lv_label_set_text(label_layer, get_layer_label(cur_layer));
        prev_layer = cur_layer;
        return;
    }

    if (prev_lang != cur_lang) {
        lv_label_set_text(label_layout, get_layout_label(cur_lang));
        prev_lang = cur_lang;
        return;
    }

    if (mac != prev_mac) {
        toggle_hidden(label_mac, mac);
        if (mac) {
            lv_label_set_text(label_gui, "CMD");
            lv_label_set_text(label_alt, "OPT");
        } else {
            lv_label_set_text(label_gui, "GUI");
            lv_label_set_text(label_alt, "ALT");
        }
        prev_mac = mac;
        return;
    }

    if (led_state.raw != prev_led_state.raw) {
        toggle_state(label_caps, LV_STATE_PRESSED, led_state.caps_lock);
        toggle_state(label_num, LV_STATE_PRESSED, led_state.num_lock);
        toggle_state(label_scroll, LV_STATE_PRESSED, led_state.scroll_lock);
        prev_led_state.raw = led_state.raw;
        return;
    }

    if (mods != prev_mods) {
        toggle_state(label_shift, LV_STATE_PRESSED, mods & MOD_MASK_SHIFT);
        toggle_state(label_ctrl, LV_STATE_PRESSED, mods & MOD_MASK_CTRL);
        toggle_state(label_alt, LV_STATE_PRESSED, mods & MOD_MASK_ALT);
        toggle_state(label_gui, LV_STATE_PRESSED, mods & MOD_MASK_GUI);
        prev_mods = mods;
        return;
    }

    hid_data_t *hid = get_hid_data();
    if (hid->hid_changed) {
        if (typing) return;
        if (hid->time_changed) {
            lv_label_set_text_fmt(label_time, "%02d:%02d", hid->hours, hid->minutes);
            hid->time_changed = false;
            return;
        }
        if (hid->media_title_changed) {
            lv_label_set_text(label_hid_media_title, hid->media_title);
            hid->media_title_changed = false;
            return;
        }
        if (hid->media_artist_changed) {
            lv_label_set_text(label_hid_media_artist, hid->media_artist);
            hid->media_artist_changed = false;
            return;
        }
        hid->hid_changed = false;
        return;
    }
}

const eh_screen_t eh_screen_home = {
    .init      = screen_home_init,
    .load      = screen_home_load,
    .housekeep = screen_home_housekeep,
};
