#pragma once

#include "quantum.h"

#define EH_RGB_LAYER_COUNT 16
#define EH_RGB_PALETTE_SIZE 25

typedef struct {
    uint8_t layer_color[EH_RGB_LAYER_COUNT];
} kb_settings_led_colors_t;

void keyboard_post_init_rgb(void);
void layer_state_set_rgb(layer_state_t layer_state);
void rgb_on(void);
void rgb_off(void);

void kb_settings_led_colors_init(void);
void kb_settings_led_colors_reset(void);
uint8_t get_layer_rgb_color(uint8_t layer);
void set_layer_rgb_color(uint8_t layer, uint8_t color);
void ergohaven_rgb_refresh_layer_colors(void);
