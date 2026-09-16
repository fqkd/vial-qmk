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
    clear(); codex_key(10, true); codex_key(10, false); codex_encoder(true);
    assert(strstr(output, "ACT10") && strstr(output, "\"act\":0") && strstr(output, "ENC_CW"));
    // Deterministic malformed input smoke fuzz under ASan/UBSan.
    for (unsigned i = 0; i < 20000; ++i) {
        uint8_t r[64]; for (unsigned j = 0; j < sizeof(r); ++j) r[j] = rand() & 255;
        r[0] = 6; r[1] = 2; r[2] %= 62; clear(); codex_receive(r, sizeof(r), 4000 + i);
    }
    codex_reset(); assert(!codex_seen_host() && codex_slots()[0].color == 0);
    puts("Codex protocol tests passed");
}
