#include QMK_KEYBOARD_H

#include <string.h>

#include "drivers/sensors/azoteq_iqs5xx.h"
#include "drivers/sensors/pmw3610.h"
#include "quantum/split_common/transactions.h"
#include "src/eh_pointing.h"
#include "via.h"
#include "pointing_device_internal.h"

#ifndef AZOTEQ_IQS5XX_ADDRESS
#define AZOTEQ_IQS5XX_ADDRESS (0x74 << 1)
#endif

#ifndef ARRAY_SIZE
#    define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

typedef enum {
    HPD3_MODULE_AUTO = 0,
    HPD3_MODULE_NONE,
    HPD3_MODULE_TRACKBALL,
    HPD3_MODULE_TOUCHPAD,
} hpd3_module_t;

typedef union {
    uint32_t raw;
    struct {
        bool    hide_left_encoder : 1;
        bool    hide_right_encoder : 1;
        uint8_t left_module : 2;
        uint8_t right_module : 2;
        uint8_t left_orientation : 2;
        uint8_t right_orientation : 2;
        uint8_t trackball_dpi : 4;
        uint8_t touchpad_dpi : 3;
        uint8_t sniper_sens : 3;
        uint8_t scroll_sens : 3;
        uint8_t text_sens : 3;
        bool    invert_scroll : 1;
        bool    acceleration : 1;
        bool    led_blinks : 1;
        uint8_t _reserved : 3;
    } __attribute__((packed));
} hpd3_via_config_t;

static const uint16_t hpd3_trackball_cpi_table[] = {200, 400, 600, 800, 1000, 1200, 1600, 2000, 2400, 3200};
static const uint16_t hpd3_touchpad_cpi_table[]  = {200, 400, 600, 800, 1000};
static const uint8_t  hpd3_sniper_table[]        = {1, 2, 4, 8, 12, 16, 24, 32};
static const uint8_t  hpd3_scroll_table[]        = {2, 4, 8, 16, 24, 32, 48, 64};
static const uint8_t  hpd3_text_table[]          = {1, 2, 4, 8, 16, 24, 32, 48};

static uint8_t hpd3_clamp_index(uint8_t index, uint8_t max) {
    return index < max ? index : (max - 1);
}

static hpd3_via_config_t hpd3_via_config = {.raw = VIA_EEPROM_LAYOUT_OPTIONS_DEFAULT};
static uint32_t          hpd3_synced_raw = VIA_EEPROM_LAYOUT_OPTIONS_DEFAULT;
static uint32_t          hpd3_applied_raw = UINT32_MAX;

static bool          hpd3_touchpad_available    = false;
static bool          hpd3_touchpad_initialized  = false;
static bool          hpd3_trackball_available   = false;
static bool          hpd3_trackball_initialized = false;
static hpd3_module_t hpd3_detected_module       = HPD3_MODULE_NONE;

static orientation_t hpd3_get_local_orientation(void) {
    if (is_keyboard_left()) {
        return (orientation_t)hpd3_via_config.left_orientation;
    } else {
        return (orientation_t)hpd3_via_config.right_orientation;
    }
}

static hpd3_module_t hpd3_get_override_module(void) {
    if (is_keyboard_left()) {
        return (hpd3_module_t)hpd3_via_config.left_module;
    } else {
        return (hpd3_module_t)hpd3_via_config.right_module;
    }
}

static hpd3_module_t hpd3_get_active_module(void) {
    hpd3_module_t module = hpd3_get_override_module();
    if (module == HPD3_MODULE_AUTO) {
        return hpd3_detected_module;
    }
    if (module == HPD3_MODULE_NONE) {
        return HPD3_MODULE_NONE;
    }
    return module;
}

static report_mouse_t hpd3_rotate_report(report_mouse_t report, orientation_t orientation) {
    int8_t tmp;
    switch (orientation) {
        case ROT_0:
            break;
        case ROT_90:
            tmp      = report.x;
            report.x = -report.y;
            report.y = tmp;
            break;
        case ROT_180:
            report.x = -report.x;
            report.y = -report.y;
            break;
        case ROT_270:
            tmp      = report.x;
            report.x = report.y;
            report.y = -tmp;
            break;
    }
    return report;
}

static bool hpd3_detect_touchpad(void) {
    pd_dprintf("hpd3_detect_touchpad: start (I2C address=0x%02X)\n", AZOTEQ_IQS5XX_ADDRESS);
    azoteq_iqs5xx_init();
    wait_ms(50);
    uint16_t product = azoteq_iqs5xx_get_product();
    pd_dprintf("hpd3_detect_touchpad: product=%u\n", product);
    if (product == AZOTEQ_IQS5XX_UNKNOWN) {
        pd_dprintf("hpd3 right touchpad not detected (product unknown)\n");
        // Force touchpad for debugging
        // return true;
        return false;
    }
    pd_dprintf("hpd3 right touchpad detected, product=%u\n", product);
    return true;
}

static bool hpd3_detect_trackball(void) {
    bool ok = pmw3610_init(0);
    if (ok) {
        pd_dprintf("hpd3 right trackball detected\n");
    } else {
        pd_dprintf("hpd3 right trackball not detected\n");
    }
    return ok;
}

static void hpd3_detect_modules(void) {
    hpd3_touchpad_available    = false;
    hpd3_trackball_available   = false;
    hpd3_touchpad_initialized  = false;
    hpd3_trackball_initialized = false;
    hpd3_detected_module       = HPD3_MODULE_NONE;

    pd_dprintf("hpd3_detect_modules: start\n");
    hpd3_touchpad_available   = hpd3_detect_touchpad();
    hpd3_touchpad_initialized = hpd3_touchpad_available;
    pd_dprintf("  touchpad_available=%d\n", hpd3_touchpad_available);

    hpd3_trackball_available   = hpd3_detect_trackball();
    hpd3_trackball_initialized = hpd3_trackball_available;
    pd_dprintf("  trackball_available=%d\n", hpd3_trackball_available);

    if (hpd3_touchpad_available) {
        hpd3_detected_module = HPD3_MODULE_TOUCHPAD;
    } else if (hpd3_trackball_available) {
        hpd3_detected_module = HPD3_MODULE_TRACKBALL;
    }
    pd_dprintf("  detected_module=%u\n", hpd3_detected_module);

    // Debug I2C status
    pd_dprintf("  I2C status after detect: touchpad=%d trackball=%d\n",
            hpd3_touchpad_available, hpd3_trackball_available);
}

static void hpd3_ensure_selected_module_ready(void) {
    switch (hpd3_get_active_module()) {
        case HPD3_MODULE_TRACKBALL:
            if (!hpd3_trackball_initialized) {
                hpd3_trackball_available   = hpd3_detect_trackball();
                hpd3_trackball_initialized = hpd3_trackball_available;
            }
            break;
        case HPD3_MODULE_TOUCHPAD:
            if (!hpd3_touchpad_initialized) {
                hpd3_touchpad_available   = hpd3_detect_touchpad();
                hpd3_touchpad_initialized = hpd3_touchpad_available;
            }
            break;
        default:
            break;
    }
}

static void hpd3_apply_device_config(void) {
    if (hpd3_applied_raw == hpd3_via_config.raw) {
        return;
    }

    dprintf("hpd3_apply_device_config raw=0x%08lX\n", (unsigned long)hpd3_via_config.raw);
    dprintf("  left_module=%u right_module=%u\n", (unsigned int)hpd3_via_config.left_module, (unsigned int)hpd3_via_config.right_module);
    dprintf("  left_orientation=%u right_orientation=%u\n", (unsigned int)hpd3_via_config.left_orientation, (unsigned int)hpd3_via_config.right_orientation);
    dprintf("  trackball_dpi=%u touchpad_dpi=%u\n", (unsigned int)hpd3_via_config.trackball_dpi, (unsigned int)hpd3_via_config.touchpad_dpi);
    dprintf("  sniper_sens=%u scroll_sens=%u text_sens=%u\n", (unsigned int)hpd3_via_config.sniper_sens, (unsigned int)hpd3_via_config.scroll_sens, (unsigned int)hpd3_via_config.text_sens);
    dprintf("  invert_scroll=%u acceleration=%u led_blinks=%u\n", (unsigned int)hpd3_via_config.invert_scroll, (unsigned int)hpd3_via_config.acceleration, (unsigned int)hpd3_via_config.led_blinks);

    set_sniper_sens(hpd3_sniper_table[hpd3_clamp_index(hpd3_via_config.sniper_sens, ARRAY_SIZE(hpd3_sniper_table))]);
    set_scroll_sens(hpd3_scroll_table[hpd3_clamp_index(hpd3_via_config.scroll_sens, ARRAY_SIZE(hpd3_scroll_table))]);
    set_text_sens(hpd3_text_table[hpd3_clamp_index(hpd3_via_config.text_sens, ARRAY_SIZE(hpd3_text_table))]);
    set_invert_scroll(hpd3_via_config.invert_scroll);
    set_acceleration(hpd3_via_config.acceleration);
    set_led_blinks(hpd3_via_config.led_blinks);

    if (hpd3_trackball_initialized) {
        uint8_t idx = hpd3_clamp_index(hpd3_via_config.trackball_dpi, ARRAY_SIZE(hpd3_trackball_cpi_table));
        uint16_t cpi = hpd3_trackball_cpi_table[idx];
        dprintf("  trackball CPI=%u (idx=%u)\n", cpi, idx);
        pmw3610_set_cpi(0, cpi);
    }
    if (hpd3_touchpad_initialized) {
        uint8_t idx = hpd3_clamp_index(hpd3_via_config.touchpad_dpi, ARRAY_SIZE(hpd3_touchpad_cpi_table));
        uint16_t cpi = hpd3_touchpad_cpi_table[idx];
        dprintf("  touchpad CPI=%u (idx=%u)\n", cpi, idx);
        azoteq_iqs5xx_set_cpi(cpi);
    }

    hpd3_applied_raw = hpd3_via_config.raw;
}

void via_set_layout_options_kb(uint32_t value) {
    dprintf("via_set_layout_options_kb raw=0x%08lX\n", (unsigned long)value);
    dprintf("  left_module=%lu, right_module=%lu\n", (unsigned long)((value >> 2) & 3), (unsigned long)((value >> 4) & 3));
    dprintf("  left_orientation=%lu, right_orientation=%lu\n", (unsigned long)((value >> 6) & 3), (unsigned long)((value >> 8) & 3));
    dprintf("  trackball_dpi=%lu, touchpad_dpi=%lu\n", (unsigned long)((value >> 10) & 15), (unsigned long)((value >> 14) & 7));
    hpd3_via_config.raw = value;
    hpd3_synced_raw     = value;
    hpd3_apply_device_config();
}

static void hpd3_sync_config_rpc(uint8_t in_len, const void *in_data, uint8_t out_len, void *out_data) {
    if (in_len == sizeof(uint32_t) && in_data != NULL) {
        uint32_t value = 0;
        memcpy(&value, in_data, sizeof(value));
        via_set_layout_options_kb(value);
    }
}

void keyboard_post_init_user(void) {
#ifdef CONSOLE_ENABLE
    debug_enable = true;
    dprintf("keyboard_post_init_user: HPD3 both halves debug\n");
#endif
    transaction_register_rpc(RPC_HPD3_CONFIG, hpd3_sync_config_rpc);
    via_set_layout_options_kb(via_get_layout_options());
    hpd3_synced_raw = hpd3_via_config.raw;
    dprintf("final raw=0x%08lX\n", (unsigned long)hpd3_synced_raw);
}

void housekeeping_task_user(void) {
    static uint32_t last_sync = 0;

    if (!is_keyboard_master()) {
        return;
    }
    if (timer_elapsed32(last_sync) < 100) {
        return;
    }

    last_sync = timer_read32();
    if (hpd3_synced_raw != hpd3_via_config.raw) {
        hpd3_synced_raw = hpd3_via_config.raw;
    }
    transaction_rpc_send(RPC_HPD3_CONFIG, sizeof(hpd3_synced_raw), &hpd3_synced_raw);
}

void pointing_device_driver_init(void) {
    pd_dprintf("pointing_device_driver_init: entry, is_keyboard_left()=%d, is_keyboard_master()=%d\n", is_keyboard_left(), is_keyboard_master());
    pd_dprintf("pointing_device_driver_init: both halves\n");
    hpd3_detect_modules();
    pd_dprintf("  detected_module=%u, touchpad_avail=%d, trackball_avail=%d\n", (unsigned int)hpd3_detected_module, hpd3_touchpad_available, hpd3_trackball_available);
    hpd3_ensure_selected_module_ready();
    hpd3_apply_device_config();
    pointing_device_set_cpi(pointing_device_driver_get_cpi());
    pd_dprintf("pointing_device_driver_init: done\n");
}

report_mouse_t pointing_device_driver_get_report(report_mouse_t mouse_report) {
    static uint32_t last_log = 0;
    if (timer_elapsed32(last_log) > 1000) {
        pd_dprintf("pointing_device_driver_get_report: is_left=%d, active_module=%u, touchpad_avail=%d, trackball_avail=%d\n",
                is_keyboard_left(), (unsigned int)hpd3_get_active_module(), hpd3_touchpad_available, hpd3_trackball_available);
        last_log = timer_read32();
    }

    hpd3_ensure_selected_module_ready();
    hpd3_apply_device_config();

    switch (hpd3_get_active_module()) {
        case HPD3_MODULE_TRACKBALL:
            if (hpd3_trackball_initialized) {
                mouse_report = pmw3610_get_report(mouse_report);
                mouse_report.x = -mouse_report.x;
            }
            break;
        case HPD3_MODULE_TOUCHPAD:
            if (hpd3_touchpad_initialized) {
                mouse_report = azoteq_iqs5xx_get_report(mouse_report);
            }
            break;
        default:
            break;
    }

    return hpd3_rotate_report(mouse_report, hpd3_get_local_orientation());
}

uint16_t pointing_device_driver_get_cpi(void) {
    switch (hpd3_get_active_module()) {
        case HPD3_MODULE_TRACKBALL:
            return hpd3_trackball_cpi_table[hpd3_clamp_index(hpd3_via_config.trackball_dpi, ARRAY_SIZE(hpd3_trackball_cpi_table))];
        case HPD3_MODULE_TOUCHPAD:
            return hpd3_touchpad_cpi_table[hpd3_clamp_index(hpd3_via_config.touchpad_dpi, ARRAY_SIZE(hpd3_touchpad_cpi_table))];
        default:
            return 0;
    }
}

void pointing_device_driver_set_cpi(uint16_t cpi) {
    dprintf("pointing_device_driver_set_cpi: cpi=%u, active_module=%u\n", cpi, (unsigned int)hpd3_get_active_module());
    switch (hpd3_get_active_module()) {
        case HPD3_MODULE_TRACKBALL:
            if (hpd3_trackball_initialized) {
                dprintf("  setting trackball CPI\n");
                pmw3610_set_cpi(0, cpi);
            } else {
                dprintf("  trackball not initialized\n");
            }
            break;
        case HPD3_MODULE_TOUCHPAD:
            if (hpd3_touchpad_initialized) {
                dprintf("  setting touchpad CPI\n");
                azoteq_iqs5xx_set_cpi(cpi);
            } else {
                dprintf("  touchpad not initialized\n");
            }
            break;
        default:
            dprintf("  no active module\n");
            break;
    }
}
