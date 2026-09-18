// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char output[8192];
static size_t output_size;
static void capture(const uint8_t r[64]) {
    assert(r[0] == 6 && r[1] == 2 && r[2] <= 61);
    assert(output_size + r[2] < sizeof(output));
    memcpy(output + output_size, r + 3, r[2]); output_size += r[2]; output[output_size] = 0;
}
static void clear(void) { output_size = 0; output[0] = 0; }
static void input(const char *s, unsigned chunk, uint32_t now) {
    while (*s) {
        uint8_t r[64] = {6, 2, 0};
        while (*s && r[2] < chunk) { r[3 + r[2]] = (uint8_t)*s++; ++r[2]; }
        codex_receive(r, 64, now);
    }
}
int main(void) {
    codex_init(capture);
    // Every fragmentation boundary, including CR/LF split across reports.
    for (unsigned chunk = 1; chunk <= 61; ++chunk) {
        codex_reset(); clear();
        input("{\"method\":\"device.status\",\"id\":42}\r\n", chunk, 100);
        assert(strstr(output, "\"id\":42") && strstr(output, "\"layer_index\":1"));
        assert(codex_seen_host());
    }
    // Windows device-kit sends bare JSON, without CRLF, over the same HID
    // reports. Exercise every split, nesting, strings and escaped quotes.
    for (unsigned chunk = 1; chunk <= 61; ++chunk) {
        codex_reset(); clear();
        input("{\"method\":\"device.status\",\"params\":null,\"id\":42}", chunk, 110);
        assert(codex_seen_host() && strstr(output, "\"id\":42"));
        clear();
        input("{\"method\":\"v.oai.rgbcfg\",\"params\":{\"keys\":{\"c\":255,\"b\":1,\"e\":1},\"ambient\":{}},\"id\":43}", chunk, 111);
        assert(strstr(output, "\"id\":43") && !strstr(output, "error"));
        assert(codex_keys_light()->color == 255);
        clear();
        input("{\"method\":\"v.oai.thstatus\",\"params\":[{\"id\":0,\"c\":65280,\"b\":1,\"e\":1}],\"id\":44}", chunk, 112);
        assert(strstr(output, "\"id\":44") && codex_slots()[0].color == 65280);
        clear();
        input("{\"method\":\"device.status\",\"params\":{\"text\":\"} \\\" { \\\\ end\"},\"id\":45", chunk, 113);
        assert(!output_size); // No response before the top-level object closes.
        input("}", chunk, 114);
        assert(strstr(output, "\"id\":45"));
        clear();
        input("{\"m\":\"sys.version\",\"id\":46}{\"m\":\"sys.version\",\"id\":47}\r\n", chunk, 115);
        assert(strstr(output, "\"id\":46") && strstr(output, "\"id\":47"));
        const char *second_line = strchr(output, '\n') + 1;
        assert(strchr(second_line, '\n') && !strchr(strchr(second_line, '\n') + 1, '\n'));
    }
    clear();
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":0,\"c\":65280,\"b\":1,\"e\":1}]}\r\n", 61, 200);
    assert(!output_size && codex_slots()[0].color == 65280);
    // Invalid later slot must not partially apply the earlier slot.
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":0,\"c\":255},{\"id\":6}],\"id\":9}\r\n", 7, 300);
    assert(codex_slots()[0].color == 65280 && strstr(output, "Invalid params"));
    clear();
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":null,\"c\":1}],\"id\":9}\r\n", 61, 301);
    assert(codex_slots()[0].color == 65280 && strstr(output, "Invalid params"));
    clear();
    input("{\"m\":\"sys.bootloader\",\"id\":\"hello\"}\r\n", 61, 400);
    assert(strstr(output, "Method not found") && strstr(output, "\"id\":\"hello\""));
    clear();
    input("{\"m\":\"device.status\",}\r\n", 61, 500);
    assert(!output_size);
    input("{\"m\":\"device.status\"", 61, 600);
    codex_tick(1601);
    input("{\"m\":\"sys.version\",\"id\":1}\r\n", 61, 1602);
    assert(strstr(output, "v0.4.1"));
    clear();
    char huge[4096]; memset(huge, 'x', sizeof(huge) - 3); huge[4093] = '\r'; huge[4094] = '\n'; huge[4095] = 0;
    input(huge, 61, 2000);
    input("{\"m\":\"sys.version\",\"id\":1}\r\n", 61, 2001);
    assert(strstr(output, "v0.4.1"));
    clear();
    uint8_t bad[64] = {6,2,255}; codex_receive(bad, 64, 3000);
    input("\r\n{\"m\":\"sys.version\",\"id\":1}\r\n", 61, 3001);
    assert(strstr(output, "v0.4.1"));
    clear();
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":2,\"c\":255,\"b\":1,\"e\":6,\"s\":0.5,\"sk\":1}],\"id\":50}", 13, 3100);
    assert(codex_slots()[2].speed == 127 && codex_slots()[2].sync_keys);
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":2,\"s\":0,\"sk\":0}],\"id\":51}", 61, 3101);
    assert(codex_slots()[2].speed == 0 && !codex_slots()[2].sync_keys);
    clear();
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":2,\"c\":123,\"s\":2}],\"id\":52}", 61, 3102);
    assert(strstr(output, "Invalid params") && codex_slots()[2].color == 255);
    clear();
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":2,\"sk\":2}],\"id\":53}", 61, 3103);
    assert(strstr(output, "Invalid params") && !codex_slots()[2].sync_keys);
    codex_reset(); clear();
    for (unsigned i = 0; i < 12; ++i) {
        codex_light_t l = codex_key_light(i);
        assert(l.effect && l.brightness); // All physical keys have idle light.
    }
    input("{\"m\":\"v.oai.rgbcfg\",\"p\":{\"keys\":{\"e\":0,\"b\":0}}}", 61, 3200);
    for (unsigned i = 0; i < 12; ++i) assert(!codex_key_light(i).brightness);
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":1,\"c\":255,\"b\":1,\"e\":6,\"sk\":1}]}", 61, 3201);
    for (unsigned i = 6; i < 12; ++i) {
        codex_light_t l = codex_key_light(i);
        assert(l.color == 255 && l.effect == 6 && l.brightness == 255);
    }
    assert(codex_key_light(0).brightness < codex_key_light(1).brightness);
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":1,\"b\":0}]}", 61, 3202);
    for (unsigned i = 0; i < 12; ++i) assert(!codex_key_light(i).brightness);
    assert(!codex_key_light(12).effect);
    clear(); codex_key(10, true); codex_key(10, false); codex_encoder(true);
    assert(strstr(output, "ACT10") && strstr(output, "\"act\":0") && strstr(output, "ENC_CW"));
    // Status-change notifications: initial state is quiet, duplicate updates
    // cannot restart the five pulses, and all twelve keys blink together.
    codex_reset(); clear();
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":0,\"c\":255,\"b\":1,\"e\":6,\"sk\":1}]}", 61, 5000);
    assert(codex_animated_key_light(0, 5250).brightness == 255);
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":0,\"c\":65280}]}", 7, 6000);
    for (unsigned phase = 0; phase < 10; ++phase) {
        for (unsigned key = 0; key < 12; ++key) {
            codex_light_t l = codex_animated_key_light(key, 6000 + phase * 250);
            assert(l.color == 65280 && l.effect == 1);
            assert(l.brightness == (phase % 2 ? 0 : 255));
        }
    }
    assert(codex_animated_key_light(6, 6250).brightness == 0);
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":0,\"c\":65280,\"b\":0.5}]}", 61, 8400);
    assert(codex_animated_key_light(0, 8499).brightness == 0);
    assert(codex_animated_key_light(0, 8500).effect == 1);
    assert(codex_animated_key_light(0, 8500).brightness == 127);
    // Invalid atomic update must not start a notification.
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":0,\"c\":255},{\"id\":8}]}", 61, 8600);
    assert(codex_animated_key_light(0, 8850).brightness == 127);
    // A new color restarts the pulse; host blackout cancels it immediately.
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":0,\"c\":255}]}", 61, 9000);
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":0,\"c\":16711680}]}", 61, 9250);
    assert(codex_animated_key_light(0, 9250).brightness == 127);
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":0,\"b\":0}]}", 61, 9260);
    assert(codex_animated_key_light(0, 9260).brightness == 100);
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":0,\"b\":1}]}", 61, 9270);
    assert(codex_animated_key_light(0, 9520).brightness == 255);
    // Unsigned timer wrap must preserve pulse timing.
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":0,\"c\":255}]}", 61, UINT32_MAX - 100);
    assert(codex_animated_key_light(0, 149).brightness == 0);
    assert(codex_animated_key_light(0, 2398).brightness == 0);
    assert(codex_animated_key_light(0, 2399).effect == 1);
    assert(codex_animated_key_light(0, 2399).brightness == 255);
    codex_reset();
    assert(codex_animated_key_light(0, 1500).effect == 1);
    // Host blackout still leaves all twelve keys steadily visible.
    input("{\"m\":\"v.oai.rgbcfg\",\"p\":{\"keys\":{\"e\":0,\"b\":0}}}", 61, 1600);
    for (unsigned i = 0; i < 12; ++i) {
        codex_light_t l = codex_animated_key_light(i, 1700);
        assert(l.effect == 1 && l.brightness == 100 && l.color == 0x607080);
    }
    assert(!codex_animated_key_light(12, 1700).effect);
    // A change from breathing to solid can signify completion at the same color.
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":0,\"c\":255,\"b\":1,\"e\":6}]}", 61, 1800);
    assert(codex_animated_key_light(0, 1900).effect == 1);
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":0,\"e\":1}]}", 61, 2000);
    assert(codex_animated_key_light(0, 2250).brightness == 0);
    assert(codex_animated_key_light(0, 4499).brightness == 0);
    assert(codex_animated_key_light(0, 4500).brightness == 255);
    // A second task takes over the whole matrix, never an independent phase.
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":1,\"c\":255,\"b\":1,\"e\":1}]}", 61, 5000);
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":0,\"c\":65280}]}", 61, 5100);
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":1,\"c\":16711680}]}", 61, 5450);
    // An unrelated inactive slot must not cancel the shared notification.
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":2,\"e\":0}]}", 61, 5500);
    for (unsigned phase = 0; phase < 10; ++phase) {
        codex_light_t overlay;
        assert(codex_notification_light(5450 + phase * 250, &overlay));
        for (unsigned key = 0; key < 12; ++key) {
            codex_light_t l = codex_animated_key_light(key, 5450 + phase * 250);
            assert(l.color == 16711680 && l.effect == 1);
            assert(l.brightness == (phase % 2 ? 0 : 255));
            assert(l.color == overlay.color && l.brightness == overlay.brightness);
        }
    }
    codex_light_t overlay;
    assert(!codex_notification_light(7950, &overlay));
    assert(codex_animated_key_light(0, 7950).color == 65280);
    assert(codex_animated_key_light(1, 7950).color == 16711680);
    assert(codex_animated_key_light(11, 7950).color == 0x607080);
    input("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":1,\"c\":255}]}", 61, 8000);
    codex_reset();
    assert(!codex_notification_light(8100, &overlay));
    // Deterministic malformed input smoke fuzz under ASan/UBSan.
    for (unsigned i = 0; i < 20000; ++i) {
        uint8_t r[64]; for (unsigned j = 0; j < sizeof(r); ++j) r[j] = rand() & 255;
        r[0] = 6; r[1] = 2; r[2] %= 62; clear(); codex_receive(r, sizeof(r), 4000 + i);
    }
    codex_reset(); assert(!codex_seen_host() && codex_slots()[0].color == 0);
    puts("Codex protocol tests passed");
}
