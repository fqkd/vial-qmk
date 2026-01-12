#include "eh_settings.h"
#include "ergohaven_ruen.h"
#include <eeconfig.h>
#include <debug.h>
#include <qmk_settings.h>

typedef union {
    uint32_t raw;
    struct {
        uint8_t ruen_toggle_mode : 2;
        bool    ruen_mac_layout : 1;
    };
} kb_config_t;

kb_config_t kb_config;

#define KB_CFG_SIZE (sizeof(kb_config))

char layer_names[DYNAMIC_KEYMAP_LAYER_COUNT][LAYER_LABEL_SIZE];

void kb_config_update(kb_config_t new_config) {
    if (new_config.raw != kb_config.raw) {
        kb_config = new_config;
        dprintf("kb_config_update %ld \n", kb_config.raw);
        eeconfig_update_kb_datablock(&kb_config, 0, sizeof(kb_config_t));
    }
}

void kb_settings_ruen_toggle_mode_set(uint8_t mode) {
    kb_config_t new_config      = kb_config;
    new_config.ruen_toggle_mode = mode;
    kb_config_update(new_config);
}

void kb_settings_ruen_mac_layout_set(bool mac_layout) {
    kb_config_t new_config     = kb_config;
    new_config.ruen_mac_layout = mac_layout;
    kb_config_update(new_config);
}

uint8_t kb_settings_ruen_toggle_mode() {
    return kb_config.ruen_toggle_mode;
}

bool kb_settings_ruen_mac_layout() {
    return kb_config.ruen_mac_layout;
}

__attribute__((weak)) const char *default_layer_label(uint8_t layer) {
    static const char *PROGMEM default_layer_labels[] = {
        "BASE", "LOWER", "RAISE", "ADJST", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVN", "TWLVE", "THRTN", "FRTN", "FIFTN",
    };
    return default_layer_labels[layer];
}

void kb_settings_reset_layer_labels(void) {
    for (int i = 0; i < DYNAMIC_KEYMAP_LAYER_COUNT; ++i) {
        eeconfig_update_kb_datablock(default_layer_label(i), KB_CFG_SIZE + i * LAYER_LABEL_SIZE, LAYER_LABEL_SIZE);
    }
    layer_name_updated = true;
}

void kb_settings_reset(void) {
    kb_settings_reset_layer_labels();
    kb_settings_init();
}

void eeconfig_init_kb(void) {
    kb_settings_reset();
    eeconfig_init_user();
}

void kb_settings_init(void) {
    eeconfig_read_kb_datablock(&kb_config, 0, sizeof(kb_config_t));
    for (int i = 0; i < DYNAMIC_KEYMAP_LAYER_COUNT; ++i)
        eeconfig_read_kb_datablock(layer_names[i], KB_CFG_SIZE + i * LAYER_LABEL_SIZE, LAYER_LABEL_SIZE);
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
    eeconfig_update_kb_datablock(layer_names[layer], KB_CFG_SIZE + LAYER_LABEL_SIZE * layer, LAYER_LABEL_SIZE);
    layer_name_updated = true;
    return 0;
}

qmk_settings_proto_t kb_protos[KB_SETTINGS_NPROTOS] PROGMEM = {
    // clang-format off
    DECLARE_SETTING(100, ruen_toggle_get, ruen_toggle_set),
    DECLARE_SETTING(101, ruen_macos_get, ruen_macos_set),
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
