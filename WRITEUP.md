# Hacking My Jarvis Desk with an Arduino

I wanted to control my Fully Jarvis standing desk from a computer. No WiFi modules,
no cloud services, no Bluetooth — just an Arduino Uno, an RJ-12 cable, and some
reverse-engineering.

This is what I found.

## The Setup

- **Desk:** Fully Jarvis with Jiecang controller
- **Controller port:** RJ-12 (6-pin, the Bluetooth add-on port)
- **Microcontroller:** Arduino Uno
- **Total cost:** ~£20 (Arduino + RJ-12 socket)
- **Wires needed:** 3 (GND, TX, RX)

### Wiring

```
RJ-12 Pin 2 (GND) → Arduino GND
RJ-12 Pin 3 (DTX) → Arduino Pin 2
RJ-12 Pin 5 (HTX) → Arduino Pin 3
Pin 4 (5V)        → Leave disconnected (USB power is fine)
```

No resistors. No level shifting. Both sides are 5V. Done.

## What Went Wrong First

Pretty much everything.

**Wiring mistake #1:** I had pins 4 and 5 swapped — feeding the desk's 5V power
supply into a data pin. Oops.

**Polarity mistake:** The original [phord/Jarvis](https://github.com/phord/Jarvis)
docs describe "5V pullups, mark=low" which sounds like inverted serial. It's not.
The RJ-12 port uses **standard UART polarity**. I figured this out by enabling
debug mode and moving the desk manually — the raw bytes were bit-inverted `F2 F2`
packets, which only makes sense if the serial was reading them wrong.

**The fix:** Normal `SoftwareSerial`, no inverse logic. 9600 baud, 8N1. Standard stuff.

## What Works

Once the wiring and polarity were sorted, everything just... worked.

### Reading
- Current height (in mm or tenths-of-inches depending on display units)
- All 4 memory preset positions
- Physical limits of the desk (min/max travel)
- User-set height limits

### Moving
- Raise/lower (continuous, like holding the button)
- Move to any of the 4 presets
- **Go to a specific height** — `goto 800` moves to 80.0cm and the desk handles
  acceleration, deceleration, and stopping. This was the big discovery.
- Stop movement

### The `goto` Command

This is the good stuff. Command `0x1B` with a 2-byte target height — the desk
does everything. No PID loops, no polling, no "am I there yet". Send it and forget it.

```
goto 800
> Native goto 800mm (80.0cm) from 744mm
[DESK] GOTO_HEIGHT (03 20)  target=80.0cm
[DESK] HEIGHT (02 E9 0F)  74.5cm
[DESK] HEIGHT (03 07 0F)  77.5cm
[DESK] HEIGHT (03 1C 0F)  79.6cm
> Target height reached: 800mm
[DESK] HEIGHT (03 1F 0F)  79.9cm
```

Not documented in phord's README. Found it in the
[ESPHome Jiecang component](https://github.com/Rocka84/esphome_components) source code,
confirmed it works on our hardware.

### The `scan` Command

I added a `scan` command that queries every known read command and dumps the results.
Change a setting on the handset, run scan, diff the output. This is how we mapped
most of the protocol.

## The Protocol

Packets look like this:
```
[addr] [addr] [cmd] [len] [params...] [checksum] [0x7E]
```

- Handset address: `0xF1 0xF1`
- Controller address: `0xF2 0xF2`
- Checksum: `(cmd + len + params) & 0xFF`
- EOM: always `0x7E`

### Commands That Work Over RJ-12

| Cmd | What it does | Response |
|-----|-------------|----------|
| 0x01 | Raise (one step) | HEIGHT stream |
| 0x02 | Lower (one step) | HEIGHT stream |
| 0x05/06 | Move to preset 1/2 | HEIGHT stream, auto-stops |
| 0x07 | Get all settings | POS_1-4 + HEIGHT |
| 0x08 | ??? | 0x05 with FF FF (or 00 06 sometimes) |
| 0x09 | ??? | 0x06 with 01 |
| 0x0C | Physical limits | 0x07 with max/min in mm |
| 0x1B | **Go to height** | Echo + HEIGHT stream, auto-stops |
| 0x1C | ??? | 0x1C with 0x35 (firmware version?) |
| 0x20 | Query limits | 0x20 bitmask + SET_MAX/SET_MIN if set |
| 0x23 | Clear limits | 0x20 confirmation |
| 0x27/28 | Move to preset 3/4 | HEIGHT stream, auto-stops |
| 0x29 | Wake | Wakes display, no data response |
| 0x2B | Stop | Stops movement, no data response |

### What Doesn't Work Over RJ-12

- **Set units** (0x0E) — handset only
- **Anti-collision** (0x1D) — not readable, probably RJ-45 only
- **BT app commands** (0xA0-0xAD) — no response, BT module only
- **Lock/brightness** — handset-local, no protocol traffic at all

### The HEIGHT Response

```
F2 F2 01 03 HH HL P2 checksum 7E
```

`{HH, HL}` = height in mm (CM mode) or tenths-of-inches (inch mode).
`P2` = always `0x0F` on our controller. phord saw `0x07`. Nobody knows what it is.
It doesn't change with units, calibration, movement, or any setting we tested.

## Things We Learned the Hard Way

### Don't Set Min = Max Height

We set both min and max limits to the same height (84.5cm). The desk got stuck.
Completely stuck. Couldn't move from the handset or the Arduino. Clearing the limits
via protocol didn't help — had to **unplug the desk from the wall** and wait.

Power cycle fixed it. But yeah, don't do that.

### Calibration Changes Everything (Uniformly)

When you recalibrate the height on the handset, every height value shifts by the
same offset. Physical limits, current height, all of it. But preset raw encoder
values stay the same — they're hardware positions, not display values.

No software offset needed — just calibrate via the handset and the protocol
values automatically match reality.

### Anti-Collision Sensitivity: Writable Over RJ-12

We physically tested this. Set sensitivity to HIGH via `0x1D 0x01`, raised the desk,
and pushed down with hand pressure — stopped easily at ~91cm. Set to LOW via
`0x1D 0x03`, same test — needed full body weight to trigger it, and it still got
to ~87cm before stopping.

**Setter commands work over RJ-12**, even though we can't read the values back.
The controller accepts and applies them immediately.

| Sensitivity | Command | Force to stop |
|-------------|---------|---------------|
| HIGH | `0x1D 0x01` | Moderate hand pressure |
| LOW | `0x1D 0x03` | Full body weight |

### Presets Are Raw Encoder Values

The POS_1-4 responses are internal encoder tick counts, not display millimetres.
There's a calibration formula somewhere (`height = offset + raw * 0.0642` per
phord/Jarvis#21) but the exact scaling varies by controller.

### The Desk Shows the Fully Logo When You Scan

The SETTINGS command (0x07) is what the handset sends on boot. The controller
wakes up and shows the brand logo. We're basically impersonating a handset.

### Handset-Local Settings

These don't go through the controller at all — no protocol traffic:
- Lock/unlock
- Screen brightness
- Button brightness

We confirmed this by running scan before and after toggling each one. Zero changes.

## RJ-12 vs RJ-45 Capability

| Feature | RJ-12 Read | RJ-12 Write | RJ-45 (per phord) |
|---------|-----------|-------------|-------------------|
| Height | ✅ | — | ✅ |
| Presets | ✅ raw values | ✅ move + save | ✅ |
| Physical limits | ✅ | — | ✅ |
| User limits | ✅ | ✅ set + clear | ✅ |
| Anti-collision | ❌ | ✅ confirmed | ✅ read + write |
| Units | ❌ | ❌ | ✅ |
| Memory mode | ❌ | Likely works | ✅ |
| Goto height | — | ✅ (0x1B) | ✅ |
| Stop | — | ✅ (0x2B) | ✅ |

## What's Still Unknown

| Response | Our data | Best guess |
|----------|----------|------------|
| 0x05 (from 0x08) | FF FF or 00 06 | Error log? Resets on power cycle |
| 0x06 (from 0x09) | 01 | Protocol version? |
| 0x1C (from 0x1C) | 0x35 | Firmware version? |
| HEIGHT P2 byte | always 0x0F | Doesn't change with units, calibration, or any setting |

## Standing on the Shoulders of Giants

None of this would have been possible without:

- [phord/Jarvis](https://github.com/phord/Jarvis) — the original reverse-engineering
  that started it all
- [Rocka84/esphome_components](https://github.com/Rocka84/esphome_components) —
  the Jiecang ESPHome component that confirmed goto-height works
- [auchter/esphome-jcb35n2](https://github.com/auchter/esphome-jcb35n2) —
  the alternative protocol for simpler controllers
- [Home Assistant community](https://community.home-assistant.io/t/desky-standing-desk-esphome-works-with-desky-uplift-jiecang-assmann-others/383790) —
  the Desky thread with years of collective knowledge
- [pimp-my-desk](https://gitlab.com/pimp-my-desk/desk-control/jiecang-reverse-engineering) —
  more Jiecang protocol details
- Everyone in [Jarvis#33](https://github.com/phord/Jarvis/issues/33) who
  decompiled the BT app and shared their findings

## What's Next

- Proper CLI commands for collision sensitivity, memory mode, units
- ESP32 port for WiFi control
- Home Assistant / MQTT integration
- Sit/stand timer with reminders

The code is a PlatformIO project with 47 unit tests. The research journal with
raw packet captures is in `RESEARCH.md`.
