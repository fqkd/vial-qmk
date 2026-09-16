// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#define VIAL_KEYBOARD_UID {0xC3, 0x6A, 0xF1, 0x8B, 0x24, 0x71, 0xE0, 0x92}
#define VIAL_UNLOCK_COMBO_ROWS { 1, 1 }
#define VIAL_UNLOCK_COMBO_COLS { 1, 2 }
#define ENCODER_RESOLUTION 4
#define ENCODER_DEFAULT_POS 0x3

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

// Separate definition/cache identity; EEPROM storage geometry stays unchanged.
#define EH_PROCESS_RECORD_USER_FIRST
