# Jarvis Desk Controller

Control a Jarvis/Jiecang standing desk via the **RJ-12 port** using the reverse-engineered serial protocol from [phord/Jarvis](https://github.com/phord/Jarvis).

Two platforms are supported:

| Platform | Interface | Features |
|----------|-----------|----------|
| **Arduino Uno** | USB Serial CLI | Direct serial commands, interactive terminal |
| **ESP32-C6** | WiFi + MQTT | Home Assistant auto-discovery, OTA updates |

## Hardware Required

### Arduino Uno
- Arduino Uno
- RJ-12 breakout / socket
- 2× 1kΩ resistors (signal protection)
- Hookup wire

### ESP32-C6
- DFRobot Beetle ESP32-C6 (or any ESP32-C6 board)
- RJ-12 breakout / socket
- 1× 1kΩ + 1× 2kΩ resistor (voltage divider for RX line)
- Hookup wire

## Wiring

The RJ-12 port is the 6-pin telephone-style jack on the desk controller (used for the optional Bluetooth add-on). Only the center 4 pins are needed.

```
 RJ-12 Jack (front view, tab down)
 ┌──────────┐
 │ 1 2 3 4 5 6 │
 └──┬─┬─┬─┬─┬─┬──┘
    │ │ │ │ │ │
    NC│ │ │ │ NC
      │ │ │ │
      │ │ │ └── Pin 5: HTX (to desk) ──[1kΩ]── Arduino Pin 3
      │ │ └──── Pin 4: VCC (5V out)    (see power notes below)
      │ └────── Pin 3: DTX (from desk) ──[1kΩ]── Arduino Pin 2
      └──────── Pin 2: GND ──────────── Arduino GND
```

### Connections Summary

| RJ-12 Pin | Label | Arduino Uno    | Notes                       |
|-----------|-------|----------------|-----------------------------|
| 1         | NC    | —              | Not connected               |
| 2         | GND   | GND            | Common ground (REQUIRED)    |
| 3         | DTX   | Pin 2 (RX)     | Via 1kΩ series resistor     |
| 4         | VCC   | —              | 5V from desk (see below)    |
| 5         | HTX   | Pin 3 (TX)     | Via 1kΩ series resistor     |
| 6         | NC    | —              | Not connected               |

### Why series resistors?

The 1kΩ resistors protect both the Arduino and the desk controller from shorts, voltage spikes during hot-plugging, and current surges. They don't affect signal integrity at 9600 baud.

### Power Options

**For development (recommended):** Power the Arduino via USB. Leave RJ-12 Pin 4 (VCC) disconnected.

**For standalone deployment:** You *can* power the Arduino from the desk's 5V supply on RJ-12 Pin 4 → Arduino 5V pin. However:

> ⚠️ **Never connect both USB and desk 5V simultaneously** — this back-powers the USB host and can damage your computer or the desk controller. Use a diode or power switch if you need both.

### ESP32-C6 Wiring

```
RJ-12 Pin 2 (GND) → ESP32 GND
RJ-12 Pin 3 (DTX) → 1kΩ + 2kΩ voltage divider → ESP32 GPIO17 (RX)
RJ-12 Pin 4 (VCC) → Not connected (USB powered)
RJ-12 Pin 5 (HTX) → ESP32 GPIO18 (TX) — 3.3V output is OK for desk 5V input
```

> ⚠️ **Voltage Warning:** The ESP32-C6 is **3.3V logic, NOT 5V tolerant**. The desk outputs 5V signals.
>
> - **RX line (desk→ESP32):** MUST use a voltage divider (1kΩ + 2kΩ gives ~3.3V) or a level shifter
> - **TX line (ESP32→desk):** 3.3V output is accepted by the desk's 5V logic (above V<sub>IH</sub> threshold)
> - **Never connect desk 5V directly to ESP32 GPIO — it will damage the chip**

## Serial Signal Notes

Live testing on a Jiecang JCB36N2CA (Fully Jarvis) confirmed **normal UART polarity** — standard 9600/8N1 with no inversion needed. Earlier documentation suggested inverted logic, but this was not the case on the tested controller.

The code uses `SoftwareSerial` with `inverse_logic = false` (standard polarity). If you get garbage data, try toggling this flag — some controller models may differ.

## Building & Uploading (PlatformIO)

This project uses [PlatformIO](https://platformio.org/) for build/upload/serial monitoring.

### Setup

1. Install the **PlatformIO IDE** extension in VSCode (`platformio.platformio-ide`)
2. Open this folder in VSCode — PlatformIO detects `platformio.ini` automatically

### Build, Upload & Monitor

Use the PlatformIO toolbar at the bottom of VSCode, or:

```bash
# Arduino Uno
pio run -e uno
pio run -e uno --target upload

# ESP32-C6
pio run -e esp32c6
pio run -e esp32c6 --target upload

# Tests
pio test -e native

# Open serial monitor (115200 baud)
pio device monitor

# Build + upload + monitor in one shot
pio run -e uno --target upload && pio device monitor
```

### ESP32-C6 Setup

1. Copy `include/config.example.h` to `include/config.h`
2. Edit `config.h` with your WiFi SSID/password and MQTT broker IP
3. Build: `pio run -e esp32c6`
4. Upload: `pio run -e esp32c6 --target upload`
5. Monitor: `pio device monitor`

### Home Assistant Integration

The ESP32 firmware publishes MQTT auto-discovery messages. After connecting:

- A **"Jarvis Desk"** device appears automatically in Home Assistant
- **Entities:** height sensor (cm), target height (number), presets 1–4 (buttons), up/down/stop (buttons)
- **Requires** an MQTT broker (e.g., Mosquitto) configured in Home Assistant

### MQTT Topics

| Topic | Direction | Description |
|-------|-----------|-------------|
| `jarvis-desk/status` | ESP→broker | Online/offline status (retained LWT) |
| `jarvis-desk/height/state` | ESP→broker | Current height in mm (retained) |
| `jarvis-desk/height/set` | broker→ESP | Move to target height in mm |
| `jarvis-desk/preset/set` | broker→ESP | Move to preset (payload: `1`–`4`) |
| `jarvis-desk/command` | broker→ESP | Send command: `up`, `down`, `stop` |
| `jarvis-desk/collision/set` | broker→ESP | Set collision sensitivity: `high`, `medium`, `low` |

### OTA Updates

After the first upload via USB, subsequent updates can use OTA:

```bash
pio run -e esp32c6 --target upload --upload-port jarvis-desk.local
```

### Alternative: Arduino IDE

If you prefer the Arduino IDE, copy files from `src/` and `include/` into a single
folder named `jarvis-desk-controller/`, rename `main.cpp` → `jarvis-desk-controller.ino`,
and open it in the Arduino IDE.

## Usage

Open the Serial Monitor at **115200 baud**. Type commands and press Enter:

| Command       | Action                         |
|---------------|--------------------------------|
| `up` / `raise`| Raise desk one step            |
| `down`/`lower`| Lower desk one step            |
| `1` `2` `3` `4`| Move to memory preset        |
| `save1`..`save4`| Save current height to preset|
| `height`      | Request current height         |
| `settings`    | Request all desk settings      |
| `limits`      | Request min/max limits         |
| `wake`        | Send wake signal               |
| `cm` / `in`   | Set display units              |
| `status`      | Show connection state          |
| `help`        | List all commands              |

### What you'll see

```
=== Jarvis Desk Controller ===
Type 'help' for commands

Waking desk...
[DESK] HEIGHT (05 06 07)  height=1286
> Moving to preset 1
[DESK] HEIGHT (03 CA 07)  height=970
```

## Protocol Reference

- **UART:** 9600 baud, 8N1, normal polarity (not inverted)
- **Packets:** `[addr][addr][cmd][len][params...][checksum][0x7E]`
- **Handset address:** `0xF1 0xF1`
- **Controller address:** `0xF2 0xF2`
- **Checksum:** Low byte of `(cmd + len + params...)`, i.e. `& 0xFF`

Full protocol documentation: https://github.com/phord/Jarvis

## Protocol Discoveries (Live Testing)

Tested on a **Jiecang JCB36N2CA** (Fully Jarvis, RJ-12 port). The RJ-12 interface works standalone — no handset required.

### Confirmed Commands (Handset → Desk)

| Cmd  | Name       | Params     | Response                  | Notes                                              |
|------|------------|------------|---------------------------|----------------------------------------------------|
| 0x01 | RAISE      | 0          | HEIGHT stream             | Continuous raise when sent repeatedly              |
| 0x02 | LOWER      | 0          | HEIGHT stream             | Continuous lower when sent repeatedly              |
| 0x05 | MOVE_1     | 0          | HEIGHT stream             | Moves to preset 1, auto-stops                      |
| 0x06 | MOVE_2     | 0          | HEIGHT stream             | Moves to preset 2, auto-stops                      |
| 0x07 | SETTINGS   | 0          | POS_1–4 + HEIGHT          | Returns all 4 presets and current height            |
| 0x0C | PHYS_LIMITS| 0          | 0x07 (HI HI LO LO)       | Physical motion limits: max/min in mm              |
| 0x1B | GOTO_HEIGHT| 2 (HI, LO)| 0x1B echo + HEIGHT stream | **Native goto-height.** Desk handles accel/decel/stop |
| 0x20 | LIMITS     | 0          | 0x20 (00)                 | User-set limits bitmask. 0x00 = none set           |
| 0x27 | MOVE_3     | 0          | HEIGHT stream             | Moves to preset 3, auto-stops                      |
| 0x28 | MOVE_4     | 0          | HEIGHT stream             | Moves to preset 4, auto-stops                      |
| 0x29 | WAKE       | 0          | none (display only)       | Wakes display. No data response on RJ-12           |
| 0x2B | STOP       | 0          | none                      | Stops all movement immediately                     |

### Controller Responses

| Resp | Name       | Params | Meaning                                               |
|------|------------|--------|-------------------------------------------------------|
| 0x01 | HEIGHT     | 3      | {P0,P1} = height mm, P2 = flags (always 0x0F here)    |
| 0x07 | PHYS_LIMITS| 4      | {P0,P1} = max mm, {P2,P3} = min mm                    |
| 0x1B | GOTO_ACK   | 2      | Echoes target height                                   |
| 0x20 | LIMITS     | 1      | Bitmask of user-set limits                             |
| 0x25–0x28 | POS_1–4 | 2   | Raw encoder value (NOT display mm)                     |

### Key Observations

- **Normal UART polarity** — standard 9600/8N1, no inversion needed.
- **GOTO_HEIGHT (0x1B)** is the cleanest move method — desk handles acceleration, deceleration, and auto-stop.
- **STOP (0x2B)** halts movement immediately. ESPHome uses wake + stop as an init sequence.
- **Preset values** are raw encoder positions, not display millimeters.
- **Height offset:** Display height has a ~2.4 cm offset from physical (tape-measure) height.
- **Physical limits** on tested unit: 76.5–125.3 cm reported, ~74.1–122.9 cm actual.
- **No-response commands:** 0x0E (UNITS), 0x19 (MEM_MODE), 0x1D (COLL_SENS) — these are setters that likely need parameters, or are RJ-45 only.

## RJ-12 vs RJ-45

This project uses the **RJ-12** interface (6-pin, Bluetooth add-on port). The larger **RJ-45** port (8-pin, handset port) has additional button mux lines (HS0-HS3) not available on RJ-12.

On RJ-12, some responses differ:
- `SETTINGS` returns fewer response packets
- `LIMIT_STOP` and `REP_PRESET` are not sent

## Troubleshooting

| Problem | Try |
|---------|-----|
| No response from desk | Check GND connection. Try `wake` command repeatedly. |
| Garbage data | Toggle `inverse_logic` in the SoftwareSerial constructor. Normal polarity (false) confirmed on JCB36N2CA. |
| Partial/corrupt packets | Check series resistors are connected. Reduce Serial.print debug output. |
| Desk doesn't move | The raise/lower commands send a single step. You may need to send repeatedly. |

## License

This project is for personal/educational use. The protocol was reverse-engineered by [phord](https://github.com/phord/Jarvis).
