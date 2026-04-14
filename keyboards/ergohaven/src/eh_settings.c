#include "eh_settings.h"
#include "eh_ruen.h"
#include "eh_pointing.h"
#include <eeconfig.h>
#include <debug.h>
#include <qmk_settings.h>
#include "via.h"

char layer_names[DYNAMIC_KEYMAP_LAYER_COUNT][LAYER_LABEL_SIZE];

__attribute__((weak)) const char *default_layer_label(uint8_t layer) {
    static const char *PROGMEM default_layer_labels[] = {
        "BASE", "LOWER", "RAISE", "ADJST", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVN", "TWLVE", "THRTN", "FRTN", "FIFTN",
    };
    return default_layer_labels[layer];
}

void kb_settings_layer_labels_reset(void) {
    for (int i = 0; i < DYNAMIC_KEYMAP_LAYER_COUNT; ++i) {
        eeconfig_update_kb_datablock(default_layer_label(i), KB_SETTINGS_LAYER_LABELS_OFFSET + i * LAYER_LABEL_SIZE, LAYER_LABEL_SIZE);
    }
    layer_name_updated = true;
}

void kb_settings_layer_labels_init(void) {
    for (int i = 0; i < DYNAMIC_KEYMAP_LAYER_COUNT; ++i)
        eeconfig_read_kb_datablock(layer_names[i], KB_SETTINGS_LAYER_LABELS_OFFSET + i * LAYER_LABEL_SIZE, LAYER_LABEL_SIZE);
}

void kb_settings_reset(void) {
    kb_settings_ruen_reset();
    kb_settings_layer_labels_reset();
    kb_settings_pointing_reset();
    kb_settings_hpd3_devices_reset();
    kb_settings_init();
}

void eeconfig_init_kb(void) {
    kb_settings_reset();
    eeconfig_init_user();
}

void kb_settings_init(void) {
    kb_settings_ruen_init();
    kb_settings_layer_labels_init();
    kb_settings_pointing_init();
    kb_settings_hpd3_devices_init();
}

#define DECLARE_SETTING_NOTIFY(id, _get, _set, _notify) {.qsid = id, .get = _get, .set = _set, .notify = _notify}
#define DECLARE_SETTING(id, _get, _set) DECLARE_SETTING_NOTIFY(id, _get, _set, NULL)

static int ruen_toggle_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t mode = get_ruen_toggle_mode();
    if (maxsz < sizeof(mode)) return -1;
    memcpy(setting, &mode, sizeof(mode));
    return 0;
}

static int ruen_toggle_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t mode;
    if (maxsz < sizeof(mode)) return -1;
    memcpy(&mode, setting, sizeof(mode));
    set_ruen_toggle_mode(mode);
    return 0;
}

static int ruen_macos_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    bool mac = get_ruen_mac_layout();
    if (maxsz < sizeof(mac)) return -1;
    memcpy(setting, &mac, sizeof(mac));
    return 0;
}

static int ruen_macos_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    bool mac;
    if (maxsz < sizeof(mac)) return -1;
    memcpy(&mac, setting, sizeof(mac));
    set_ruen_mac_layout(mac);
    return 0;
}

// should match with json/settings_ruen.json
// and UNICODE_SELECTED_MODES in config.h
static int unicode_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t unicode_mode = get_unicode_input_mode();
    uint8_t mode         = 0;
    switch (unicode_mode) {
        case UNICODE_MODE_MACOS:
            mode = 0;
            break;
        case UNICODE_MODE_LINUX:
            mode = 1;
            break;
        case UNICODE_MODE_WINDOWS:
            mode = 2;
            break;
        case UNICODE_MODE_WINCOMPOSE:
            mode = 3;
            break;
        default:
            return -1;
    }
    if (maxsz < sizeof(mode)) return -1;
    memcpy(setting, &mode, sizeof(mode));
    return 0;
}

static int unicode_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t mode = 0;
    if (maxsz < sizeof(mode)) return -1;
    memcpy(&mode, setting, sizeof(mode));
    switch (mode) {
        case 0:
            set_unicode_input_mode(UNICODE_MODE_MACOS);
            break;
        case 1:
            set_unicode_input_mode(UNICODE_MODE_LINUX);
            break;
        case 2:
            set_unicode_input_mode(UNICODE_MODE_WINDOWS);
            break;
        case 3:
            set_unicode_input_mode(UNICODE_MODE_WINCOMPOSE);
            break;
        default:
            return -1;
    }
    return 0;
}

const char *layer_name(uint8_t layer) {
    if (layer >= 0 && layer <= 15)
        return layer_names[layer];
    else
        return "UNDEF";
}

static int layer_name_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    int layer = proto->qsid - 200;
    if (layer < 0 || layer >= DYNAMIC_KEYMAP_LAYER_COUNT) return -1;
    strcpy(setting, layer_names[layer]);
    return 0;
}

bool layer_name_updated = false;

static int layer_name_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    int layer = proto->qsid - 200;
    if (layer < 0 || layer >= DYNAMIC_KEYMAP_LAYER_COUNT) return -1;
    dprintf("layer_name_set %d %s\n", layer, (const char *)setting);
    snprintf(layer_names[layer], sizeof(layer_names[layer]), (const char *)setting);
    eeconfig_update_kb_datablock(layer_names[layer], KB_SETTINGS_LAYER_LABELS_OFFSET + LAYER_LABEL_SIZE * layer, LAYER_LABEL_SIZE);
    layer_name_updated = true;
    return 0;
}

static const uint16_t hpd3_trackball_cpi_table[] = {200, 400, 600, 800, 1000, 1200, 1600, 2000, 2400, 3200};
static const uint16_t hpd3_touchpad_cpi_table[]  = {200, 400, 600, 800, 1000};

static hpd3_device_id_t hpd3_device_for_qsid(uint16_t qsid) {
#if defined(KEYBOARD_ergohaven_hpd_rev3)
    switch (qsid) {
        case 120:
        case 130:
            return HPD3_DEVICE_LEFT_BALL;
        case 121:
        case 131:
            return HPD3_DEVICE_RIGHT_BALL;
        case 122:
        case 132:
            return HPD3_DEVICE_LEFT_TOUCH;
        case 123:
        case 133:
            return HPD3_DEVICE_RIGHT_TOUCH;
        default:
            return HPD3_DEVICE_LEFT_BALL;
    }
#else
    switch (qsid) {
        case 120:
        case 129:
            return HPD3_DEVICE_LEFT_BALL;
        case 121:
        case 130:
            return HPD3_DEVICE_RIGHT_BALL;
        case 122:
        case 131:
            return HPD3_DEVICE_LEFT_TOUCH;
        case 123:
        case 132:
            return HPD3_DEVICE_RIGHT_TOUCH;
        default:
            return HPD3_DEVICE_LEFT_BALL;
    }
#endif
}

#if defined(KEYBOARD_ergohaven_hpd_rev3)
static hpd3_side_id_t hpd3_side_for_qsid(uint16_t qsid) {
    switch (qsid) {
        case 124:
        case 125:
        case 126:
        case 134:
            return HPD3_SIDE_LEFT;
        case 127:
        case 128:
        case 129:
        case 135:
            return HPD3_SIDE_RIGHT;
        default:
            return HPD3_SIDE_LEFT;
    }
}
#endif

static uint8_t hpd3_index_from_value_u16(const uint16_t *table, size_t count, uint16_t value) {
    uint8_t best    = 0;
    uint16_t best_d = UINT16_MAX;
    for (size_t i = 0; i < count; ++i) {
        uint16_t d = table[i] > value ? table[i] - value : value - table[i];
        if (d < best_d) {
            best_d = d;
            best   = (uint8_t)i;
        }
    }
    return best;
}

static uint16_t hpd3_value_from_index_u16(const uint16_t *table, size_t count, uint8_t idx) {
    if (idx >= count) idx = (uint8_t)(count - 1);
    return table[idx];
}

static const uint16_t *hpd3_dpi_table_for_qsid(uint16_t qsid, size_t *count) {
    switch (qsid) {
        case 120:
        case 121:
            *count = ARRAY_SIZE(hpd3_trackball_cpi_table);
            return hpd3_trackball_cpi_table;
        case 122:
        case 123:
            *count = ARRAY_SIZE(hpd3_touchpad_cpi_table);
            return hpd3_touchpad_cpi_table;
        default:
            *count = 0;
            return NULL;
    }
}

static int modules_trackball_dpi_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    size_t count = 0;
    const uint16_t *table = hpd3_dpi_table_for_qsid(proto->qsid, &count);
    if (!table) return -1;
    uint16_t cpi = hpd3_value_from_index_u16(table, count, get_hpd3_device_dpi_index(hpd3_device_for_qsid(proto->qsid)));
    if (maxsz < sizeof(cpi)) return -1;
    memcpy(setting, &cpi, sizeof(cpi));
    return 0;
}

static int modules_trackball_dpi_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    size_t count = 0;
    const uint16_t *table = hpd3_dpi_table_for_qsid(proto->qsid, &count);
    if (!table) return -1;
    uint16_t cpi;
    if (maxsz < sizeof(cpi)) return -1;
    memcpy(&cpi, setting, sizeof(cpi));
    set_hpd3_device_dpi_index(hpd3_device_for_qsid(proto->qsid), hpd3_index_from_value_u16(table, count, cpi));
    return 0;
}

static int modules_sens_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t sens = 0;
#if defined(KEYBOARD_ergohaven_hpd_rev3)
    switch (proto->qsid) {
        case 124:
        case 127:
            sens = get_hpd3_side_sens(hpd3_side_for_qsid(proto->qsid), POINTING_MODE_SNIPER);
            break;
        case 125:
        case 128:
            sens = get_hpd3_side_sens(hpd3_side_for_qsid(proto->qsid), POINTING_MODE_SCROLL);
            break;
        case 126:
        case 129:
            sens = get_hpd3_side_sens(hpd3_side_for_qsid(proto->qsid), POINTING_MODE_TEXT);
            break;
        default:
            return -1;
    }
#else
    switch (proto->qsid) {
        case 124:
            sens = get_sniper_sens();
            break;
        case 125:
            sens = get_scroll_sens();
            break;
        case 126:
            sens = get_text_sens();
            break;
        default:
            return -1;
    }
#endif
    if (maxsz < sizeof(sens)) return -1;
    memcpy(setting, &sens, sizeof(sens));
    return 0;
}

static int modules_sens_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t sens;
    if (maxsz < sizeof(sens)) return -1;
    memcpy(&sens, setting, sizeof(sens));
#if defined(KEYBOARD_ergohaven_hpd_rev3)
    switch (proto->qsid) {
        case 124:
        case 127:
            set_hpd3_side_sens(hpd3_side_for_qsid(proto->qsid), POINTING_MODE_SNIPER, sens);
            break;
        case 125:
        case 128:
            set_hpd3_side_sens(hpd3_side_for_qsid(proto->qsid), POINTING_MODE_SCROLL, sens);
            break;
        case 126:
        case 129:
            set_hpd3_side_sens(hpd3_side_for_qsid(proto->qsid), POINTING_MODE_TEXT, sens);
            break;
        default:
            return -1;
    }
#else
    switch (proto->qsid) {
        case 124:
            set_sniper_sens(sens);
            break;
        case 125:
            set_scroll_sens(sens);
            break;
        case 126:
            set_text_sens(sens);
            break;
        default:
            return -1;
    }
#endif
    return 0;
}

static int modules_bool_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    bool value = false;
#if defined(KEYBOARD_ergohaven_hpd_rev3)
    switch (proto->qsid) {
        case 136: value = get_invert_scroll(); break;
        case 137: value = get_acceleration(); break;
        case 138: value = get_sticky_mode(); break;
        case 139: value = get_led_blinks(); break;
        default: return -1;
    }
#else
    switch (proto->qsid) {
        case 127: value = get_invert_scroll(); break;
        case 128: value = get_acceleration(); break;
        case 133: value = get_sticky_mode(); break;
        case 134: value = get_led_blinks(); break;
        default: return -1;
    }
#endif
    if (maxsz < sizeof(value)) return -1;
    memcpy(setting, &value, sizeof(value));
    return 0;
}

static int modules_bool_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    bool value;
    if (maxsz < sizeof(value)) return -1;
    memcpy(&value, setting, sizeof(value));
#if defined(KEYBOARD_ergohaven_hpd_rev3)
    switch (proto->qsid) {
        case 136: set_invert_scroll(value); break;
        case 137: set_acceleration(value); break;
        case 138: set_sticky_mode(value); break;
        case 139: set_led_blinks(value); break;
        default: return -1;
    }
#else
    switch (proto->qsid) {
        case 127: set_invert_scroll(value); break;
        case 128: set_acceleration(value); break;
        case 133: set_sticky_mode(value); break;
        case 134: set_led_blinks(value); break;
        default: return -1;
    }
#endif
    return 0;
}

static int modules_select_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t v = 0;
#if defined(KEYBOARD_ergohaven_hpd_rev3)
    switch (proto->qsid) {
        case 130:
        case 131:
        case 132:
        case 133:
            v = (uint8_t)get_hpd3_device_orientation(hpd3_device_for_qsid(proto->qsid));
            break;
        case 134:
        case 135:
            v = (uint8_t)get_hpd3_side_mode(hpd3_side_for_qsid(proto->qsid));
            break;
        default: return -1;
    }
#else
    switch (proto->qsid) {
        case 129:
        case 130:
        case 131:
        case 132:
            v = (uint8_t)get_hpd3_device_orientation(hpd3_device_for_qsid(proto->qsid));
            break;
        case 135:
            v = (uint8_t)get_pointing_mode();
            break;
        default:
            return -1;
    }
#endif
    if (maxsz < sizeof(v)) return -1;
    memcpy(setting, &v, sizeof(v));
    return 0;
}

static int modules_select_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t v;
    if (maxsz < sizeof(v)) return -1;
    memcpy(&v, setting, sizeof(v));
#if defined(KEYBOARD_ergohaven_hpd_rev3)
    switch (proto->qsid) {
        case 130:
        case 131:
        case 132:
        case 133:
            set_hpd3_device_orientation(hpd3_device_for_qsid(proto->qsid), (orientation_t)v);
            break;
        case 134:
        case 135:
            set_hpd3_side_mode(hpd3_side_for_qsid(proto->qsid), (pointing_mode_t)v);
            break;
        default: return -1;
    }
#else
    switch (proto->qsid) {
        case 129:
        case 130:
        case 131:
        case 132:
            set_hpd3_device_orientation(hpd3_device_for_qsid(proto->qsid), (orientation_t)v);
            break;
        case 135:
            set_pointing_mode((pointing_mode_t)v);
            break;
        default:
            return -1;
    }
#endif
    return 0;
}

qmk_settings_proto_t kb_protos[KB_SETTINGS_NPROTOS] PROGMEM = {
    // clang-format off
    DECLARE_SETTING(100, ruen_toggle_get, ruen_toggle_set),
    DECLARE_SETTING(101, ruen_macos_get, ruen_macos_set),
    DECLARE_SETTING(102, unicode_get, unicode_set),
    DECLARE_SETTING(120, modules_trackball_dpi_get, modules_trackball_dpi_set),
    DECLARE_SETTING(121, modules_trackball_dpi_get, modules_trackball_dpi_set),
    DECLARE_SETTING(122, modules_trackball_dpi_get, modules_trackball_dpi_set),
    DECLARE_SETTING(123, modules_trackball_dpi_get, modules_trackball_dpi_set),
#if defined(KEYBOARD_ergohaven_hpd_rev3)
    DECLARE_SETTING(124, modules_sens_get, modules_sens_set),
    DECLARE_SETTING(125, modules_sens_get, modules_sens_set),
    DECLARE_SETTING(126, modules_sens_get, modules_sens_set),
    DECLARE_SETTING(127, modules_sens_get, modules_sens_set),
    DECLARE_SETTING(128, modules_sens_get, modules_sens_set),
    DECLARE_SETTING(129, modules_sens_get, modules_sens_set),
    DECLARE_SETTING(130, modules_select_get, modules_select_set),
    DECLARE_SETTING(131, modules_select_get, modules_select_set),
    DECLARE_SETTING(132, modules_select_get, modules_select_set),
    DECLARE_SETTING(133, modules_select_get, modules_select_set),
    DECLARE_SETTING(134, modules_select_get, modules_select_set),
    DECLARE_SETTING(135, modules_select_get, modules_select_set),
    DECLARE_SETTING(136, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(137, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(138, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(139, modules_bool_get, modules_bool_set),
#else
    DECLARE_SETTING(124, modules_sens_get, modules_sens_set),
    DECLARE_SETTING(125, modules_sens_get, modules_sens_set),
    DECLARE_SETTING(126, modules_sens_get, modules_sens_set),
    DECLARE_SETTING(127, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(128, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(129, modules_select_get, modules_select_set),
    DECLARE_SETTING(130, modules_select_get, modules_select_set),
    DECLARE_SETTING(131, modules_select_get, modules_select_set),
    DECLARE_SETTING(132, modules_select_get, modules_select_set),
    DECLARE_SETTING(133, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(134, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(135, modules_select_get, modules_select_set),
    DECLARE_SETTING(136, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(137, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(138, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(139, modules_bool_get, modules_bool_set),
#endif
    DECLARE_SETTING(200, layer_name_get, layer_name_set),
    DECLARE_SETTING(201, layer_name_get, layer_name_set),
    DECLARE_SETTING(202, layer_name_get, layer_name_set),
    DECLARE_SETTING(203, layer_name_get, layer_name_set),
    DECLARE_SETTING(204, layer_name_get, layer_name_set),
    DECLARE_SETTING(205, layer_name_get, layer_name_set),
    DECLARE_SETTING(206, layer_name_get, layer_name_set),
    DECLARE_SETTING(207, layer_name_get, layer_name_set),
    DECLARE_SETTING(208, layer_name_get, layer_name_set),
    DECLARE_SETTING(209, layer_name_get, layer_name_set),
    DECLARE_SETTING(210, layer_name_get, layer_name_set),
    DECLARE_SETTING(211, layer_name_get, layer_name_set),
    DECLARE_SETTING(212, layer_name_get, layer_name_set),
    DECLARE_SETTING(213, layer_name_get, layer_name_set),
    DECLARE_SETTING(214, layer_name_get, layer_name_set),
    DECLARE_SETTING(215, layer_name_get, layer_name_set),
    // clang-format on
};
