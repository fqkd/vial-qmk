#include QMK_KEYBOARD_H
#include "ergohaven.h"
#include "src/eh_pointing.h"

// clang-format off
// phenom-mini-layout-v0.0.14: align Base/Lower/Raise with the published Phenom Mini layout image.
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT(
KC_ESC,   KC_Q,     KC_W,     KC_E,     KC_R,     KC_T,                                   KC_Y,     KC_U,     KC_I,     KC_O,     KC_P,     KC_BSPC,
KC_TAB,   KC_A,     KC_S,     KC_D,     KC_F,     KC_G,                                   KC_H,     KC_J,     KC_K,     KC_L,     KC_SCLN,  KC_BSLS,
KC_LSFT,  KC_Z,     KC_X,     KC_C,     KC_V,     KC_B,                                   KC_N,     KC_M,     KC_COMM,  KC_DOT,   KC_SLSH,  KC_RSFT,
          KC_GRV,   KC_LGUI,  KC_LCTL,  LOWER,    KC_SPC,                                 KC_ENT,   RAISE,    KC_LALT,  KC_LBRC,  KC_RBRC,
                                                            KC_MUTE,            KC_MUTE
    ),

    [_LOWER] = LAYOUT(
_______,  KC_1,     KC_2,     KC_3,     KC_4,     KC_5,                                   KC_6,     KC_7,     KC_8,     KC_9,     KC_0,     _______,
_______,  KC_HOME,  KC_INS,   KC_DEL,  KC_END,    KC_ENT,                                 _______,  KC_LEFT,  KC_DOWN,  KC_UP,    KC_RIGHT, _______,
_______,  _______,  _______,  _______,  _______,  KC_PSCR,                                _______,  _______,  _______,  _______,  _______,  _______,
          KC_PGUP,  KC_PGDN,  _______,  _______,  _______,                                _______,  ADJUST,   _______,  KC_CAPS,  CW_TOGG,
                                                            _______,            _______
    ),

    [_RAISE] = LAYOUT(
_______,  KC_AT,    KC_LT,    KC_EQL,   KC_GT,    KC_GRV,                                 KC_CIRC,  KC_DQT,   KC_UNDS,  KC_QUOT,  _______,  _______,
_______,  KC_BSLS,  KC_LPRN,  KC_MINS,  KC_RPRN,  KC_PLUS,                                KC_PERC,  KC_LCBR,  KC_SCLN,  KC_RCBR,  KC_EXLM,  _______,
_______,  KC_HASH,  KC_ASTR,  KC_COLN,  KC_SLSH,  _______,                               _______,  KC_PIPE,  KC_TILD,  KC_AMPR,  KC_DLR,   _______,
          _______,  _______,  _______,  ADJUST,   _______,                                _______,  _______,  _______,  _______,  _______,
                                                            _______,            _______
    ),

    [_ADJUST] = LAYOUT(
KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,    KC_F6,                                  KC_F7,    KC_F8,    KC_F9,    KC_F10,   KC_F11,   KC_F12,
_______,  _______,  KC_MPRV,  KC_MPLY,  KC_MNXT,  _______,                                _______,  KC_VOLD,  KC_MUTE,  KC_VOLU,  _______,  _______,
_______,  _______,  _______,  _______,  _______,  _______,                                _______,  _______,  _______,  _______,  _______,  _______,
          _______,  _______,  _______,  _______,  _______,                                _______,  _______,  _______,  _______,  _______,
                                                            _______,            _______
    ),

    [_FOUR] = LAYOUT(
_______,  _______,  _______,  _______,  _______,  _______,                                _______,  _______,  _______,  _______,  _______,  _______,
_______,  EH_SCR,   KC_BTN3,  KC_BTN2,  KC_BTN1,  EH_SNP,                                 EH_SNP,   KC_BTN1,  KC_BTN2,  KC_BTN3,  EH_SCR,   _______,
_______,  _______,  _______,  _______,  _______,  EH_TXT,                                 EH_TXT,   _______,  _______,  _______,  _______,  _______,
          _______,  _______,  _______,  _______,  _______,                                _______,  _______,  _______,  _______,  _______,
                                                            _______,            _______
    ),
};
// clang-format on

#ifdef ENCODER_MAP_ENABLE
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [_BASE]   = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
    [_LOWER]  = {ENCODER_CCW_CW(RGB_VAD, RGB_VAI), ENCODER_CCW_CW(RGB_VAD, RGB_VAI)},
    [_RAISE]  = {ENCODER_CCW_CW(KC_LEFT, KC_RIGHT), ENCODER_CCW_CW(KC_LEFT, KC_RIGHT)},
    [_ADJUST] = {ENCODER_CCW_CW(KC_WH_D, KC_WH_U), ENCODER_CCW_CW(KC_WH_D, KC_WH_U)},
    [_FOUR]   = {ENCODER_CCW_CW(KC_WH_D, KC_WH_U), ENCODER_CCW_CW(KC_WH_D, KC_WH_U)},
};
#endif
