#include "display.h"
#include "eeconfig.h"
#include "ergohaven.h"
#include "ergohaven_rgb.h"
#include "src/display/eh_display.h"

#define MACROPAD_RGB_TIMEOUT_DEFAULT_MINS 10
#define MACROPAD_RGB_TIMEOUT_EEPROM_OFFSET (KB_SETTINGS_LED_COLORS_OFFSET + offsetof(kb_settings_led_colors_t, timeout_mins))

static uint8_t rgb_timeout_mins = MACROPAD_RGB_TIMEOUT_DEFAULT_MINS;

static bool rgb_timeout_mins_is_valid(uint8_t timeout_mins) {
    static const uint8_t timeout_variants[] = {0, 1, 2, 5, 10, 15, 30, 60};

    for (uint8_t i = 0; i < sizeof(timeout_variants); ++i) {
        if (timeout_variants[i] == timeout_mins) {
            return true;
        }
    }
    return false;
}

static void update_rgb_timeout_mins(uint8_t timeout_mins) {
    if (rgb_timeout_mins == timeout_mins) {
        return;
    }
    rgb_timeout_mins = timeout_mins;
    eeconfig_update_kb_datablock(&rgb_timeout_mins, MACROPAD_RGB_TIMEOUT_EEPROM_OFFSET, sizeof(rgb_timeout_mins));
}

void kb_settings_led_colors_init(void) {
    eeconfig_read_kb_datablock(&rgb_timeout_mins, MACROPAD_RGB_TIMEOUT_EEPROM_OFFSET, sizeof(rgb_timeout_mins));
    if (!rgb_timeout_mins_is_valid(rgb_timeout_mins)) {
        rgb_timeout_mins = MACROPAD_RGB_TIMEOUT_DEFAULT_MINS;
        eeconfig_update_kb_datablock(&rgb_timeout_mins, MACROPAD_RGB_TIMEOUT_EEPROM_OFFSET, sizeof(rgb_timeout_mins));
    }
}

void kb_settings_led_colors_reset(void) {
    rgb_timeout_mins = MACROPAD_RGB_TIMEOUT_DEFAULT_MINS;
    eeconfig_update_kb_datablock(&rgb_timeout_mins, MACROPAD_RGB_TIMEOUT_EEPROM_OFFSET, sizeof(rgb_timeout_mins));
}

uint8_t get_led_rgb_timeout_mins(void) {
    return rgb_timeout_mins;
}

void set_led_rgb_timeout_mins(uint8_t timeout_mins) {
    if (rgb_timeout_mins_is_valid(timeout_mins)) {
        update_rgb_timeout_mins(timeout_mins);
    }
}

uint32_t get_led_rgb_timeout_ms(void) {
    if (rgb_timeout_mins == 0) {
        return 0;
    }
    return (uint32_t)rgb_timeout_mins * 60 * 1000;
}

void housekeeping_task_user(void) {
    display_housekeeping_task();
}

void keyboard_post_init_user(void) {
    display_init_kb();
}
