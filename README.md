# Jarvis Desk Controller — Arduino Uno

Control a Jarvis/Jiecang standing desk from an Arduino Uno via the **RJ-12 port** using the reverse-engineered serial protocol from [phord/Jarvis](https://github.com/phord/Jarvis).

## Hardware Required

- Arduino Uno
- RJ-12 breakout / socket
- 2× 1kΩ resistors (signal protection)
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

## Serial Signal Notes

The desk uses **5V logic with pull-up resistors** — a "mark" (idle/1) is HIGH, and data is sent by pulling lines LOW. This is **inverted** compared to standard TTL UART.

The code uses `SoftwareSerial` with `inverse_logic = true` to handle this. If you find you're receiving garbage data, this is the first thing to verify — try toggling the inverse logic flag.

**If inverse logic doesn't resolve it**, the signals may actually be standard polarity on your controller model. The protocol was reverse-engineered from specific models, and yours may differ.

## Building & Uploading

### Arduino IDE

1. Open `jarvis-desk-controller.ino` in the Arduino IDE
2. Select **Board:** Arduino Uno
3. Select your serial port
4. Click Upload

### arduino-cli

```bash
arduino-cli compile -b arduino:avr:uno .
arduino-cli upload -b arduino:avr:uno -p /dev/ttyACM0 .
```

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

- **UART:** 9600 baud, 8N1, inverted logic
- **Packets:** `[addr][addr][cmd][len][params...][checksum][0x7E]`
- **Handset address:** `0xF1 0xF1`
- **Controller address:** `0xF2 0xF2`
- **Checksum:** Low byte of `(cmd + len + params...)`, i.e. `& 0xFF`

Full protocol documentation: https://github.com/phord/Jarvis

## RJ-12 vs RJ-45

This project uses the **RJ-12** interface (6-pin, Bluetooth add-on port). The larger **RJ-45** port (8-pin, handset port) has additional button mux lines (HS0-HS3) not available on RJ-12.

On RJ-12, some responses differ:
- `SETTINGS` returns fewer response packets
- `LIMIT_STOP` and `REP_PRESET` are not sent

## Troubleshooting

| Problem | Try |
|---------|-----|
| No response from desk | Check GND connection. Try `wake` command repeatedly. |
| Garbage data | Toggle `inverse_logic` in the SoftwareSerial constructor (true ↔ false). |
| Partial/corrupt packets | Check series resistors are connected. Reduce Serial.print debug output. |
| Desk doesn't move | The raise/lower commands send a single step. You may need to send repeatedly. |

## License

This project is for personal/educational use. The protocol was reverse-engineered by [phord](https://github.com/phord/Jarvis).
