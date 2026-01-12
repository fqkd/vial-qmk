#include QMK_KEYBOARD_H
#include "transactions.h"
#include "ergohaven.h"
#include "display.h"
#include "ergohaven_pointing.h"
#include "src/display/eh_display.h"
#include "ergohaven_ruen.h"

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT(
        KC_GRV,   KC_1,   KC_2,    KC_3,     KC_4,     KC_5,                                              KC_6,  KC_7,     KC_8,     KC_9,   KC_0,    KC_BSPC,
        KC_ESC,   KC_Q,   KC_W,    KC_E,     KC_R,     KC_T,                                              KC_Y,  KC_U,     KC_I,     KC_O,   KC_P,    KC_BSLS,
        KC_TAB,   KC_A,   KC_S,    KC_D,     KC_F,     KC_G,                                              KC_H,  KC_J,     KC_K,     KC_L,   KC_SCLN, KC_QUOT,
        KC_LSFT,  KC_Z,   KC_X,    KC_C,     KC_V,     KC_B,                                              KC_N,  KC_M,     KC_COMM,  KC_DOT, KC_SLSH, KC_RSFT,
                          KC_PGDN, KC_PGUP,  KC_LCTL,  LOWER, KC_LALT, KC_SPACE,         KC_ENT, KC_RGUI, RAISE, KC_RCTL,  KC_LBRC,  KC_RBRC,
                                                                       _______,          _______
    ),

    [_LOWER] = LAYOUT(
        KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,    KC_F6,                                              KC_F7,   KC_F8,   KC_F9,   KC_F10, KC_F11,  KC_F12,
        _______, _______, KC_HOME, KC_UP,   KC_END,   KC_INS,                                             _______, KC_7,    KC_8,    KC_9,   _______, _______,
        _______, _______, KC_LEFT, KC_DOWN, KC_RIGHT, KC_ENT,                                             _______, KC_4,    KC_5,    KC_6,   _______, _______,
        _______, _______, PREVWRD, KC_CAPS, NEXTWRD,  KC_DEL,                                             KC_PSCR, KC_1,    KC_2,    KC_3,   _______, _______,
                          _______, _______, _______, _______, _______, _______,         _______, _______, ADJUST, _______, KC_0, _______,
                                                                       _______,         _______
    ),

    [_RAISE] = LAYOUT(
        _______, _______, KC_AT,   KC_DLR,  KC_HASH, _______,                                             _______, _______, KC_CIRC, _______, _______,  _______,
        _______, _______, KC_LT,   KC_EQL,  KC_GT,   KC_GRV,                                              _______, KC_LBRC, KC_UNDS, KC_RBRC, _______,  _______,
        _______, KC_BSLS, KC_LPRN, KC_MINS, KC_RPRN, KC_PLUS,                                             KC_PERC, KC_LCBR, KC_SCLN, KC_RCBR, KC_EXLM,  _______,
        _______, _______, KC_ASTR, KC_COLN, KC_SLSH, _______,                                             _______, KC_PIPE, KC_TILD, KC_AMPR, _______,  _______,
                          _______, _______, _______, ADJUST, _______, _______,         _______, _______, _______, _______, _______, _______,
                                                                      _______,         _______
    ),

    [_ADJUST] = LAYOUT(
        QK_BOOT, _______, _______, _______, _______, _______,                                              _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______,                                              _______, _______, _______, _______, DM_PLY1, DM_REC1,
        _______, _______, _______, _______, _______, _______,                                              _______, KC_VOLD, KC_MUTE, KC_VOLU, DM_PLY2, DM_REC2,
        _______, _______, _______, _______, _______, _______,                                              _______, KC_MPRV, KC_MPLY, KC_MNXT, _______, DM_RSTP,
                          _______, _______, _______, _______, _______, _______,          _______, _______, _______, _______, _______, _______,
                                                                       _______,          _______
    ),

    [_FOUR] = LAYOUT(
        _______, _______, _______, _______, _______, _______,                                              _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______,                                              _______, _______, _______, _______, _______, _______,
        _______, EH_SCR,  KC_BTN3, KC_BTN2, KC_BTN1, EH_SNP,                                               _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, EH_TXT,                                               _______, _______, _______, _______, _______, _______,
                          _______, _______, _______, _______, _______, _______,          _______, _______, _______, _______, _______, _______,
                                                                       _______,          _______
    ),
};
// clang-format on

typedef union {
    uint32_t raw;
    struct {
        uint8_t text_mode : 3;
        uint8_t scroll_mode : 3;
        uint8_t sniper_mode : 2;
        uint8_t dpi_mode : 4;
        bool    invert_scroll : 1;
        bool    acceleration : 1;
    };
} vial_config_t;

static vial_config_t vial_config;

typedef union {
    uint8_t raw;
    struct {
        uint8_t lang : 1;
        bool    mac : 1;
        bool    caps_word : 1;
    };
} display_config_t;

display_config_t display_config;

typedef union {
    uint16_t raw;
    struct {
        uint16_t dpi;
    };
} touch_config_t;

touch_config_t touch_config;

uint8_t split_get_lang(void) {
    return is_keyboard_master() ? get_cur_lang() : display_config.lang;
}

bool split_get_mac(void) {
    return is_keyboard_master() ? keymap_config.swap_lctl_lgui : display_config.mac;
}

bool split_get_caps_word(void) {
    return is_keyboard_master() ? is_caps_word_on() : display_config.caps_word;
}

const int     DPI_TABLE[15]    = {100, 200, 300, 400, 500, 600, 800, 1000, 1200, 1600, 2000, 2500, 3200, 4000, 5000};
const int32_t SNIPER_TABLE[15] = {2, 3, 4, 5};
const int32_t SCROLL_TABLE[15] = {6, 8, 11, 16, 23, 32, 45, 64};
const int32_t TEXT_TABLE[15]   = {6, 8, 11, 16, 23, 32, 45, 64};

int get_dpi(uint8_t dpi_mode) {
    if (dpi_mode < ARRAY_SIZE(DPI_TABLE))
        return DPI_TABLE[dpi_mode];
    else
        return DPI_TABLE[0];
}

void via_set_layout_options_kb(uint32_t value) {
    dprintf("via_set_layout_options_kb %lx\n", value);
    vial_config.raw = value;
    pointing_device_set_cpi(get_dpi(vial_config.dpi_mode));
    set_scroll_sens(SCROLL_TABLE[vial_config.scroll_mode]);
    set_sniper_sens(SNIPER_TABLE[vial_config.sniper_mode]);
    set_text_sens(TEXT_TABLE[vial_config.text_mode]);
    set_invert_scroll(vial_config.invert_scroll);
    set_acceleration(vial_config.acceleration);
}

void sync_touch(uint8_t in_buflen, const void* in_data, uint8_t out_buflen, void* out_data) {
    memcpy(&touch_config, in_data, sizeof(touch_config_t));
    pointing_device_set_cpi(touch_config.dpi);
    touch_config.dpi = pointing_device_get_cpi();
    memcpy(out_data, &touch_config, sizeof(touch_config_t));
}

void sync_display(uint8_t in_buflen, const void* in_data, uint8_t out_buflen, void* out_data) {
    memcpy(&display_config, in_data, sizeof(display_config_t));
}

void housekeeping_task_user(void) {
    if (is_display_enabled() && is_keyboard_left()) {
        display_housekeeping_task();
    }

    if (!is_keyboard_left() && is_keyboard_master()) {
        static bool is_display_on = true;

        uint32_t activity_elapsed = last_input_activity_elapsed();
        if (activity_elapsed > EH_TIMEOUT) {
            if (is_display_on) {
                backlight_level_noeeprom(0);
                is_display_on = false;
            }
        } else {
            if (!is_display_on) {
                backlight_init();
                is_display_on = true;
            }
        }
    }

    if (is_keyboard_left() && is_keyboard_master()) {
        static uint32_t       last_sync = 0;
        static touch_config_t slave     = {.raw = 0};

        if (last_sync == 0 || timer_elapsed32(last_sync) > 500) {
            if (slave.raw != touch_config.raw) {
                if (transaction_rpc_exec(RPC_SYNC_TOUCH, sizeof(touch_config_t), &touch_config, sizeof(touch_config_t), &slave)) {
                    dprintf("sync touch settings %d (slave %d)\n", touch_config.dpi, slave.dpi);
                }
                last_sync = timer_read32();
            }
        }
    }

    if (!is_keyboard_left() && is_keyboard_master()) {
        {
            static uint32_t         last_sync = 0;
            static display_config_t slave     = {.raw = 0};

            if (last_sync == 0 || timer_elapsed32(last_sync) > 500) {
                display_config.lang      = split_get_lang();
                display_config.mac       = split_get_mac();
                display_config.caps_word = split_get_caps_word();

                if (slave.raw != display_config.raw) {
                    if (transaction_rpc_send(RPC_SYNC_DISPLAY, sizeof(display_config_t), &display_config)) {
                        slave.raw = display_config.raw;
                        dprintf("sync display settings %x\n", display_config.raw);
                    }
                    last_sync = timer_read32();
                }
            }
        }

        {
            static uint32_t       last_sync   = 0;
            static touch_config_t real_config = {.raw = 0};

            if ((last_sync == 0 || timer_elapsed32(last_sync) > 100)) {
                if (touch_config.raw != real_config.raw) {
                    pointing_device_set_cpi(touch_config.dpi);
                    real_config.dpi = pointing_device_get_cpi();
                    dprintf("sync touch settings %d (real %d)\n", touch_config.dpi, real_config.dpi);
                }
                last_sync = timer_read32();
            }
        }
    }
}

void keyboard_post_init_user(void) {
    if (is_keyboard_left()) {
        display_init_kb();
    }

    vial_config.raw = via_get_layout_options();
    via_set_layout_options_kb(vial_config.raw);
    set_led_blinks(false);

    transaction_register_rpc(RPC_SYNC_TOUCH, sync_touch);
    transaction_register_rpc(RPC_SYNC_DISPLAY, sync_display);
}
