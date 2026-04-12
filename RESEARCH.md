# Jarvis Desk Protocol — Research Journal

> Reverse-engineering notes for a **Fully Jarvis** standing desk with a **Jiecang** controller,
> communicating via the **RJ-12** (Bluetooth add-on) port using an **Arduino Uno**.
>
> Protocol reference: [phord/Jarvis](https://github.com/phord/Jarvis)
> ESPHome reference: [Rocka84/esphome_components](https://github.com/Rocka84/esphome_components/tree/master/components/jiecang_desk_controller)

---

## Hardware Setup

| Item | Details |
|------|---------|
| Desk | Fully Jarvis standing desk |
| Controller | Jiecang (model TBD — RJ-12 port present) |
| Handset | Touchpanel with 4 memory presets |
| Microcontroller | Arduino Uno (ATmega328P, 5V logic) |
| Connection | RJ-12 6-pin jack (Bluetooth add-on port) |
| UART | 9600 baud, 8N1, **normal polarity** (NOT inverted) |

### Wiring (RJ-12 → Arduino Uno)

```
RJ-12 Pin 2 (GND) → Arduino GND
RJ-12 Pin 3 (DTX) → Arduino Digital Pin 2 (RX)
RJ-12 Pin 4 (VCC) → Disconnected (USB powered for dev)
RJ-12 Pin 5 (HTX) → Arduino Digital Pin 3 (TX)
```

No resistors, no level shifting — both sides are 5V logic.

---

## Journey Log

### 2026-04-11: Project Scaffolding

- Created PlatformIO project targeting Arduino Uno
- Implemented the phord/Jarvis UART protocol: packet builder, checksum, parser state machine
- Serial monitor CLI with commands: up, down, stop, presets, settings, height, etc.
- Added features: continuous raise/lower with 30s safety timeout, move-to-height,
  raw hex debug mode, auto-detect serial polarity
- 47 native unit tests for protocol and parser — all passing

### 2026-04-11: First Connection Attempt (FAILED)

**Problem:** Auto-detect found no desk on either polarity.

**Root cause 1 — Wiring error:** RJ-12 pins 4 (VCC) and 5 (HTX) were swapped.
Pin 4 is 5V power, pin 5 is the data line. We had the desk's 5V power going into
a data pin and the data line tied to constant 5V.

**Fix:** Disconnected pin 4 (VCC) entirely, moved pin 5 (HTX) to Arduino D3.

**Root cause 2 — Still no response after wiring fix.**
Enabled debug mode and moved desk manually with the RJ-45 handset controller.
Saw data:
```
RX: 83 83 F9 F9 E3 E1 B7 03 00
```

These are **bit-inverted** `F2 F2` packets! The code was trying inverted serial
first (based on the phord docs saying "5V pullups, mark=low"), but this controller
uses **normal/standard UART polarity**.

**Fix:** Changed default to normal polarity. Result:
```
[DESK] HEIGHT (03 12 0F)  height=786
```

**Lesson learned:** The phord/Jarvis docs describe an older controller model.
The Jiecang RJ-12 interface uses standard UART, confirmed by the
ESPHome jiecang_desk_controller component.

### 2026-04-12: Live Testing & Protocol Discovery

Connected via serial monitor. Systematically tested all features and probed
unknown commands.

#### Working Features
- **settings** → Returns POS_1–4 (raw encoder values) + current HEIGHT
- **height** → Works via SETTINGS command (0x07)
- **limits** → Returns 0x00 (no user-set limits)
- **raise/lower** → Continuous movement works, stop works
- **preset 1–4** → Desk moves to stored position, auto-stops
- **debug mode** → Raw hex byte output for troubleshooting

#### Major Discovery: GOTO_HEIGHT (0x1B)

The desk supports a **native goto-height command** — not documented in the
phord/Jarvis README but used by the ESPHome component:

```
Handset sends:  F1 F1 1B 02 HI LO checksum 7E
Desk responds:  F2 F2 1B 02 HI LO checksum 7E  (echo)
                F2 F2 01 03 HH HL 0F checksum 7E  (HEIGHT stream until settled)
```

Where `{HI, LO}` = target height in the same units as HEIGHT reports (mm on this desk).

**Test:** Sent `raw 1B 03 06` (target = 774 = 77.4cm). Desk moved from 80.1cm
down to 77.4cm and auto-stopped. The desk handles acceleration, deceleration,
and final positioning — no need for the crude continuous raise/lower approach!

#### STOP Command (0x2B)

Also discovered (via ESPHome reference) that **0x2B** is a proper STOP command.
The phord docs only document 0x29 (WAKE), which on our controller only wakes
the display and doesn't stop movement or trigger data responses.

---

## Protocol Probe Results

### Commands Tested (Handset → Desk, 0xF1 address)

| Cmd | Name | Params | Response | Our Data | Notes |
|-----|------|--------|----------|----------|-------|
| 0x01 | RAISE | 0 | HEIGHT stream | ✅ | Send repeatedly for continuous raise |
| 0x02 | LOWER | 0 | HEIGHT stream | ✅ | Send repeatedly for continuous lower |
| 0x05 | MOVE_1 | 0 | HEIGHT stream | ✅ | Desk moves to preset 1, auto-stops |
| 0x06 | MOVE_2 | 0 | HEIGHT stream | ✅ | Desk moves to preset 2, auto-stops |
| 0x07 | SETTINGS | 0 | POS_1-4 + HEIGHT | ✅ | Returns all 4 presets + current height |
| 0x08 | UNKNOWN_08 | 0 | 0x05 (FF FF) | ✅ | Docs saw 00 00 or FF FF. Unknown purpose |
| 0x09 | UNKNOWN_09 | 0 | 0x06 (01) | ✅ | Consistent with docs. Unknown purpose |
| 0x0C | PHYS_LIMITS | 0 | 0x07 | 04 E5 02 FD | **Confirmed:** {P0,P1}=1253 max, {P2,P3}=765 min (mm) |
| 0x0E | UNITS | 1 | — | no response | Setter — needs param. Sent without param |
| 0x19 | MEM_MODE | 1 | — | no response | Setter — needs param. Sent without param |
| 0x1B | **GOTO_HEIGHT** | 2 (HI,LO) | 0x1B echo + HEIGHT | **03 06** | 🆕 **Native goto!** Desk moves to target, auto-stops |
| 0x1C | UNKNOWN_1C | 0 | 0x1C | 35 | Docs saw 0x0A. We got 0x35 (53). Firmware version? |
| 0x1D | COLL_SENS | 1 | — | no response | Setter — needs param. Sent without param |
| 0x1F | UNKNOWN_1F | 0 | 0x1F | 00 | Matches docs. Unknown purpose |
| 0x20 | LIMITS | 0 | 0x20 | 00 | No user-set min/max limits configured |
| 0x27 | MOVE_3 | 0 | HEIGHT stream | ✅ | Desk moves to preset 3, auto-stops |
| 0x28 | MOVE_4 | 0 | HEIGHT stream | ✅ | Desk moves to preset 4, auto-stops |
| 0x29 | WAKE | 0 | none (display) | ✅ | Wakes desk display. **No data response on RJ-12** |
| 0x2B | **STOP** | 0 | none | ✅ | 🆕 Stops movement. From ESPHome reference |

### Desk Responses Observed (Controller → Handset, 0xF2 address)

| Resp | Name | Params | Meaning |
|------|------|--------|---------|
| 0x01 | HEIGHT | 3: HH HL 0F | Current height. {P0,P1} = mm. P2 always 0x0F on this controller |
| 0x05 | UNK_05 | 2 | Response to 0x08. Seen: FF FF |
| 0x06 | UNK_06 | 1 | Response to 0x09. Seen: 01 |
| 0x07 | PHYS_LIMITS | 4 | Response to 0x0C. {P0,P1}=max mm, {P2,P3}=min mm |
| 0x1B | GOTO_ACK | 2 | Echoes target height from 0x1B command |
| 0x1C | UNK_1C | 1 | Response to 0x1C. Seen: 0x35 |
| 0x1F | UNK_1F | 1 | Response to 0x1F. Seen: 0x00 |
| 0x20 | LIMITS | 1 | Bitmask: 0x00=none, 0x01=max, 0x10=min, 0x11=both |
| 0x25 | POS_1 | 2 | Preset 1 stored value (raw encoder, NOT display mm) |
| 0x26 | POS_2 | 2 | Preset 2 stored value |
| 0x27 | POS_3 | 2 | Preset 3 stored value |
| 0x28 | POS_4 | 2 | Preset 4 stored value |

---

## Observations & Open Questions

### HEIGHT Third Byte (P2)
The HEIGHT response always has `0x0F` as the third parameter on our controller.
The phord docs note seeing `0x07` and `0x0F`. This might indicate:
- Units (0x07 = inches mode, 0x0F = cm mode)?
- Controller model/firmware identifier?
- Something else entirely?

**Experiment idea:** Change units to inches and see if P2 changes.

### Preset Values vs Display Height
Preset positions are stored as raw encoder values, not display millimeters:
```
POS_1 raw=5328   → desk goes to display 77.4cm
POS_2 raw=5616   → desk goes to display ~78.8cm
POS_3 raw=12187  → desk goes to standing height
POS_4 raw=14065  → desk goes to max standing height
```

The relationship between raw encoder values and display mm is non-linear or uses
an offset calibration. The desk controller handles this mapping internally.

### Display Height vs Physical Height
There's a consistent **~2.4cm offset**:
- Desk display shows: 77.4cm
- Tape measure shows: ~75.0cm

This is likely because the desk measures from a mechanical reference point
(e.g., top of the motor column) rather than from the floor. The offset may vary
by desk frame configuration.

### UNKNOWN_1C Response Discrepancy
phord docs saw `0x0A` (10), we got `0x35` (53). This could be:
- Firmware version number
- Feature capability flags
- Controller model identifier
- Anti-collision sensitivity range/setting

### Commands Not Yet Tested
These require parameters and could change desk configuration — test with caution:
- `0x0E` with param 0x00/0x01 — Set units CM/IN
- `0x19` with param 0x00/0x01 — Set memory mode (one-touch/constant-touch)
- `0x1D` with param 0x01/0x02/0x03 — Set anti-collision sensitivity (H/M/L)
- `0x03/0x04/0x25/0x26` — Save presets (PROGMEM) — **will overwrite stored positions!**
- `0x21/0x22` — Set max/min height limits
- `0x23` — Clear min/max limits
- `0x91` — Calibrate — **DANGER: puts desk in RESET mode**

### Commands Not Available on RJ-12
Per the phord docs, these are RJ-45 only:
- `LIMIT_STOP (0x23 response)` — not sent on RJ-12
- `REP_PRESET (0x92 response)` — not sent on RJ-12

---

## Research Findings (2026-04-12)

Compiled from GitHub repos, ESPHome components, Home Assistant community,
Jarvis issues/discussions, and a Jiecang BT app decompile.

### New Commands Found (from BT app decompile — Jarvis issue #33)

| Cmd | Name | Params | Notes |
|-----|------|--------|-------|
| 0xA0 | PATCH | ? | From Jiecang BT app. Purpose unknown |
| 0xA2 | FETCH_STAND_TIME | ? | Query standing time statistics |
| 0xA6 | FETCH_STAND_TIME (alt) | ? | Alternative stand time query |
| 0xAA | FETCH_ALL_TIME | ? | Query all usage time statistics |
| 0xAD | SET_MEMORY_HEIGHT | ? | Variant-dependent — alternative preset save? |

**Source:** Decompiled Jiecang Bluetooth app `Command.kt`, referenced in
[phord/Jarvis#33](https://github.com/phord/Jarvis/issues/33).

### Bluetooth Protocol Details

The Jiecang BLE module uses the same UART frame format over Bluetooth:
- **BLE Service:** `0xFE60`
- **BLE Characteristics:** `0xFE61` (write) / `0xFE62` (notify)
- **Module:** Lierda LSD4BT-E95ASTD001
- Commands are the same `F1 F1 cmd len params checksum 7E` packets,
  often sent twice for reliability over BLE
- The RJ-12 port is literally the BT module's UART connection —
  we're speaking the same protocol the Bluetooth add-on would

**Source:** [pkg/jiecang/jiecang.go](https://github.com/phord/Jarvis/issues/33)
in a Go implementation referenced in the discussion.

### Preset Calibration Formula

From [phord/Jarvis#21](https://github.com/phord/Jarvis/issues/21):
```
height_mm = offset + raw_encoder_value * 0.0642
```
This explains why our preset values (5328, 5616, 12187, 14065) don't directly
correspond to display mm. The controller applies an offset and a scaling factor
to convert raw encoder ticks to display height.

**Verification with our data:**
```
POS_1 raw=5328, display=77.4cm (774mm)
POS_2 raw=5616, display=~78.8cm (788mm)

Difference: 5616-5328 = 288 raw ticks → 788-774 = 14mm
Scale: 14/288 = 0.0486 mm/tick (close to 0.0642 but not exact)
```
The formula may vary by controller model or our offset may differ.

### HEIGHT Response P2 Byte

Still unresolved across all sources:
- Our controller: always `0x0F`
- phord's controller: saw `0x07` and `0x0F`
- No source has decoded this byte

**Theories:**
- Bit flags for units/mode (0x07 = 0b0111, 0x0F = 0b1111)?
- Controller feature/capability mask?
- Movement state indicator?

**Experiment to try:** Change units to inches (`raw 0E 01`) and check if P2 changes.

### Still Unknown Commands

| Cmd/Resp | Our Data | Other Data | Best Guess |
|----------|----------|------------|------------|
| 0x08→0x05 | FF FF | 00 00 or FF FF | Possibly "stored error code" or "last event" — FF FF = none |
| 0x09→0x06 | 01 | 01 | Possibly "controller status" or "protocol version" |
| 0x1C→0x1C | 0x35 (53) | 0x0A (10) | Firmware version or feature capability flags |
| 0x1F→0x1F | 0x00 | 0x00 | Always 0x00 — possibly "child lock" or reserved |

### Alternative Protocol: JCB35N2 (Simple 2-Button Handset)

The JCB35N2 controller (used with basic up/down handsets) uses a completely
different 4-byte protocol:
```
0x01 0x01 0x01 <height_byte>    (during movement)
0x01 0x05 0x01 0xAA             (movement ended)
```
Height byte: each unit ≈ 0.1 inches.
Calibration: `raw_to_inches = (val - min_raw) * 0.1 + min_inches`

**Source:** [auchter/esphome-jcb35n2](https://github.com/auchter/esphome-jcb35n2)

Some controllers support BOTH protocols and may switch between them.

### RJ-12 5th Wire (Pin 1 or 6)

From Jarvis #33: the BT adapter's 5th wire is labeled `BKG` and measured
at 3.3V. Speculation it stands for "background" or "backlight" — possibly
a signal to control the handset display backlight.

---

## Recommendations: Where to Go Next

### Safe Experiments (read-only or reversible)
1. **Decode HEIGHT P2 byte** — Change units to inches (`raw 0E 01`), get
   height report, see if P2 changes. Change back with `raw 0E 00`.
2. **Probe 0x08 after movement** — Send `raw 08` right after the desk moves
   to see if the FF FF changes (error/event log theory).
3. **Try BT app commands** — Send `raw A0`, `raw A2`, `raw AA` to see if
   the controller responds (stand time statistics would be cool!).
4. **Scan for undocumented commands** — Systematically send `raw 0A`,
   `raw 0B`, `raw 0D`, `raw 10`–`raw 18`, `raw 1A`, `raw 1E`, `raw 24`,
   `raw 2A`, `raw 30`–`raw 40` and log any responses.

### Useful Features to Implement
5. **Native goto-height in CLI** — ✅ Done! Test with `goto 802`.
6. **Height offset calibration** — Add a user-configurable offset (-24mm)
   so displayed heights match real tape-measure heights.
7. **Stand time tracking** — If 0xA2/0xAA commands work, display sit/stand
   statistics.
8. **Preset management via protocol** — Save/recall presets using real
   display heights, with the encoder calibration formula.

### Ambitious Goals
9. **WiFi/BLE bridge** — Port to ESP32, expose as BLE device or MQTT client
   for Home Assistant integration.
10. **Sit/stand reminder** — Track time at height and send alerts.
11. **Contribute findings upstream** — Open a discussion on phord/Jarvis
    with our GOTO_HEIGHT, STOP, and physical limits discoveries.

---

## Tools & Resources

- **Protocol docs:** https://github.com/phord/Jarvis
- **ESPHome Jiecang component:** https://github.com/Rocka84/esphome_components/tree/master/components/jiecang_desk_controller
- **ESPHome JCB35N2 component:** https://github.com/auchter/esphome-jcb35n2
- **Loctek/Flexispot controller:** https://github.com/iMicknl/LoctekMotion_IoT
- **Jiecang reverse engineering:** https://gitlab.com/pimp-my-desk/desk-control/jiecang-reverse-engineering
- **Community discussion (HA):** https://community.home-assistant.io/t/desky-standing-desk-esphome-works-with-desky-uplift-jiecang-assmann-others/383790
- **BT app decompile & extras:** https://github.com/phord/Jarvis/issues/33
- **Preset calibration formula:** https://github.com/phord/Jarvis/issues/21
- **JCB35N2 protocol discussion:** https://github.com/phord/Jarvis/discussions/4
- **RJ-12/RJ-45 protocol split:** https://github.com/ssieb/esphome_components/issues/40
