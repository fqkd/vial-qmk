// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "../v3/config.h"

// Experimental wire compatibility identity. Not an assigned Ergohaven VID/PID.
// Required discovery compatibility must be verified on the target Windows app.
#undef VENDOR_ID
#define VENDOR_ID 0x303A
#undef PRODUCT_ID
#define PRODUCT_ID 0x8360
#undef MANUFACTURER
#define MANUFACTURER "Work Louder"
#undef PRODUCT
#define PRODUCT "Codex Micro"

// Host owns dimming in this keymap. Avoid writing status updates to EEPROM.
#undef EH_RGB_MATRIX_RUNTIME_TIMEOUT
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 100
