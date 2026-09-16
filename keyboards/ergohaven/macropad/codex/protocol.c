// SPDX-License-Identifier: GPL-2.0-or-later
// Independent implementation of the publicly documented Micro HID wire format.
#include "protocol.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uint16_t start, end, next; char type; } token_t;
static token_t tokens[256];
static unsigned nt, pos;
static char rx[CODEX_MESSAGE_BYTES];
static size_t used;
static bool discard, host_seen;
static unsigned nesting;
static bool in_string, escaped;
static uint32_t last_byte, last_rx;
static codex_light_t slots[CODEX_SLOT_COUNT], keys;
static codex_send_fn send_report;

static void ws(void) { while (rx[pos] == ' ' || rx[pos] == '\t' || rx[pos] == '\r' || rx[pos] == '\n') ++pos; }
static int value(unsigned depth) {
    ws();
    if (depth > 12 || nt >= 256 || !rx[pos]) return -1;
    unsigned i = nt++;
    token_t *t = &tokens[i];
    t->start = pos; t->type = rx[pos];
    if (rx[pos] == '{' || rx[pos] == '[') {
        bool object = rx[pos++] == '{';
        char close = object ? '}' : ']';
        ws();
        if (rx[pos] != close) for (;;) {
            if (object) {
                ws();
                if (rx[pos] != '"' || value(depth + 1) < 0) return -1;
                ws(); if (rx[pos++] != ':') return -1;
            }
            if (value(depth + 1) < 0) return -1;
            ws();
            if (rx[pos] == close) break;
            if (rx[pos++] != ',') return -1;
        }
        ++pos;
    } else if (rx[pos] == '"') {
        ++pos;
        while (rx[pos] && rx[pos] != '"') {
            if ((unsigned char)rx[pos] < 32) return -1;
            if (rx[pos++] == '\\') {
                char e = rx[pos++];
                if (e == 'u') { for (int j = 0; j < 4; ++j) if (!isxdigit((unsigned char)rx[pos++])) return -1; }
                else if (!e || !strchr("\"\\/bfnrt", e)) return -1;
            }
        }
        if (rx[pos++] != '"') return -1;
    } else if (rx[pos] == '-' || isdigit((unsigned char)rx[pos])) {
        t->type = 'n';
        if (rx[pos] == '-') ++pos;
        if (rx[pos] == '0') ++pos;
        else { if (rx[pos] < '1' || rx[pos] > '9') return -1; while (isdigit((unsigned char)rx[pos])) ++pos; }
        if (rx[pos] == '.') { ++pos; if (!isdigit((unsigned char)rx[pos])) return -1; while (isdigit((unsigned char)rx[pos])) ++pos; }
        if (rx[pos] == 'e' || rx[pos] == 'E') {
            ++pos; if (rx[pos] == '+' || rx[pos] == '-') ++pos;
            if (!isdigit((unsigned char)rx[pos])) return -1;
            while (isdigit((unsigned char)rx[pos])) ++pos;
        }
    } else {
        t->type = 'l';
        const char *lit = rx[pos] == 't' ? "true" : rx[pos] == 'f' ? "false" : "null";
        size_t n = strlen(lit);
        if (strncmp(rx + pos, lit, n)) return -1;
        pos += n;
    }
    t->end = pos; t->next = nt;
    return (int)i;
}
static bool eq(int i, const char *s) {
    return i >= 0 && tokens[i].type == '"' && (size_t)(tokens[i].end - tokens[i].start) == strlen(s) + 2 && !memcmp(rx + tokens[i].start + 1, s, strlen(s));
}
static int field(int object, const char *name) {
    if (object < 0 || tokens[object].type != '{') return -1;
    for (unsigned i = object + 1; i < tokens[object].next;) {
        unsigned v = tokens[i].next;
        if (eq(i, name)) return v;
        i = tokens[v].next;
    }
    return -1;
}
static bool number(int i, double *out) {
    if (i < 0 || tokens[i].type != 'n') return false;
    size_t n = tokens[i].end - tokens[i].start;
    if (n >= 32) return false;
    char text[32]; memcpy(text, rx + tokens[i].start, n); text[n] = 0;
    *out = strtod(text, NULL);
    return isfinite(*out);
}
static void emit(const char *json) {
    if (!send_report) return;
    size_t n = strlen(json), offset = 0;
    // Append CRLF as part of the stream, even across a report boundary.
    while (offset < n + 2) {
        uint8_t report[64] = {6, 2, 0};
        while (report[2] < 61 && offset < n + 2) {
            report[3 + report[2]++] = offset < n ? (uint8_t)json[offset] : offset == n ? '\r' : '\n';
            ++offset;
        }
        send_report(report);
    }
}
static bool light(int i, codex_light_t *out) {
    if (i < 0 || tokens[i].type != '{') return false;
    int c = field(i, "c"), b = field(i, "b"), e = field(i, "e");
    if (c < 0) c = field(i, "color");
    if (b < 0) b = field(i, "brightness");
    if (e < 0) e = field(i, "effect");
    double v;
    if (c >= 0) { if (!number(c, &v) || v < 0 || v > 16777215 || v != (uint32_t)v) return false; out->color = (uint32_t)v; }
    if (b >= 0) { if (!number(b, &v) || v < 0 || v > 1) return false; out->brightness = (uint8_t)(v * 255); }
    if (e >= 0) { if (!number(e, &v) || v < 0 || v > 6 || v != (uint8_t)v) return false; out->effect = (uint8_t)v; }
    return true;
}
static void message(uint32_t now) {
    nt = pos = 0;
    if (value(0) != 0) return;
    ws(); if (rx[pos] || tokens[0].type != '{') return;
    int m = field(0, "m"), p = field(0, "p"), id = field(0, "id");
    if (m < 0) m = field(0, "method");
    if (p < 0) p = field(0, "params");
    if (m < 0 || tokens[m].type != '"') return;
    char id_text[80] = {0};
    if (id >= 0) {
        unsigned n = tokens[id].end - tokens[id].start;
        if (n >= sizeof(id_text) || (tokens[id].type != 'n' && tokens[id].type != '"')) return;
        memcpy(id_text, rx + tokens[id].start, n);
    }
    host_seen = true; last_rx = now;
    const char *result = "{\"ok\":1}";
    int error = 0;
    if (eq(m, "device.status")) {
        result = "{\"version\":\"v0.4.1\",\"profile_index\":0,\"layer_index\":1,\"battery\":100,\"is_charging\":false}";
    } else if (eq(m, "sys.version")) {
        result = "\"v0.4.1\"";
    } else if (eq(m, "v.oai.thstatus")) {
        codex_light_t next[6]; memcpy(next, slots, sizeof(next));
        if (p < 0 || tokens[p].type != '[') error = -32602;
        else for (unsigned i = p + 1; i < tokens[p].next; i = tokens[i].next) {
            double slot;
            if (!number(field(i, "id"), &slot) || slot < 0 || slot > 5 || slot != (int)slot || !light(i, &next[(int)slot])) { error = -32602; break; }
        }
        if (!error) memcpy(slots, next, sizeof(slots));
    } else if (eq(m, "v.oai.rgbcfg") || eq(m, "lights.preview")) {
        int k = field(p, eq(m, "lights.preview") ? "backlight" : "keys");
        codex_light_t next = keys;
        if (p < 0 || tokens[p].type != '{' || (k >= 0 && !light(k, &next))) error = -32602;
        else keys = next;
    } else if (eq(m, "host.focused_app")) {
        result = "null";
    } else error = -32601;
    if (id >= 0) {
        char response[320];
        if (error) snprintf(response, sizeof(response), "{\"jsonrpc\":\"2.0\",\"id\":%s,\"error\":{\"code\":%d,\"message\":\"%s\"}}", id_text, error, error == -32602 ? "Invalid params" : "Method not found");
        else snprintf(response, sizeof(response), "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":%s}", id_text, result);
        emit(response);
    }
}
void codex_reset(void) {
    used = 0; discard = false; host_seen = false; last_rx = last_byte = 0;
    nesting = 0; in_string = escaped = false;
    memset(slots, 0, sizeof(slots));
    keys = (codex_light_t){0x303030, 80, 1};
}
void codex_init(codex_send_fn send) { send_report = send; codex_reset(); }
static void reset_frame(void) { used = nesting = 0; in_string = escaped = false; }
void codex_tick(uint32_t now) { if ((used || discard) && (uint32_t)(now - last_byte) > 1000) { reset_frame(); discard = false; } }
void codex_receive(const uint8_t *r, size_t n, uint32_t now) {
    codex_tick(now);
    if (n != 64 || r[0] != 6 || r[1] != 2 || r[2] > 61) { reset_frame(); discard = true; last_byte = now; return; }
    last_byte = now;
    for (unsigned i = 0; i < r[2]; ++i) {
        char ch = (char)r[i + 3];
        if (discard) { if (ch == '\n') discard = false; continue; }
        if (ch == '\n') {
            if (used && rx[used - 1] == '\r') { rx[--used] = 0; message(now); }
            reset_frame(); continue;
        }
        if (!ch || used >= sizeof(rx) - 1) { reset_frame(); discard = true; continue; }
        if (!used && (ch == ' ' || ch == '\t' || ch == '\r')) continue;
        rx[used++] = ch;
        // The Windows SDK sends JSON without a line terminator. Track lexical
        // boundaries across HID chunks; only the strict parser applies state.
        // Do not confuse braces inside strings (including escaped quotes) with
        // the end of the top-level object. Responses remain CRLF terminated.
        if (in_string) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') in_string = false;
        } else if (ch == '"') in_string = true;
        else if (ch == '{' || ch == '[') ++nesting;
        else if ((ch == '}' || ch == ']') && nesting) {
            if (--nesting == 0) {
                rx[used] = 0;
                message(now);
                reset_frame();
            }
        }
    }
}
void codex_key(uint8_t key, bool pressed) {
    static const char *const names[] = {"AG00", "AG01", "AG02", "AG03", "AG04", "AG05", "ACT06", "ACT07", "ACT08", "ACT09", "ACT10", "ACT12", "ENC"};
    if (key >= sizeof(names) / sizeof(names[0])) return;
    char out[100];
    snprintf(out, sizeof(out), "{\"m\":\"v.oai.hid\",\"p\":{\"k\":\"%s\",\"act\":%d,\"ag\":%d}}", names[key], pressed ? 1 : 0, key < 6 ? key : -1);
    emit(out);
}
void codex_encoder(bool clockwise) { emit(clockwise ? "{\"m\":\"v.oai.hid\",\"p\":{\"k\":\"ENC_CW\",\"act\":2,\"ag\":-1}}" : "{\"m\":\"v.oai.hid\",\"p\":{\"k\":\"ENC_CC\",\"act\":2,\"ag\":-1}}"); }
bool codex_seen_host(void) { return host_seen; }
uint32_t codex_last_rx(void) { return last_rx; }
const codex_light_t *codex_slots(void) { return slots; }
const codex_light_t *codex_keys_light(void) { return &keys; }
