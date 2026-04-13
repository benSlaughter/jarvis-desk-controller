# Jarvis Desk Controller — Hardware

Custom PCB to connect a DFRobot Beetle ESP32-C6 to a Jarvis standing desk via
the RJ-12 control port. The board provides a voltage divider on the desk→MCU
data line (5 V → 3.3 V logic) and breaks out power + UART signals.

## Bill of Materials

| Ref | Part | Value | Package / Footprint | Qty | Example Part Number |
|-----|------|-------|---------------------|-----|---------------------|
| U1 | DFRobot Beetle ESP32-C6 | — | 2×7 pin header, 2.54 mm pitch | 1 | DFRobot DFR1117 |
| J1 | RJ-12 socket | 6P6C | Through-hole, right-angle | 1 | Amphenol 54601-906LF |
| R1 | Resistor | 1 kΩ | 0805 (or 1/4 W through-hole) | 1 | — |
| R2 | Resistor | 2 kΩ | 0805 (or 1/4 W through-hole) | 1 | — |
| — | Pin headers (female) | 2.54 mm | 1×7, through-hole | 2 | — |

> **Note:** If you use through-hole resistors, increase the board width by a
> few mm to accommodate them.

## Assembly Instructions

### 1. Order the PCB

See [PCB Ordering Guide](#pcb-ordering-guide) below or the detailed
[KiCad Guide](kicad-guide.md) for Gerber generation.

### 2. Solder Components (order matters)

1. **SMD resistors first** (if using 0805) — R1 (1 kΩ) and R2 (2 kΩ). They
   sit on the top side between the RJ-12 footprint and the pin headers.
2. **RJ-12 socket (J1)** — Push through from the top, solder on the bottom.
   The socket opening should face the board edge.
3. **Female pin headers** — Two 1×7 strips for the Beetle. Solder from the
   bottom so the Beetle plugs in on top.
4. **Plug in the Beetle** — The USB-C port should face away from the RJ-12
   socket.

### 3. Connect to Desk

Use a straight-through RJ-12 6P6C cable (all 6 pins wired 1-to-1). Plug one
end into the controller's handset port and the other into J1.

### 4. Flash Firmware

Connect USB-C to the Beetle and flash with PlatformIO:

```bash
pio run -t upload
```

## PCB Ordering Guide

### JLCPCB

1. In KiCad, go to **File → Fabrication Outputs → Gerbers (.gbr)**.
2. Also generate the drill file (Excellon format).
3. Zip all Gerber + drill files together.
4. Go to <https://www.jlcpcb.com/> → **Order Now** → upload the zip.
5. Recommended settings:
   - **Layers:** 2
   - **PCB Thickness:** 1.6 mm
   - **Surface Finish:** HASL (lead-free)
   - **PCB Color:** any (green is cheapest)
   - **Dimensions:** ~35 mm × 25 mm
   - **Qty:** 5 (minimum)
6. Total cost is typically **$2–5 USD** + shipping for 5 boards.

### PCBWay

Same Gerber zip works. Upload at <https://www.pcbway.com/> → **Instant Quote**.
Use the same settings as above.

## Schematic Overview

See [schematic.md](schematic.md) for the full ASCII schematic and component
details.

```
RJ12 Pin 2 (GND) ───────────────── Beetle GND
RJ12 Pin 3 (DTX) ──[R1 1kΩ]──┬── Beetle GPIO17 (RX)
                               │
                            [R2 2kΩ]
                               │
                              GND
RJ12 Pin 4 (VCC) ───────────────── Beetle VIN (5V)
RJ12 Pin 5 (HTX) ───────────────── Beetle GPIO18 (TX)
```

## Files

| File | Description |
|------|-------------|
| [README.md](README.md) | This file — BOM, assembly, ordering |
| [schematic.md](schematic.md) | ASCII schematic and component details |
| [kicad-guide.md](kicad-guide.md) | Step-by-step KiCad PCB design guide |
