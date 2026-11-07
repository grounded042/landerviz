# LanderViz Rev A — assembly requirements

2026-11-06. **For confirmation; not production approval.** Use only the
files from `pcbway-handoff-20261106`, based on snapshot `marking-20261105`.
These instructions replace the earlier PCBWay assembly notes.

## Scope and files

Manufacture 5 boards and fully assemble 2, top side only. Source exact bill
of materials (BOM) part numbers; obtain approval for substitutions. Each board
has 38 BOM lines and 70 components. Leave R22/R23 unpopulated. Display and
cable are not supplied. No firmware programming or functional test is ordered.

- `landerviz-pcbway-bom.csv`: quantities per board.
- `landerviz-pcbway-centroid.csv`: 67 surface/mixed-mount components.
- `all-70-placements.csv`: complete positions, including through-hole
  J3/J4/SW3; assemble these too. J1/J2 also have through-hole shell anchors.
- `board-layers.pdf`: placement, pad numbering, polarity and assembly drawing.
- Gerbers/drills and `FABRICATION_NOTES.md`: fabrication requirements.

All parts are on top. CSV positions are millimetres, X right/Y up, with the
original Gerber/drill origin. Rotation is native KiCad counterclockwise.
Coordinates are CAD anchors, not guaranteed machine pickup centres: reconcile
actual pad positions and package offsets before assembly programming.

## Orientation, viewed from top

| Parts | Required orientation |
|---|---|
| J1 / J2 | USB openings face left / right respectively; align contacts and shell anchors. |
| J3 | Audio opening faces left through notch; sleeve=GND, ring=LINE_R, tip=LINE_L. |
| J4 | Cable opening faces outward over top edge. Pin 1 is rightmost; pins 1–4 right-to-left are GND, display 5V, SDA, SCL. |
| L1 | Physical dot faces left, toward +1V1 / pad 1. |
| C20 | Positive terminal left. |
| C26 / C27 | Positive terminals right. |
| ICs, transistors, diodes | Match manufacturer numbered pins to drawing; D1/D2 are dual diodes. |
| SW3 | Match A/C/B, S1/S2 and retaining tabs to drawing. |

## Explicit confirmation required before production

1. **U1: RP2354A, A4 silicon revision.** Confirm lot/marking evidence.
   The package suffix A does not establish A4. No alternate revision without
   customer approval. C6/C7/C9/C10 must be Murata GRM155R60J475ME47D.
2. **Hidden ground-pad solder joints:** U1 has four unfilled 0.30 mm holes
   within its 3.4 × 3.4 mm exposed pad. U2 has three unfilled 0.20 mm holes
   within its 1.00 × 2.65 mm exposed pad; paste coverage is 62.5%.
   U6's 0.84 × 2.4 mm exposed pad has 65.4% paste coverage and no holes.
   Confirm acceptance of the supplied hole/paste geometry and state the
   inspection method. Propose any required change and cost before proceeding;
   filling/capping or stencil changes are not preauthorized.
3. Provide a placement drawing/preview for customer approval before assembly,
   including L1's physical dot and connector alignment. Confirm whether this
   preassembly approval is available; postassembly photographs are different.

Please include accepted requirements and any exceptions in PCBWay's final
manufacturing summary for customer confirmation. File upload alone does not
establish acceptance. Report discrepancies before production.
