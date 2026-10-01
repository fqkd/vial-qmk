// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "protocol.h"
#define ENTROPY_TITLE_BYTES 96
bool entropy_tasks_receive(uint8_t *packet, uint8_t length, uint32_t now);
void entropy_tasks_reset(void);
void entropy_tasks_tick(uint32_t now);
bool entropy_tasks_active(void);
bool entropy_tasks_occupied(uint8_t slot);
bool entropy_tasks_completed(uint8_t slot);
const char *entropy_tasks_title(uint8_t slot);
const codex_light_t *entropy_tasks_lights(void);
bool entropy_tasks_notification(uint32_t now, codex_light_t *light, int slot);
void entropy_tasks_press(uint8_t slot, bool pressed);
void entropy_tasks_interaction(void);
