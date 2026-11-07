# PCBWay handoff — internal ordering guide

Updated 2026-11-06. **Not released for production.** This is a local guide;
do not upload it or the review history as manufacturing instructions.
No vendor message, upload, payment or order was performed by the agent.

## Current candidate

`fabrication/pcbway-handoff-20261106/` supersedes earlier local quote packages
for a future replacement handoff. The checked source remains
`marking-20261105`: no schematic, copper, drill, part or placement change.
Existing snapshots and quote packages are preserved.

| Upload field | File |
|---|---|
| Gerbers | `landerviz-gerbers.zip` — 12 fabrication files plus short `FABRICATION_NOTES.md` |
| BOM | `landerviz-pcbway-bom.csv` — 38 lines, quantities per board, 70 components |
| Placement | `landerviz-pcbway-centroid.csv` — 67 surface/mixed-mount positions |
| Assembly instructions | `landerviz-assembly-instructions.zip` — `READ_FIRST.md`, all 70 positions and `board-layers.pdf` only |

The local `instructions/numbered-pad-locations.csv` and `manifest.json` remain
verification records, not additional upload requirements. The three pure
through-hole parts J3/J4/SW3 must be assembled; J1/J2 have surface contacts and
through-hole shell anchors. All parts are top side. R22/R23 stay unpopulated.

## Order settings and cost

Baseline: 5 boards fabricated, 2 fully assembled, 48 × 42 mm, four layers,
1.6 mm. Blue mask follows the user's accepted response to PCBWay's black-mask
spacing restriction. Lead-free HASL follows the last visible quote-form
selection; confirm the actual quote and final summary match before release.
Older notes requested black/OSP and are superseded; do not submit both sets.

PCBWay inquiry T-5C2W1175882A: manufacturing subtotal $141.08 (components
$50.04, assembly $29.00, boards $62.04), quoted lead time 24–26 days.
User reports a revised delivered total of approximately $214. Confirm the
written delivered total and its included charges; this is not agent-verified.
JLCPCB's previous approximately $260 delivered total replaces the obsolete
$164 comparison. Neither quote is a functional guarantee.

## Before production approval

Obtain explicit acceptance in PCBWay's final manufacturing summary of the
short fabrication/assembly requirements, especially:

- Exact RP2354A A4 sourcing, with lot/marking evidence; exact regulator caps.
- U1/U2 unfilled ground-pad holes and U6 paste coverage/process/inspection.
- Proposed layer stack, 0.20 mm drills and 0.40 mm plated slots.
- Availability of customer placement approval **before** assembly.
- Blue mask, agreed finish, individual-board delivery, final cost and lead time.

Uploading notes does not establish acceptance. PCBWay's general guidance
promises postassembly photographs, not necessarily preassembly approval.
Send only specific unresolved manufacturing questions; do not ask the factory
to determine whether our complete circuit works.

## Engineering work separate from the factory handoff

See `PREORDER_CONFIDENCE.md` for unresolved electrical evidence and the
proposed verification work. It is internal, not a manufacturing instruction.
No simulation result or board bring-up evidence is implied by this cleanup.

## Reproduce and validate

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s hardware -p test_prepare_pcbway.py
PYTHONDONTWRITEBYTECODE=1 python3 hardware/prepare_pcbway.py \
  hardware/fabrication/marking-20261105 \
  hardware/fabrication/pcbway-handoff-20261106
```

The exporter checks frozen artifact hashes, stored electrical/layout audit
results and reference/coordinate agreement; it does not rerun KiCad or certify
circuit operation. It refuses to overwrite outputs. Verify Gerber/drill ZIP
members, BOM and centroid are identical to v2 before replacing uploads.

Sources:
[PCBWay AI-generated-file notice](https://www.pcbway.com/blog/PCB_Manufacturing_Information/Notice_for_AI_Generated_Design_Files_and_Manufacturing_Requirements_62656ff6.html),
[BOM/placement requirements](https://www.pcbway.com/assembly-file-requirements.html).
