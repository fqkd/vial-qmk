# Dedicated experimental target; the ordinary v3 target is unchanged.
VIAL_ENABLE = yes
VIALRGB_ENABLE = no
ENCODER_MAP_ENABLE = no
MIDI_ENABLE = no
OPT_DEFS += -DCODEX_MICRO_ENABLE
SRC += keyboards/ergohaven/macropad/codex/protocol.c
SRC += keyboards/ergohaven/macropad/codex/runtime.c
