# Jarvis Desk Controller — Schematic

## Circuit Description

The Jarvis desk communicates over a 5 V UART bus exposed on an RJ-12 (6P6C)
connector. The Beetle ESP32-C6 runs on 3.3 V logic, so the desk→MCU receive
line needs a voltage divider (R1 + R2) to step 5 V down to ~3.3 V. The MCU→desk
transmit line can drive directly since 3.3 V is above the 5 V CMOS high
threshold on most desk controllers.

## ASCII Schematic

```
                        Beetle ESP32-C6
                     ┌───────────────────┐
                     │                   │
 RJ-12 6P6C         │  VIN (5V)  GND    │
┌──────────┐         │   ●        ●      │
│          │         │   │        │      │
│  Pin 1   │  NC     │   │        │      │
│          │         │   │        │      │
│  Pin 2   │──GND────┼───┼────────┘      │
│  (GND)   │         │   │               │
│          │         │   │               │
│  Pin 3   │──DTX──┐ │   │    GPIO17 ●   │
│  (DTX)   │       │ │   │    (RX)   │   │
│          │      ┌┴┐│   │          │   │
│          │      │R││   │          │   │
│          │      │1││   │          │   │
│          │      │ ││   │          │   │
│          │      │1││   │          │   │
│          │      │k││   │          │   │
│          │      └┬┘│   │          │   │
│          │       ├──┼───┼──────────┘   │
│          │       │  │   │               │
│          │      ┌┴┐ │   │               │
│          │      │R│ │   │               │
│          │      │2│ │   │               │
│          │      │ │ │   │               │
│          │      │2│ │   │               │
│          │      │k│ │   │               │
│          │      └┬┘ │   │               │
│          │       │  │   │               │
│          │      GND │   │               │
│          │         │   │               │
│  Pin 4   │──VCC────┼───┘               │
│  (VCC)   │         │                   │
│          │         │        GPIO18 ●   │
│  Pin 5   │──HTX────┼───────(TX)────┘   │
│  (HTX)   │         │                   │
│          │         │                   │
│  Pin 6   │  NC     │                   │
│          │         │                   │
└──────────┘         └───────────────────┘
```

## Simplified Netlist

```
RJ12.2  (GND) ───────────────────────── Beetle.GND
RJ12.3  (DTX) ───── R1 (1kΩ) ────┬──── Beetle.GPIO17 (RX)
                                  │
                              R2 (2kΩ)
                                  │
                                 GND
RJ12.4  (VCC) ───────────────────────── Beetle.VIN (5V)
RJ12.5  (HTX) ───────────────────────── Beetle.GPIO18 (TX)
```

## Voltage Divider Calculation

```
V_out = V_in × R2 / (R1 + R2)
V_out = 5.0 V × 2000 / (1000 + 2000)
V_out = 5.0 V × 0.667
V_out ≈ 3.33 V
```

This is within the ESP32-C6 GPIO input range (max 3.6 V).

## Component Details

### U1 — DFRobot Beetle ESP32-C6 (DFR1117)

- **Pinout (left side, top to bottom):** VIN, GND, 3V3, GPIO0, GPIO1, GPIO15, GPIO23
- **Pinout (right side, top to bottom):** GPIO22, GPIO21, GPIO20, GPIO19, GPIO18, GPIO17, GPIO4
- **Used pins:**
  - VIN — 5 V power input from desk
  - GND — common ground
  - GPIO17 — UART RX (desk DTX → MCU, via voltage divider)
  - GPIO18 — UART TX (MCU → desk HTX, direct)
- **Form factor:** 25 mm × 20.5 mm, 2×7 pin header at 2.54 mm pitch

### J1 — RJ-12 6P6C Socket

- **Type:** Through-hole, right-angle, PCB mount
- **Suggested parts:**
  - Amphenol 54601-906LF
  - Molex 95501-2661
  - SparkFun PRT-00132
- **Pin numbering** (looking into the socket opening, latch on top):
  - Pin 1: NC (not connected)
  - Pin 2: GND
  - Pin 3: DTX (desk transmit → MCU receive)
  - Pin 4: VCC (5 V)
  - Pin 5: HTX (MCU transmit → desk receive)
  - Pin 6: NC (not connected)

### R1 — 1 kΩ Resistor

- **Package:** 0805 SMD (preferred) or 1/4 W axial through-hole
- **Tolerance:** 1% or 5% — either is fine
- **Purpose:** Upper leg of voltage divider

### R2 — 2 kΩ Resistor

- **Package:** 0805 SMD (preferred) or 1/4 W axial through-hole
- **Tolerance:** 1% or 5% — either is fine
- **Purpose:** Lower leg of voltage divider (to GND)

## Net Summary

| Net Name | Connected Pins |
|-----------|---------------|
| GND | RJ12.2, Beetle.GND, R2 (low side) |
| VCC_5V | RJ12.4, Beetle.VIN |
| DTX_5V | RJ12.3, R1 (high side) |
| DTX_3V3 | R1 (low side), R2 (high side), Beetle.GPIO17 |
| HTX | RJ12.5, Beetle.GPIO18 |
