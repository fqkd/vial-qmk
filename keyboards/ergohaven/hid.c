#include "hid.h"
#include <string.h>
#include "via.h"

static hid_data_t hid_data;

hid_data_t *get_hid_data(void) {
    return &hid_data;
}

static uint32_t hid_sync_time = 0;

bool is_hid_active(void) {
    return (hid_sync_time != 0) && timer_elapsed32(hid_sync_time) < 61 * 1000;
}

typedef enum {
    _TIME = 0xAA, // random value that does not conflict with VIA, must match companion app
    _VOLUME,
    _LAYOUT,
    _MEDIA_ARTIST,
    _MEDIA_TITLE,
} hid_data_type;

void read_string(uint8_t *data, char *string_data) {
    uint8_t data_length = MIN(31, data[1]);
    memcpy(string_data, data + 2, data_length);
    string_data[data_length] = '\0';
}

bool process_raw_hid_data(uint8_t *data, uint8_t length) {
    uint8_t data_type = data[0];

    bool new_hid_data = false;

    switch (data_type) {
        case _TIME:
            hid_data.hours        = data[1];
            hid_data.minutes      = data[2];
            hid_data.time_changed = true;
            new_hid_data          = true;
            break;

        case _VOLUME:
            hid_data.volume         = data[1];
            hid_data.volume_changed = true;
            new_hid_data            = true;
            break;

        case _LAYOUT:
            hid_data.layout         = data[1];
            hid_data.layout_changed = true;
            new_hid_data            = true;
            break;

        case _MEDIA_ARTIST:
            read_string(data, hid_data.media_artist);
            hid_data.media_artist_changed = true;
            new_hid_data                  = true;
            break;

        case _MEDIA_TITLE:
            read_string(data, hid_data.media_title);
            hid_data.media_title_changed = true;
            new_hid_data                 = true;
            break;

        default:
            break;
    }

    if (new_hid_data) {
        hid_sync_time        = timer_read32();
        hid_data.hid_changed = new_hid_data;
    }

    return new_hid_data;
}

#if (defined(OLED_ENABLE) || defined(EH_HAS_DISPLAY)) && defined(SPLIT_KEYBOARD)
#    include "transactions.h"

void raw_hid_receive_kb(uint8_t *data, uint8_t length) {
    bool res = process_raw_hid_data(data, length);
    if (res && is_keyboard_master()) transaction_rpc_send(RPC_SYNC_HID, length, data);
    if (res) *((uint64_t *)data) = VIAL_HID_MAGIC;
}

void hid_sync(uint8_t in_buflen, const void *in_data, uint8_t out_buflen, void *out_data) {
    process_raw_hid_data((uint8_t *)in_data, out_buflen);
}

void keyboard_post_init_hid(void) {
    transaction_register_rpc(RPC_SYNC_HID, hid_sync);
}

#else

void raw_hid_receive_kb(uint8_t *data, uint8_t length) {
    bool res = process_raw_hid_data(data, length);
    if (res) *((uint64_t *)data) = VIAL_HID_MAGIC;
}

void keyboard_post_init_hid(void) {}

#endif
