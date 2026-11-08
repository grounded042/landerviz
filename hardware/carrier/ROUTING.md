# Carrier routing draft — 2026-11-07

The first complete copper pass is saved in
[landerviz-carrier.kicad_pcb](landerviz-carrier.kicad_pcb).
Open the separate carrier project in KiCad, then PCB Editor.
[routing-overview.svg](routing-overview.svg) shows the routes with component
envelopes; ground fills are omitted from that illustration for readability.
Actual copper-fill views are in the ignored draft export directory below.

**Draft for review, not a fabrication release.** The footprint and parts-fit
questions in [PARTS.md](PARTS.md) remain open. Routing does not establish that
the selected headers fit or that the assembled carrier works.

## Copper choices

The board remains **90 × 75 mm, two copper layers, 32 electrical assembly positions and four mechanical holes**.
All footprint positions, rotations, pad coordinates and pad nets match the
saved unrouted `placement-source.kicad_pcb`. The November 7 updates add four board-only mounting holes and screw clearances,
update markings and replace SW3 after Astra review. Electrical component/terminal
centres are unchanged; the encoder shell centres, drill/pad sizes and body outline
now match the replacement described in ENCODER.md. The original bare-chip board remains
unchanged from its frozen export.

A trace is a narrow copper connection; a via is a plated hole joining copper
layers. Ground fills are broad copper areas connected to the common ground.

| Item | Current draft |
|---|---|
| Main 5 V path, input → C1 branch → keyboard | 1.5 mm wide, top layer, no vias |
| Other 5 V / 3.3 V branches | 0.5 mm; inner PGA 3.3 V escape is 0.25 mm |
| Native USB pair | 0.80 mm; 0.20 mm edge spacing on the long parallel straights |
| Other signal traces / host USB escapes | 0.25 mm |
| Minimum copper clearance | 0.20 mm |
| Minimum copper-to-edge clearance | 0.40 mm |
| Vias | 22, diameter 0.80 mm / drill 0.40 mm |
| Ground | Filled on both layers, thermal spokes at through-hole ground pads |

Thermal spokes are narrow copper bridges that make a large ground-connected
pad easier to heat while soldering. The fill uses a 0.25 mm gap and 0.40 mm
spokes. Ground is common across the board; no split analog-ground island is
introduced. The keyboard supply branches at C1 rather than travelling through
the PGA module's VB pin. The direct, always-powered output and brightness cap
remain as accepted in [../CARRIER.md](../CARRIER.md).

Audio is routed before the ordinary controls. Jack-to-coupling-capacitor
paths are approximately **31.35 mm left / 45.09 mm right**. Bias nodes remain
separate. The underside is mostly ground copper, with signal escapes where
the dense module pin grid requires them.

## USB review boundary

All USB data traces stay on the top layer with no vias. Native USB lengths
are approximately **46.55 mm D+ / 44.55 mm D−**. Host paths, including the
carrier portions on both sides of the 22 Ω resistors, are approximately
**23.74 mm D+ / 22.79 mm D−**; these exclude module traces and connector internals.

USB paths reserve underside space for ground copper outside the PGA pin field.
The pin escapes fan apart, and pin/pad clearances and the dense module escapes
still interrupt ground immediately beneath some short portions. Inspection sampled
the filled underside at 0.1 mm intervals along the USB centre lines: about
94% of the native pair has ground directly beneath it. The shorter host and
resistor/connector escapes have additional interruptions. This percentage is
an inspection aid, not an electrical acceptance threshold.

The [desktop USB review](USB_REVIEW.md) widened the native pair from 0.25 mm
to 0.80 mm without moving components. These are short prototype routes,
not verified 90 Ω differential pairs.
Differential impedance describes how the paired traces behave electrically
during fast signal transitions; it depends on spacing, width and the board's
layer construction. Clearance/connectivity checks do not establish it, and
the carrier/header/module transitions have not been simulated or measured.
No USB-compliance or guaranteed-function claim follows from this draft.

Reference: Raspberry Pi's
[Hardware design with RP2350](https://datasheets.raspberrypi.com/rp2350/hardware-design-with-rp2350.pdf)
discusses routing USB over ground and two-layer constraints. Its example
dimensions must not be treated as validation of this different carrier geometry.

## Validation recorded

KiCad **10.0.6**, zones refilled and board saved:

- **0 design-rule violations.**
- **0 unconnected items.**
- **0 schematic parity issues** (board/schematic mismatches).
- No DRC exclusions added to obtain these results.
- All 32 electrical plus four mechanical footprint/pad placements compared against the unrouted snapshot;
  saved-board pad/net/coordinate/courtyard audit passes with `--allow-routing`.
- Twelve exported-netlist fault tests, fourteen placement checks and four
  routing-search tests pass. The schematic's independent 16-requirement audit
  and electrical-rules check remain separate checks.

The front/back filled-copper plots were visually inspected. Exports live in
`hardware/fabrication/carrier-followup-20261107/`: `checks/routing-drc.json`,
`front.pdf`, `back.pdf` and matching PNG views. They are draft
review artifacts, not an order package. The old `placement-drc.json` describes
the earlier unrouted snapshot and is not the current routing result.

Before ordering, obtain the focused follow-up described in
ASTRA_FOLLOWUP_PROMPT.md. User-confirmed converter row order and capacitor fit,
and the accepted manufacturer-dimension basis for the processor, are recorded
in REVIEW_RESOLUTION.md. Full socket/module mating and functional USB/audio
checks remain assembly/bring-up work. The user will ensure A4 before buying
the processor module. The PGA2350 firmware port remains separate work.

## Reproduce

`route_board.py` uses installed KiCad and Python's standard library. USB paths
are explicit; remaining nets use the local two-layer search in `route_grid.py`.
KiCad, independently of that search, checks the saved copper geometry.

**Running the router replaces generated copper in its output board. Preserve
manual routing edits first.** It reads `placement-source.kicad_pcb`, refuses
a source containing tracks/copper-fill zones (mechanical rule areas are allowed) and writes only after every route succeeds.
Use `--output /tmp/landerviz-routing.kicad_pcb` to experiment without replacing
the main board; rule checks on that output also need the same project rules.
The generator does not perform zone filling or claim DRC success itself.

From the repository root:

```sh
/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3 \
  hardware/carrier/route_board.py
/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli pcb drc \
  --refill-zones --save-board --schematic-parity --format json \
  --output /tmp/landerviz-carrier-routing-drc.json \
  hardware/carrier/landerviz-carrier.kicad_pcb
python3 hardware/carrier/test_route_grid.py
```

The report's three finding lists must all be empty. KiCad's normal DRC exit
status alone is insufficient; inspect the report or use `--exit-code-violations`.
No external autorouter, dependency installation, uploads or purchases are used.
