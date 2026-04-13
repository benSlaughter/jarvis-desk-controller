# Jarvis Desk Controller — KiCad Design Guide

Step-by-step instructions for creating the schematic and PCB layout in
**KiCad 8** (also works with KiCad 7).

---

## 1. Create the KiCad Project

1. Open KiCad → **File → New Project**.
2. Name it `jarvis-desk-controller` and save inside the `hardware/` directory.
3. KiCad creates `.kicad_pro`, `.kicad_sch`, and `.kicad_pcb` files.

---

## 2. Schematic — Add Symbols

Open the schematic editor (`.kicad_sch`).

### 2.1 Add Components

Use **Place → Add Symbol** (shortcut `A`) to add each component:

| Ref | Symbol (KiCad Library) | Notes |
|-----|----------------------|-------|
| U1 | `Connector_Generic:Conn_02x07_Odd_Even` | Represents the Beetle pin headers |
| J1 | `Connector:RJ12` or `Connector:6P6C` | If not available, use `Connector_Generic:Conn_01x06` |
| R1 | `Device:R` | 1 kΩ |
| R2 | `Device:R` | 2 kΩ |

> **Tip:** If there's no exact RJ-12 symbol, use a generic 1×6 connector and
> rename the pins to match the RJ-12 pinout.

### 2.2 Beetle ESP32-C6 Pin Mapping

The Beetle has 2×7 pins. Map the `Conn_02x07` symbol pins as follows:

| Symbol Pin | Row | Left Header | Right Header |
|------------|-----|-------------|-------------|
| 1 / 2 | 1 | VIN | GPIO22 |
| 3 / 4 | 2 | GND | GPIO21 |
| 5 / 6 | 3 | 3V3 | GPIO20 |
| 7 / 8 | 4 | GPIO0 | GPIO19 |
| 9 / 10 | 5 | GPIO1 | GPIO18 |
| 11 / 12 | 6 | GPIO15 | GPIO17 |
| 13 / 14 | 7 | GPIO23 | GPIO4 |

**Pins used in this design:**

| Function | Beetle Pin | Symbol Pin # |
|----------|-----------|-------------|
| VIN (5V) | Left row, pin 1 | 1 |
| GND | Left row, pin 2 | 3 |
| GPIO18 (TX) | Right row, pin 5 | 10 |
| GPIO17 (RX) | Right row, pin 6 | 12 |

### 2.3 Wire the Schematic

Connect nets as follows:

1. **J1 Pin 4 (VCC)** → **U1 Pin 1 (VIN)**
2. **J1 Pin 2 (GND)** → **U1 Pin 3 (GND)** → **GND power symbol**
3. **J1 Pin 3 (DTX)** → **R1 pad 1**
4. **R1 pad 2** → **R2 pad 1** → **U1 Pin 12 (GPIO17/RX)**
5. **R2 pad 2** → **GND power symbol**
6. **J1 Pin 5 (HTX)** → **U1 Pin 10 (GPIO18/TX)**

Add **GND** power symbols from `power:GND` to all ground connections.

### 2.4 Add Net Labels

For clarity, add net labels:

- `VCC_5V` on the power rail
- `DTX_5V` between J1.3 and R1
- `DTX_3V3` between R1, R2, and GPIO17
- `HTX` between J1.5 and GPIO18

### 2.5 Annotate & Run ERC

1. **Tools → Annotate Schematic** — auto-assign reference designators.
2. **Inspect → Electrical Rules Checker** — fix any errors.
3. Add component values (1k, 2k) via double-click on each resistor.

---

## 3. Assign Footprints

Open **Tools → Assign Footprints** and set:

| Ref | Footprint |
|-----|-----------|
| U1 | `Connector_PinHeader_2.54mm:PinHeader_2x07_P2.54mm_Vertical` |
| J1 | `Connector_RJ:RJ12_Amphenol_54601` or `Connector_RJ:RJ25_Molex_95501-2661` |
| R1 | `Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm` |
| R2 | `Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm` |

> **Alternative footprints for through-hole resistors:**
> `Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal`

> **If the exact RJ-12 footprint isn't in your KiCad library:**
> Search the KiCad footprint library for `RJ` — there are several 6P6C
> options. You can also find footprints on SnapEDA or Ultra Librarian and
> import them.

---

## 4. PCB Layout

### 4.1 Board Setup

1. Open the PCB editor (`.kicad_pcb`).
2. **File → Board Setup**:
   - **Layers:** 2 (F.Cu + B.Cu)
   - **Board thickness:** 1.6 mm
   - **Design Rules:**
     - Minimum trace width: 0.2 mm
     - Minimum clearance: 0.2 mm
     - Minimum via drill: 0.3 mm
     - Minimum via diameter: 0.6 mm

### 4.2 Import Netlist

- **Tools → Update PCB from Schematic** (or press `F8`).
- All components appear in a cluster — spread them out.

### 4.3 Board Outline

Draw the board edge on the **Edge.Cuts** layer:

- **Suggested dimensions: 35 mm × 25 mm** (fits the Beetle with some margin)
- Use **Place → Line** on Edge.Cuts to draw a rectangle.
- Coordinates (origin at bottom-left corner):
  - (0, 0) → (35, 0) → (35, 25) → (0, 25) → close

> You may be able to shrink to ~32 mm × 22 mm depending on the RJ-12 footprint
> size. Adjust after placing components.

### 4.4 Place Components

Suggested placement (approximate coordinates, origin at board center):

```
┌─────────────────────────────────┐
│  ┌─────┐                        │
│  │ J1  │   [R1]  [R2]           │
│  │RJ-12│                        │
│  └─────┘                        │
│                                 │
│  ●  ●  ●  ●  ●  ●  ●          │
│  │  U1 — Beetle Headers  │     │
│  ●  ●  ●  ●  ●  ●  ●          │
│                        ↑USB-C   │
└─────────────────────────────────┘
```

1. **J1 (RJ-12):** Left edge, centered vertically. Socket opening faces left
   (off the board edge).
2. **U1 (Beetle headers):** Centered, below J1. The USB-C end of the Beetle
   should face the right edge for easy access.
3. **R1, R2:** Between J1 and U1, near the DTX/RX trace route.

### 4.5 Mounting Holes (Optional)

If space permits, add 2 mounting holes:

- Symbol: `Mechanical:MountingHole_Pad` (or `MountingHole`)
- Footprint: `MountingHole:MountingHole_2.2mm_M2_Pad_Via`
- Place in opposite corners (e.g., top-right and bottom-left).

### 4.6 Route Traces

Use **Route → Route Single Track** (`X`) with these widths:

| Net | Width | Layer |
|-----|-------|-------|
| VCC_5V | 0.5 mm | F.Cu |
| GND | 0.5 mm | F.Cu (or ground pour) |
| DTX_5V | 0.25 mm | F.Cu |
| DTX_3V3 | 0.25 mm | F.Cu |
| HTX | 0.25 mm | F.Cu |

**Routing tips:**

- Route power traces (VCC, GND) first with 0.5 mm width.
- Route signal traces (DTX, HTX) at 0.25 mm.
- Keep the voltage divider (R1 → R2 → GND) tight and close to GPIO17.
- All traces should fit on a single layer (F.Cu) for this simple design.

### 4.7 Ground Pour

Add a copper ground pour on **B.Cu** (back copper):

1. **Place → Zone** on B.Cu layer.
2. Draw the zone covering the entire board outline.
3. Assign the zone to the `GND` net.
4. Press `B` to fill the zone.

This provides a solid ground plane and improves signal integrity.

### 4.8 Silkscreen

Add text on the **F.Silkscreen** layer:

- Board name: `Jarvis Desk Ctrl`
- Version: `v1.0`
- Pin labels near J1: `GND`, `DTX`, `VCC`, `HTX`
- Orientation indicator for the Beetle (arrow or dot at pin 1)

---

## 5. Design Rule Check

1. **Inspect → Design Rules Checker** (DRC).
2. Fix any errors (unconnected nets, clearance violations, etc.).
3. Common issues:
   - Unrouted nets → route them or verify intentionally NC.
   - Clearance errors → adjust trace spacing or pad sizes.

---

## 6. Generate Manufacturing Files

### 6.1 Gerber Files

1. **File → Fabrication Outputs → Gerbers (.gbr)**
2. Select all copper layers + silkscreen + solder mask + Edge.Cuts.
3. Output directory: `hardware/gerbers/`

Required layers:

| Layer | KiCad Name | Gerber Suffix |
|-------|-----------|---------------|
| Front Copper | F.Cu | .GTL |
| Back Copper | B.Cu | .GBL |
| Front Solder Mask | F.Mask | .GTS |
| Back Solder Mask | B.Mask | .GBS |
| Front Silkscreen | F.Silkscreen | .GTO |
| Back Silkscreen | B.Silkscreen | .GBO |
| Board Outline | Edge.Cuts | .GKO |

### 6.2 Drill Files

1. **File → Fabrication Outputs → Drill Files (.drl)**
2. Format: Excellon
3. Output to the same `hardware/gerbers/` directory.

### 6.3 Package for Ordering

```bash
cd hardware/gerbers
zip ../jarvis-desk-ctrl-gerbers.zip *
```

Upload the zip to JLCPCB, PCBWay, or OSH Park.

---

## 7. Quick Reference

| Parameter | Value |
|-----------|-------|
| Board size | 35 mm × 25 mm (adjust to fit) |
| Layers | 2 |
| Thickness | 1.6 mm |
| Min trace width | 0.2 mm |
| Power trace width | 0.5 mm |
| Signal trace width | 0.25 mm |
| Min clearance | 0.2 mm |
| Min via drill | 0.3 mm |
| Component side | Top (F.Cu) |
| Ground pour | Bottom (B.Cu) |

---

## 8. Tips

- **First time with KiCad?** The official
  [Getting Started guide](https://docs.kicad.org/8.0/en/getting_started_in_kicad/getting_started_in_kicad.html)
  is excellent.
- **Can't find a footprint?** Check [SnapEDA](https://www.snapeda.com/) or
  [Ultra Librarian](https://www.ultralibrarian.com/) for free KiCad footprints.
- **Test before soldering:** Use a multimeter to verify continuity on the bare
  PCB before populating components.
- **Order extras:** PCBs are cheap in quantities of 5–10. Order spares in case
  of mistakes.
