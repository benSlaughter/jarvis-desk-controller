# Jarvis Desk Controller — 3D-Printed Case

## Overview

A two-part snap-fit enclosure for the Jarvis desk controller, housing:

- DFRobot Beetle ESP32-C6 v1.1 (mounted on headers)
- Custom carrier PCB (~35 × 25 mm)
- RJ-12 6P6C through-hole socket

The case is designed for FDM printing with no supports required.

```
┌─────────────────────────────────┐
│  ╔═══════════╗   Top Shell      │
│  ║  Beetle   ║  (snaps onto     │
│  ║  ESP32-C6 ║   bottom)        │
│  ╚═══════════╝                  │
│  ┌───────────────────────┐      │
│  │     Carrier PCB       │◄─ RJ-12 opening
│  └───────────────────────┘      │
│        Bottom Shell             │
│  (flat base for tape mount)     │
└─────────────────────────────────┘
        ▲               ▲
    USB-C opening    Vent slots
```

## Approximate Case Dimensions

| Dimension | Value |
|-----------|-------|
| Length    | ~38 mm |
| Width     | ~28 mm |
| Height    | ~32 mm (both halves) |

## Print Settings

| Setting | Value |
|---------|-------|
| **Material** | PLA (PETG also works well) |
| **Layer height** | 0.2 mm |
| **Infill** | 20% |
| **Walls/perimeters** | 3 (gives ~1.2–1.5 mm wall) |
| **Supports** | **None required** |
| **Bed adhesion** | Brim recommended for top shell |
| **Nozzle** | 0.4 mm |
| **Print temp** | 200–210 °C (PLA) |
| **Bed temp** | 60 °C |

### Orientation

- **Bottom shell**: Print as-is (flat bottom on bed). No supports needed.
- **Top shell**: **Flip upside-down** so the flat ceiling sits on the build plate. No supports needed.

## Assembly Instructions

### 1. Prepare the PCB

1. Solder the Beetle ESP32-C6 onto the carrier PCB via pin headers.
2. Solder the RJ-12 socket onto the carrier PCB edge.
3. Flash firmware via USB-C before placing into the case (easier access).

### 2. Insert PCB into Bottom Shell

1. Orient the PCB so the **RJ-12 socket** faces the large opening on one short wall.
2. The **USB-C port** on the Beetle should face the small slot on the opposite short wall.
3. Press the PCB down onto the 4 support posts. The posts have pilot holes — you can optionally use M2 screws or just friction-fit.

### 3. Attach Top Shell

1. Align the top shell over the bottom, matching the lip.
2. Press down firmly until the snap-fit tabs click into place.
3. Verify both the RJ-12 and USB-C openings are unobstructed.

### 4. Mount to Desk

1. Clean the mounting surface (under/behind desk).
2. Apply double-sided mounting tape (e.g., 3M VHB) to the flat bottom of the case.
3. Press firmly onto the desk surface and hold for 30 seconds.
4. Connect the RJ-12 cable from the Jarvis desk handset port.

## Design Notes

- **Ventilation**: Slots on both long side walls and the top ceiling allow passive airflow for the ESP32's WiFi radio heat.
- **Snap-fit tabs**: 4 tabs (2 per long side) with 0.3 mm tolerance. If too tight, lightly sand the tabs. If too loose, add a small drop of CA glue.
- **Parametric**: All dimensions are variables at the top of `case.scad`. Adjust `tolerance` if your printer runs tight or loose.
- **USB-C access**: The opening allows a standard USB-C cable for reflashing without disassembly.

## What It Looks Like

The finished case is a small rounded rectangle roughly the size of a matchbox (38 × 28 × 32 mm). The bottom half is a shallow tray with four internal posts and a flat underside for tape mounting. The top half is a taller shell with ventilation slots on the sides and ceiling. The two halves click together via small snap-fit tabs on the long edges.

One short end has a rectangular opening (~15 mm wide) for the RJ-12 cable connector. The opposite short end has a smaller slot (~9 mm wide) for the USB-C port. Side walls feature a column of narrow ventilation slits, and the top has a grid of small slots for heat dissipation.

The overall aesthetic is clean and minimal — a simple utility enclosure suitable for hiding behind a standing desk.
