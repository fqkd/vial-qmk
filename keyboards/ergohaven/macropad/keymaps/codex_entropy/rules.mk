VIAL_ENABLE = yes
VIALRGB_ENABLE = yes
ENCODER_MAP_ENABLE = yes
MIDI_ENABLE = no
OPT_DEFS += -DCODEX_MICRO_ENABLE -DCODEX_HYBRID_ENABLE
SRC += keyboards/ergohaven/macropad/codex/protocol.c
SRC += keyboards/ergohaven/macropad/codex/runtime.c
