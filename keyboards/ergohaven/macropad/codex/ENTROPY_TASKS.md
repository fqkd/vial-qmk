# Entropy-owned local task list

Optional Windows companion:
[fqkd/entropy, codex/macropad-task-titles](https://github.com/fqkd/entropy/tree/codex/macropad-task-titles).
See its `CODEX_MACROPAD_RU.md` for setup and state-source limitations.

The hybrid keymap can temporarily use six named tasks supplied by Entropy.
The companion owns both status/title data and opening by local task UUID.
Native Micro slots are never paired with independently guessed titles.
While the companion lease is active, Micro action keys are suppressed.
The layer key and normal QMK assignments continue to work. On lease expiry
the firmware clears all companion titles/events and resumes native Micro.

## MPT1 wire contract

Vial raw HID interface, 32 bytes per request/reply. No extra USB interface.
Header bytes: `D7 01 operation request_sequence generation_le32`.
Replies echo all eight header bytes; byte 8 is status:
0 success, 1 invalid, 2 busy, 3 expired/wrong active generation.

| Operation | Request payload (starting byte 8) | Reply data (starting byte 9) |
|---|---|---|
| 0 capabilities | none | ASCII `MPT1` |
| 1 begin snapshot | none | none |
| 2 row metadata | slot, state, UUID 16 bytes, title byte length, reserved zero | none |
| 3 title chunk | slot, byte offset, byte count (0–21), UTF-8 bytes | none |
| 4 commit | none | none |
| 5 poll/heartbeat | none | event sequence LE32, event generation LE32, slot (255 if none) |
| 6 acknowledge | event sequence LE32 | none |
| 7 release lease | none | none |

States: 0 empty, 1 unknown/read, 2 working, 3 complete/unopened, 4 waiting (reserved;
current companion cannot detect this), 5 aborted/error. Empty rows have
zero UUID and length. Occupied rows require unique nonzero UUID and 1–96
bytes of valid UTF-8 without ASCII control characters. All six rows and all
title bytes must arrive before commit. Repeated metadata clears its chunks.

An unacknowledged button event blocks every commit. Replacing UUIDs also
waits three seconds after a local interaction. Poll keeps the six-second
lease alive. Poll/ACK/release must name the committed generation. Events
are queued on press only; release doesn't open twice. The bounded queue
holds eight events (additional presses are ignored until drained).
Requests and replies share Entropy's existing serialized HID worker.

Completion stays filled green on screen and on its assigned task key on layer 0
after the five notification pulses. Entropy clears it to state 1 on opening that
result. Encoder selection alone is not an acknowledgement. Per-result receipts
are persisted by Entropy; the wire protocol and EEPROM layout are unchanged.
Screen colors: working #38A8FF, complete #36ED95, waiting #FFBE32, aborted #FF5270.
Text contrast is chosen automatically; RGB still respects the user's brightness.

The hybrid rev3 keymap captures GP24/GP25 edges with ChibiOS PAL interrupts.
A full quadrature cycle produces one detent, with bounce/invalid transitions
filtered and 31 completed events buffered during SPI screen flushes. QMK key
processing stays on the main loop. Unchanged task labels/styles are cached.
`test_encoder_capture.c` covers direction, bounce, partial turns, invalid edges,
buffered bursts and queue overflow/recovery.

Pure C tests cover malformed/partial snapshots, UTF-8, repeated metadata,
duplicate/empty IDs, atomic commit, interaction deferral, generation-bound
events, acknowledgement, notification timing, timer wrap and lease expiry.
