// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CODEX_REPORT_BYTES 64
#define CODEX_MESSAGE_BYTES 2048
#define CODEX_SLOT_COUNT 6
typedef struct {
    uint32_t color;
    uint8_t brightness, effect;
} codex_light_t;
typedef void (*codex_send_fn)(const uint8_t report[CODEX_REPORT_BYTES]);
void codex_init(codex_send_fn send);
void codex_receive(const uint8_t *report, size_t length, uint32_t now);
void codex_tick(uint32_t now);
void codex_reset(void);
void codex_key(uint8_t key, bool pressed);
void codex_encoder(bool clockwise);
bool codex_seen_host(void);
uint32_t codex_last_rx(void);
const codex_light_t *codex_slots(void);
const codex_light_t *codex_keys_light(void);
