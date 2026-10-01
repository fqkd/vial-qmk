#pragma once

#define HAL_USE_SPI TRUE
#define HAL_USE_PWM TRUE
#ifdef CODEX_HYBRID_ENABLE
#    define PAL_USE_CALLBACKS TRUE
#endif

#include_next <halconf.h>
