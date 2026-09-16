# Macropad rev3: experimental native Codex USB firmware

Status: implementation prototype. Windows desktop recognition and physical key,
RGB and LCD behavior are **not hardware-validated**. This is an independent
Ergohaven experiment, not an official OpenAI or Work Louder firmware.

Build: `qmk compile -kb ergohaven/macropad/rev3 -km codex`.
Return to the existing firmware: build the same keyboard with `-km v3`.
The GitHub Build Ergohaven workflow builds both targets and tests the portable
protocol code under AddressSanitizer and UndefinedBehaviorSanitizer.

## Runtime

No companion service, custom Windows driver, API key or MCP server is used.
The application must recognize the device and speak the Micro vendor protocol.
This build advertises the observed Micro discovery identity (`303A:8360`,
`Work Louder`, `Codex Micro`) and a vendor HID report 6 with 63 data bytes.
This is an experimental compatibility identity, **not an assigned Ergohaven
product identity or evidence of official support**. Product distribution needs
a separate identity/support decision. USB interface topology still differs
from a genuine Micro; Windows discovery must be tested before claiming parity.

Vial infrastructure is retained internally for compatibility with common
Ergohaven code. Its USB configuration protocol is intentionally unavailable in
this target. Original v3 behavior and its custom splash artwork are preserved
in the separate v3 target. Physical events ignore saved Vial key mappings.

The RP2040 flash layout is intentionally left at the existing 2 MB build limit,
even though this board reportedly carries 4 MB. Do not move EEPROM storage
without a migration plan. All incoming RGB state is held only in RAM.

## Controls

Rows below refer to the 12 switches, from top to bottom and left to right.

| Row | Left | Middle | Right |
| --- | --- | --- | --- |
| 1 | Task 1 | Task 2 | Task 3 |
| 2 | Task 4 | Task 5 | Task 6 |
| 3 | ACT06 / Fast | ACT07 / Approve | ACT08 / Decline |
| 4 | ACT09 / Continue in new task | ACT10 / Push to talk | ACT12 / Send |

The desktop app owns action bindings. Firmware sends press AND release and
never substitutes an unconditional Enter for approval. Encoder uses `ENC_CW`,
`ENC_CC` (act 2) and `ENC` press/release; reported encoder identifiers vary
between community references and require testing against the target app.

LCD displays six numbered colors and elapsed time since the last valid host
message. It does not claim knowledge of task titles, percentages or reasoning
level. Silence is not classified as disconnected because traffic can be
event-driven. Suspend/deconfiguration clears cached state.

RGB supports solid/off and approximate breathing. Other documented effect IDs
currently use solid color; speed and cross-zone synchronization are not yet
implemented. Brightness is capped to reduce current. There is no chassis RGB
zone, joystick, Bluetooth or battery; compatibility status reports a fixed
battery value and protocol version, not physical telemetry or vendor firmware.

## USB and parser

Interrupt OUT and 64-byte Output SET_REPORT on endpoint zero are supported.
Feature reports are not advertised. Incoming complete JSON objects are accepted
with or without CRLF, matching the Windows SDK's unterminated requests. Outgoing
messages use CRLF termination. Both directions use 61-byte chunks.
A bounded 2048-byte receive buffer, bounded token count and depth,
strict JSON validation, timeout for incomplete input and atomic slot updates
prevent malformed traffic from changing partial lighting state. Unknown
methods return an error. Host-requested bootloader entry, filesystem writes
and firmware updates are not implemented.

Protocol facts consulted (implementation written independently):

- https://github.com/arthurcolle/codex-micro-open
- https://github.com/eliBenven/freemicro/blob/main/docs/PROTOCOL.md

These are community observations with some disagreements, not a specification
or a Windows compatibility guarantee. No source from noncommercial firmware
projects is included.

## First physical test and recovery

1. Export current Vial settings and keep the current working UF2. Confirm this
   is rev3; never use this file on rev1/rev2.
2. Enter RP2040 bootloader using the board BOOT/RESET procedure or existing
   QMK bootmagic (top-left physical key held while connecting). Bootmagic
   resets EEPROM, which is why the settings export matters.
3. Copy the experimental UF2 to RPI-RP2. Open the Windows desktop app and
   check whether Micro settings appear and LCD changes from Waiting for host.
4. Verify all six task keys, RGB, dial and voice press/release on a disposable
   task. Test approve/decline only with a request you have inspected on screen.
5. Test reconnect, app restart and Windows sleep/resume. Record app version
   and which controls work. A successful build is not a substitute for this.
6. To recover, enter BOOTSEL again, copy the preserved stock UF2, then restore
   Vial settings. Do not accept a vendor firmware update for this prototype.

Portable test command:

```sh
gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  keyboards/ergohaven/macropad/codex/protocol.c \
  keyboards/ergohaven/macropad/codex/test_protocol.c -lm -o /tmp/codex-test
/tmp/codex-test
```
