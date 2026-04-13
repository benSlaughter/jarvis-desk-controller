# Gerber Export Instructions

`kicad-cli` is not installed on this system. To generate Gerber files:

## Option 1: KiCad GUI
1. Open `jarvis-desk.kicad_pcb` in KiCad 8
2. File → Plot (or Fabrication → Plot)
3. Select layers: F.Cu, B.Cu, F.SilkS, B.SilkS, F.Mask, B.Mask, Edge.Cuts
4. Output directory: `gerbers/`
5. Click "Plot"
6. Click "Generate Drill Files" for drill data

## Option 2: kicad-cli (if installed)
```bash
kicad-cli pcb export gerbers --output hardware/kicad/gerbers/ hardware/kicad/jarvis-desk.kicad_pcb
kicad-cli pcb export drill --output hardware/kicad/gerbers/ hardware/kicad/jarvis-desk.kicad_pcb
```

## Fabrication
Upload the generated Gerber + drill files as a zip to your PCB manufacturer (JLCPCB, PCBWay, OSH Park, etc.).
