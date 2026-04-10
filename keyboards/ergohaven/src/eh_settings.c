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
static const uint8_t  hpd3_sniper_table[]        = {1, 2, 4, 8, 12, 16, 24, 32};
static const uint8_t  hpd3_scroll_table[]        = {2, 4, 8, 16, 24, 32, 48, 64};
static const uint8_t  hpd3_text_table[]          = {1, 2, 4, 8, 16, 24, 32, 48};

static uint32_t hpd3_layout_raw(void) {
    return via_get_layout_options();
}

static void hpd3_layout_write(uint32_t raw) {
    via_set_layout_options_kb(raw);
}

static uint32_t hpd3_field_mask(uint8_t width) {
    return width >= 32 ? 0xFFFFFFFFu : ((1u << width) - 1u);
}

static uint32_t hpd3_field_get(uint8_t shift, uint8_t width) {
    return (hpd3_layout_raw() >> shift) & hpd3_field_mask(width);
}

static void hpd3_field_set(uint8_t shift, uint8_t width, uint32_t value) {
    uint32_t raw  = hpd3_layout_raw();
    uint32_t mask = hpd3_field_mask(width) << shift;
    raw            = (raw & ~mask) | ((value & hpd3_field_mask(width)) << shift);
    hpd3_layout_write(raw);
}

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

static uint8_t hpd3_index_from_value_u8(const uint8_t *table, size_t count, uint8_t value) {
    uint8_t best    = 0;
    uint8_t best_d  = UINT8_MAX;
    for (size_t i = 0; i < count; ++i) {
        uint8_t d = table[i] > value ? table[i] - value : value - table[i];
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

static uint8_t hpd3_value_from_index_u8(const uint8_t *table, size_t count, uint8_t idx) {
    if (idx >= count) idx = (uint8_t)(count - 1);
    return table[idx];
}

static int modules_trackball_dpi_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint16_t cpi = hpd3_value_from_index_u16(hpd3_trackball_cpi_table, ARRAY_SIZE(hpd3_trackball_cpi_table), hpd3_field_get(10, 4));
    if (maxsz < sizeof(cpi)) return -1;
    memcpy(setting, &cpi, sizeof(cpi));
    return 0;
}

static int modules_trackball_dpi_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint16_t cpi;
    if (maxsz < sizeof(cpi)) return -1;
    memcpy(&cpi, setting, sizeof(cpi));
    hpd3_field_set(10, 4, hpd3_index_from_value_u16(hpd3_trackball_cpi_table, ARRAY_SIZE(hpd3_trackball_cpi_table), cpi));
    return 0;
}

static int modules_touchpad_dpi_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint16_t cpi = hpd3_value_from_index_u16(hpd3_touchpad_cpi_table, ARRAY_SIZE(hpd3_touchpad_cpi_table), hpd3_field_get(14, 3));
    if (maxsz < sizeof(cpi)) return -1;
    memcpy(setting, &cpi, sizeof(cpi));
    return 0;
}

static int modules_touchpad_dpi_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint16_t cpi;
    if (maxsz < sizeof(cpi)) return -1;
    memcpy(&cpi, setting, sizeof(cpi));
    hpd3_field_set(14, 3, hpd3_index_from_value_u16(hpd3_touchpad_cpi_table, ARRAY_SIZE(hpd3_touchpad_cpi_table), cpi));
    return 0;
}

static int modules_sens_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t sens = 0;
    switch (proto->qsid) {
        case 122:
            sens = hpd3_value_from_index_u8(hpd3_sniper_table, ARRAY_SIZE(hpd3_sniper_table), hpd3_field_get(17, 3));
            break;
        case 123:
            sens = hpd3_value_from_index_u8(hpd3_scroll_table, ARRAY_SIZE(hpd3_scroll_table), hpd3_field_get(20, 3));
            break;
        case 124:
            sens = hpd3_value_from_index_u8(hpd3_text_table, ARRAY_SIZE(hpd3_text_table), hpd3_field_get(23, 3));
            break;
        default:
            return -1;
    }
    if (maxsz < sizeof(sens)) return -1;
    memcpy(setting, &sens, sizeof(sens));
    return 0;
}

static int modules_sens_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t sens;
    if (maxsz < sizeof(sens)) return -1;
    memcpy(&sens, setting, sizeof(sens));
    switch (proto->qsid) {
        case 122:
            hpd3_field_set(17, 3, hpd3_index_from_value_u8(hpd3_sniper_table, ARRAY_SIZE(hpd3_sniper_table), sens));
            break;
        case 123:
            hpd3_field_set(20, 3, hpd3_index_from_value_u8(hpd3_scroll_table, ARRAY_SIZE(hpd3_scroll_table), sens));
            break;
        case 124:
            hpd3_field_set(23, 3, hpd3_index_from_value_u8(hpd3_text_table, ARRAY_SIZE(hpd3_text_table), sens));
            break;
        default:
            return -1;
    }
    return 0;
}

static int modules_bool_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    bool value = false;
    switch (proto->qsid) {
        case 125: value = hpd3_field_get(26, 1); break;
        case 126: value = hpd3_field_get(27, 1); break;
        case 127: value = get_sticky_mode(); break;
        case 128: value = hpd3_field_get(28, 1); break;
        default: return -1;
    }
    if (maxsz < sizeof(value)) return -1;
    memcpy(setting, &value, sizeof(value));
    return 0;
}

static int modules_bool_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    bool value;
    if (maxsz < sizeof(value)) return -1;
    memcpy(&value, setting, sizeof(value));
    switch (proto->qsid) {
        case 125: hpd3_field_set(26, 1, value); break;
        case 126: hpd3_field_set(27, 1, value); break;
        case 127: set_sticky_mode(value); break;
        case 128: hpd3_field_set(28, 1, value); break;
        default: return -1;
    }
    return 0;
}

static int modules_select_get(const qmk_settings_proto_t *proto, void *setting, size_t maxsz) {
    uint8_t v = 0;
    switch (proto->qsid) {
        case 129: v = hpd3_field_get(2, 2); break;
        case 130: v = hpd3_field_get(4, 2); break;
        case 131: v = hpd3_field_get(6, 2); break;
        case 132: v = hpd3_field_get(8, 2); break;
        case 133: v = (uint8_t)get_pointing_mode(); break;
        default: return -1;
    }
    if (maxsz < sizeof(v)) return -1;
    memcpy(setting, &v, sizeof(v));
    return 0;
}

static int modules_select_set(const qmk_settings_proto_t *proto, const void *setting, size_t maxsz) {
    uint8_t v;
    if (maxsz < sizeof(v)) return -1;
    memcpy(&v, setting, sizeof(v));
    switch (proto->qsid) {
        case 129: hpd3_field_set(2, 2, v); break;
        case 130: hpd3_field_set(4, 2, v); break;
        case 131: hpd3_field_set(6, 2, v); break;
        case 132: hpd3_field_set(8, 2, v); break;
        case 133: set_pointing_mode((pointing_mode_t)v); break;
        default: return -1;
    }
    return 0;
}

qmk_settings_proto_t kb_protos[KB_SETTINGS_NPROTOS] PROGMEM = {
    // clang-format off
    DECLARE_SETTING(100, ruen_toggle_get, ruen_toggle_set),
    DECLARE_SETTING(101, ruen_macos_get, ruen_macos_set),
    DECLARE_SETTING(102, unicode_get, unicode_set),
    DECLARE_SETTING(120, modules_trackball_dpi_get, modules_trackball_dpi_set),
    DECLARE_SETTING(121, modules_touchpad_dpi_get, modules_touchpad_dpi_set),
    DECLARE_SETTING(122, modules_sens_get, modules_sens_set),
    DECLARE_SETTING(123, modules_sens_get, modules_sens_set),
    DECLARE_SETTING(124, modules_sens_get, modules_sens_set),
    DECLARE_SETTING(125, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(126, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(127, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(128, modules_bool_get, modules_bool_set),
    DECLARE_SETTING(129, modules_select_get, modules_select_set),
    DECLARE_SETTING(130, modules_select_get, modules_select_set),
    DECLARE_SETTING(131, modules_select_get, modules_select_set),
    DECLARE_SETTING(132, modules_select_get, modules_select_set),
    DECLARE_SETTING(133, modules_select_get, modules_select_set),
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
