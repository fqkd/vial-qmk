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
        uint8_t left_ball_orientation : 2;
        uint8_t right_ball_orientation : 2;
        uint8_t left_touch_orientation : 2;
        uint8_t right_touch_orientation : 2;
        uint8_t left_ball_dpi : 4;
        uint8_t right_ball_dpi : 4;
        uint8_t left_touch_dpi : 4;
        uint8_t right_touch_dpi : 4;
        uint8_t _reserved : 5;
        bool    schema_v2 : 1;
    } __attribute__((packed));
} hpd3_via_config_t;

static const uint16_t hpd3_trackball_cpi_table[] = {200, 400, 600, 800, 1000, 1200, 1400, 1600, 1800, 2000, 2200, 2400, 2600, 2800, 3000, 3200};
static const uint16_t hpd3_touchpad_cpi_table[]  = {100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};

#define HPD3_LAYOUT_SCHEMA_V2 (1u << 31)
#define HPD3_LEFT_BALL_AXIS_SHIFT 2
#define HPD3_RIGHT_BALL_AXIS_SHIFT 4
#define HPD3_LEFT_TOUCH_AXIS_SHIFT 6
#define HPD3_RIGHT_TOUCH_AXIS_SHIFT 8
#define HPD3_LEFT_BALL_DPI_SHIFT 10
#define HPD3_RIGHT_BALL_DPI_SHIFT 14
#define HPD3_LEFT_TOUCH_DPI_SHIFT 18
#define HPD3_RIGHT_TOUCH_DPI_SHIFT 22

static uint32_t hpd3_upgrade_raw(uint32_t raw) {
    if (raw & HPD3_LAYOUT_SCHEMA_V2) {
        return raw;
    }

    if (raw == 0x03324480u || raw == 0x03324540u) {
        return VIA_EEPROM_LAYOUT_OPTIONS_DEFAULT;
    }

    uint32_t upgraded = raw & 0x3u;
    uint32_t left_orientation  = (raw >> 6) & 0x3u;
    uint32_t right_orientation = (raw >> 8) & 0x3u;
    uint32_t trackball_dpi     = (raw >> 10) & 0xFu;
    uint32_t touchpad_dpi      = (raw >> 14) & 0x7u;

    upgraded |= left_orientation << HPD3_LEFT_BALL_AXIS_SHIFT;
    upgraded |= right_orientation << HPD3_RIGHT_BALL_AXIS_SHIFT;
    upgraded |= left_orientation << HPD3_LEFT_TOUCH_AXIS_SHIFT;
    upgraded |= right_orientation << HPD3_RIGHT_TOUCH_AXIS_SHIFT;
    upgraded |= trackball_dpi << HPD3_LEFT_BALL_DPI_SHIFT;
    upgraded |= trackball_dpi << HPD3_RIGHT_BALL_DPI_SHIFT;
    upgraded |= touchpad_dpi << HPD3_LEFT_TOUCH_DPI_SHIFT;
    upgraded |= touchpad_dpi << HPD3_RIGHT_TOUCH_DPI_SHIFT;
    upgraded |= HPD3_LAYOUT_SCHEMA_V2;
    return upgraded;
}

static uint8_t hpd3_clamp_index(uint8_t index, uint8_t max) {
    return index < max ? index : (max - 1);
}

// hpd3-axis-indep-v0.0.5, read side-specific fields from raw, not C bitfields.
static hpd3_via_config_t          hpd3_via_config           = {.raw = VIA_EEPROM_LAYOUT_OPTIONS_DEFAULT};
static uint32_t                   hpd3_synced_raw           = VIA_EEPROM_LAYOUT_OPTIONS_DEFAULT;
static uint32_t                   hpd3_applied_raw          = UINT32_MAX;
static kb_settings_hpd3_devices_t hpd3_synced_devices;
static kb_settings_hpd3_devices_t hpd3_applied_devices;
static bool                       hpd3_applied_devices_valid = false;

static hpd3_device_id_t hpd3_get_local_device_id(hpd3_module_t module) {
    bool left = is_keyboard_left();

    switch (module) {
        case HPD3_MODULE_TRACKBALL:
            return left ? HPD3_DEVICE_LEFT_BALL : HPD3_DEVICE_RIGHT_BALL;
        case HPD3_MODULE_TOUCHPAD:
        default:
            return left ? HPD3_DEVICE_LEFT_TOUCH : HPD3_DEVICE_RIGHT_TOUCH;
    }
}

static bool          hpd3_touchpad_available    = false;
static bool          hpd3_touchpad_initialized  = false;
static bool          hpd3_trackball_available   = false;
static bool          hpd3_trackball_initialized = false;
static hpd3_module_t hpd3_detected_module       = HPD3_MODULE_NONE;

static orientation_t hpd3_get_local_orientation(void) {
    return get_hpd3_device_orientation(hpd3_get_local_device_id(hpd3_detected_module));
}

static hpd3_module_t hpd3_get_active_module(void) {
    return hpd3_detected_module;
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

static report_mouse_t hpd3_rotate_trackball_report(report_mouse_t report, orientation_t orientation) {
    report = hpd3_rotate_report(report, orientation);
    report.y = -report.y;
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
    kb_settings_hpd3_devices_t devices = get_settings_hpd3_devices();
    if (hpd3_applied_raw == hpd3_via_config.raw && hpd3_applied_devices_valid && memcmp(&hpd3_applied_devices, &devices, sizeof(devices)) == 0) {
        return;
    }

    dprintf("hpd3_apply_device_config raw=0x%08lX\n", (unsigned long)hpd3_via_config.raw);
    dprintf("  left_ball_axis=%u right_ball_axis=%u\n", get_hpd3_device_orientation(HPD3_DEVICE_LEFT_BALL), get_hpd3_device_orientation(HPD3_DEVICE_RIGHT_BALL));
    dprintf("  left_touch_axis=%u right_touch_axis=%u\n", get_hpd3_device_orientation(HPD3_DEVICE_LEFT_TOUCH), get_hpd3_device_orientation(HPD3_DEVICE_RIGHT_TOUCH));
    dprintf("  left_ball_dpi=%u right_ball_dpi=%u\n", get_hpd3_device_dpi_index(HPD3_DEVICE_LEFT_BALL), get_hpd3_device_dpi_index(HPD3_DEVICE_RIGHT_BALL));
    dprintf("  left_touch_dpi=%u right_touch_dpi=%u\n", get_hpd3_device_dpi_index(HPD3_DEVICE_LEFT_TOUCH), get_hpd3_device_dpi_index(HPD3_DEVICE_RIGHT_TOUCH));

    if (hpd3_trackball_initialized) {
        uint8_t idx = hpd3_clamp_index(get_hpd3_device_dpi_index(hpd3_get_local_device_id(HPD3_MODULE_TRACKBALL)), ARRAY_SIZE(hpd3_trackball_cpi_table));
        uint16_t cpi = hpd3_trackball_cpi_table[idx];
        dprintf("  trackball CPI=%u (idx=%u)\n", cpi, idx);
        pmw3610_set_cpi(0, cpi);
    }
    if (hpd3_touchpad_initialized) {
        uint8_t idx = hpd3_clamp_index(get_hpd3_device_dpi_index(hpd3_get_local_device_id(HPD3_MODULE_TOUCHPAD)), ARRAY_SIZE(hpd3_touchpad_cpi_table));
        uint16_t cpi = hpd3_touchpad_cpi_table[idx];
        dprintf("  touchpad CPI=%u (idx=%u)\n", cpi, idx);
        azoteq_iqs5xx_set_cpi(cpi);
    }

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    set_auto_mouse_layer(get_hpd3_auto_mouse_layer());
    set_auto_mouse_enable(get_hpd3_auto_mouse_enable());
    if (!get_hpd3_auto_mouse_enable()) {
        auto_mouse_layer_off();
    }
#endif

    hpd3_applied_raw           = hpd3_via_config.raw;
    hpd3_applied_devices       = devices;
    hpd3_applied_devices_valid = true;
}

uint32_t via_set_layout_options_normalize_kb(uint32_t value) {
    if ((hpd3_via_config.raw & HPD3_LAYOUT_SCHEMA_V2) && !(value & HPD3_LAYOUT_SCHEMA_V2)) {
        value = (hpd3_via_config.raw & ~0x3u) | (value & 0x3u);
    }
    return value;
}

void via_set_layout_options_kb(uint32_t value) {
    value = hpd3_upgrade_raw(value);
    dprintf("via_set_layout_options_kb raw=0x%08lX\n", (unsigned long)value);
    dprintf("  left_ball_axis=%lu, right_ball_axis=%lu\n", (unsigned long)((value >> HPD3_LEFT_BALL_AXIS_SHIFT) & 3), (unsigned long)((value >> HPD3_RIGHT_BALL_AXIS_SHIFT) & 3));
    dprintf("  left_touch_axis=%lu, right_touch_axis=%lu\n", (unsigned long)((value >> HPD3_LEFT_TOUCH_AXIS_SHIFT) & 3), (unsigned long)((value >> HPD3_RIGHT_TOUCH_AXIS_SHIFT) & 3));
    dprintf("  left_ball_dpi=%lu, right_ball_dpi=%lu\n", (unsigned long)((value >> HPD3_LEFT_BALL_DPI_SHIFT) & 15), (unsigned long)((value >> HPD3_RIGHT_BALL_DPI_SHIFT) & 15));
    dprintf("  left_touch_dpi=%lu, right_touch_dpi=%lu\n", (unsigned long)((value >> HPD3_LEFT_TOUCH_DPI_SHIFT) & 15), (unsigned long)((value >> HPD3_RIGHT_TOUCH_DPI_SHIFT) & 15));
    hpd3_via_config.raw = value;
    hpd3_synced_raw     = value;
    hpd3_apply_device_config();
}

static void hpd3_sync_config_rpc(uint8_t in_len, const void *in_data, uint8_t out_len, void *out_data) {
    if (in_len == sizeof(uint32_t) && in_data != NULL) {
        uint32_t value = 0;
        memcpy(&value, in_data, sizeof(value));
        value = hpd3_upgrade_raw(value);
        if (via_get_layout_options() != value) {
            via_set_layout_options(value);
        } else {
            via_set_layout_options_kb(value);
        }
    }
}

static void hpd3_sync_devices_rpc(uint8_t in_len, const void *in_data, uint8_t out_len, void *out_data) {
    if (in_len == sizeof(kb_settings_hpd3_devices_t) && in_data != NULL) {
        kb_settings_hpd3_devices_t value;
        memcpy(&value, in_data, sizeof(value));
        set_settings_hpd3_devices(value);
        hpd3_applied_devices_valid = false;
    }
}

void keyboard_post_init_user(void) {
#ifdef CONSOLE_ENABLE
    debug_enable = true;
    dprintf("keyboard_post_init_user: HPD3 both halves debug\n");
#endif
    transaction_register_rpc(RPC_HPD3_CONFIG, hpd3_sync_config_rpc);
    transaction_register_rpc(RPC_HPD3_DEVICES, hpd3_sync_devices_rpc);
    hpd3_applied_raw = UINT32_MAX; // force re-apply after early kb_settings_pointing_init may have set CPI=0
    hpd3_applied_devices_valid = false;

    uint32_t raw = hpd3_upgrade_raw(via_get_layout_options());
    if (raw != via_get_layout_options()) {
        via_set_layout_options(raw);
    } else {
        via_set_layout_options_kb(raw);
    }

    hpd3_synced_raw = hpd3_via_config.raw;
    hpd3_synced_devices = get_settings_hpd3_devices();
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
        transaction_rpc_send(RPC_HPD3_CONFIG, sizeof(hpd3_synced_raw), &hpd3_synced_raw);
    }

    kb_settings_hpd3_devices_t devices = get_settings_hpd3_devices();
    if (memcmp(&hpd3_synced_devices, &devices, sizeof(devices)) != 0) {
        hpd3_synced_devices = devices;
        transaction_rpc_send(RPC_HPD3_DEVICES, sizeof(hpd3_synced_devices), &hpd3_synced_devices);
    }
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

    if (hpd3_get_active_module() == HPD3_MODULE_TRACKBALL) {
        return hpd3_rotate_trackball_report(mouse_report, hpd3_get_local_orientation());
    }
    return hpd3_rotate_report(mouse_report, hpd3_get_local_orientation());
}

static bool hpd3_report_has_motion(report_mouse_t report) {
    return abs(report.x) >= 1 || abs(report.y) >= 1 || abs(report.h) >= 1 || abs(report.v) >= 1 || report.buttons;
}

static report_mouse_t hpd3_apply_side_mode(report_mouse_t mrpt, hpd3_side_id_t side) {
    static int32_t accumulated_h[HPD3_SIDE_COUNT] = {0};
    static int32_t accumulated_v[HPD3_SIDE_COUNT] = {0};

    if (get_hpd3_side_acceleration(side)) {
        int x = mrpt.x;
        int y = mrpt.y;
        mrpt.x = (mouse_xy_report_t)(x > 0 ? x * x / 16 + x : -x * x / 16 + x);
        mrpt.y = (mouse_xy_report_t)(y > 0 ? y * y / 16 + y : -y * y / 16 + y);
    }

    pointing_mode_t pmode   = get_hpd3_side_mode(side);
    int32_t         divisor = get_hpd3_side_sens(side, pmode);

    if (pmode == POINTING_MODE_NORMAL || divisor <= 0) {
        return mrpt;
    }

    accumulated_h[side] += mrpt.x;
    accumulated_v[side] += mrpt.y;

    int shift_x = accumulated_h[side] / divisor;
    int shift_y = accumulated_v[side] / divisor;

    accumulated_h[side] -= shift_x * divisor;
    accumulated_v[side] -= shift_y * divisor;

    mrpt.x = 0;
    mrpt.y = 0;

    if (shift_x == 0 && shift_y == 0) {
        return mrpt;
    }

    switch (pmode) {
        case POINTING_MODE_SNIPER:
            mrpt.x = shift_x;
            mrpt.y = shift_y;
            break;

        case POINTING_MODE_SCROLL:
            if (abs(shift_x) > abs(shift_y)) {
                mrpt.h              = shift_x;
                accumulated_v[side] = 0;
            } else if (abs(shift_x) < abs(shift_y)) {
                mrpt.v              = -shift_y;
                accumulated_h[side] = 0;
            }
            if (get_hpd3_side_invert_scroll(side)) {
                mrpt.h = -mrpt.h;
                mrpt.v = -mrpt.v;
            }
            break;

        case POINTING_MODE_TEXT:
        case POINTING_MODE_USR1:
        case POINTING_MODE_USR2:
        case POINTING_MODE_USR3: {
#ifdef EH_TRACKBALL_TEXT_DIR_REMAP
            static uint16_t kc_up[HPD3_SIDE_COUNT]    = {KC_UP, KC_UP};
            static uint16_t kc_down[HPD3_SIDE_COUNT]  = {KC_DOWN, KC_DOWN};
            static uint16_t kc_left[HPD3_SIDE_COUNT]  = {KC_LEFT, KC_LEFT};
            static uint16_t kc_right[HPD3_SIDE_COUNT] = {KC_RIGHT, KC_RIGHT};
#ifdef EH_HPD_LAYERS
            uint8_t layer   = get_current_layer();
            kc_up[side]     = dynamic_keymap_get_keycode(layer, 0, 0);
            kc_down[side]   = dynamic_keymap_get_keycode(layer, 0, 1);
            kc_left[side]   = dynamic_keymap_get_keycode(layer, 0, 2);
            kc_right[side]  = dynamic_keymap_get_keycode(layer, 0, 3);
#endif

            if (kc_up[side] != kc_down[side] || kc_up[side] != kc_left[side] || kc_up[side] != kc_right[side] || kc_up[side] != KC_NO) {
                if (abs(shift_x) > abs(shift_y)) {
                    shift_y             = 0;
                    accumulated_v[side] = 0;
                } else if (abs(shift_x) < abs(shift_y)) {
                    shift_x             = 0;
                    accumulated_h[side] = 0;
                }

                for (; shift_x > 0; shift_x--) tap_code16(kc_right[side]);
                for (; shift_x < 0; shift_x++) tap_code16(kc_left[side]);
                for (; shift_y > 0; shift_y--) tap_code16(kc_up[side]);
                for (; shift_y < 0; shift_y++) tap_code16(kc_down[side]);
            }
#endif
            break;
        }

        default:
            break;
    }

    return mrpt;
}

report_mouse_t pointing_device_task_combined_kb(report_mouse_t left_report, report_mouse_t right_report) {
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    bool auto_layer_active = false;
    if (get_hpd3_auto_mouse_enable()) {
        if (get_hpd3_side_mode(HPD3_SIDE_LEFT) == POINTING_MODE_NORMAL && hpd3_report_has_motion(left_report)) {
            auto_layer_active = true;
        }
        if (get_hpd3_side_mode(HPD3_SIDE_RIGHT) == POINTING_MODE_NORMAL && hpd3_report_has_motion(right_report)) {
            auto_layer_active = true;
        }
    }
    set_pointing_auto_mouse_override(true, auto_layer_active);
#endif

    left_report  = hpd3_apply_side_mode(left_report, HPD3_SIDE_LEFT);
    right_report = hpd3_apply_side_mode(right_report, HPD3_SIDE_RIGHT);
    return pointing_device_combine_reports(left_report, right_report);
}

uint16_t pointing_device_driver_get_cpi(void) {
    switch (hpd3_get_active_module()) {
        case HPD3_MODULE_TRACKBALL:
            return hpd3_trackball_cpi_table[hpd3_clamp_index(get_hpd3_device_dpi_index(hpd3_get_local_device_id(HPD3_MODULE_TRACKBALL)), ARRAY_SIZE(hpd3_trackball_cpi_table))];
        case HPD3_MODULE_TOUCHPAD:
            return hpd3_touchpad_cpi_table[hpd3_clamp_index(get_hpd3_device_dpi_index(hpd3_get_local_device_id(HPD3_MODULE_TOUCHPAD)), ARRAY_SIZE(hpd3_touchpad_cpi_table))];
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
