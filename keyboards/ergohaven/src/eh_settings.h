#pragma once

#include <stdint.h>
#include <stdbool.h>

#define LAYER_LABEL_SIZE 16

void kb_settings_init(void);

uint8_t kb_settings_ruen_toggle_mode(void);

bool kb_settings_ruen_mac_layout(void);

void kb_settings_ruen_toggle_mode_set(uint8_t mode);

void kb_settings_ruen_mac_layout_set(bool mac_layout);

/* should override if needed */
const char *default_layer_label(uint8_t layer);

const char *layer_name(uint8_t layer);

extern bool layer_name_updated;
