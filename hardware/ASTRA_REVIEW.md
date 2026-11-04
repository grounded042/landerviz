# LanderViz revision-A independent review

Perform a **read-only, pre-fabrication electrical and PCB review**. This is my
one independent review opportunity. Spend effort checking the design; keep
your response concise. Do not edit, stage, commit, upload, order, or redesign
the product. Do not spend tokens explaining elementary electronics, repeating
these requirements, praising the work, or reviewing the visualizer software.

## Read in this order

In the repository, paths below are relative to its root. In the supplied ZIP,
the repository snapshot is under `source/`; generated evidence is at ZIP root.

1. `hardware/REVIEW_NOTES.md`: current candidate, changes, calculations, and
   unresolved questions. Treat the author's conclusions as claims to verify.
2. `PCB.md`: approved requirements. Historical layout notes are secondary.
3. All five `hardware/*.kicad_sch` files, `landerviz.kicad_pcb`, project rules,
   local symbols/footprints. The actual nets, pins, and copper are authoritative.
4. Package evidence: `schematics.pdf`, `board-layers.pdf`, `audit.json`,
   `components.csv`, `erc.json`, `drc.json`, and `manifest.json`.
   Pictures omit some component bodies because local footprints lack 3D models.
5. Manufacturer documents in `references/` and links in REVIEW_NOTES. Use
   targeted sections; do not read the entire 1,371-page processor datasheet
   or dump complete source/JSON files into the conversation.
   Confirm consequential claims against primary sources. No dependency installs.

If native files or essential plots cannot be inspected, say what is missing;
do not imply a completed layout review. With filesystem access, inspect the
files directly. The archive's draft manufacturing files are for review only.

## Frozen scope

- Standalone stereo line-audio acquisition; mono FFT (fast Fourier transform)
  and cohesive lighting on-chip. Proven RP2040 breadboard, **new bare RP2354A**
  PCB with 2 MB in-package flash, no external program flash.
- Native USB device upstream; Pico-PIO-USB host on GPIO0/D+, GPIO1/D− toward
  Moonlander. Forward keyboard reports and vendor lighting commands. Both
  links are USB full-speed, **12 Mbit/s**, not USB high-speed 480 Mbit/s.
- 48 × 42 mm, four layers, nominal 1.6 mm, **JLC04161H-3313** working stack.
  Components on top; twelve bare probes below. In1 is ground only; In2 has
  signals and ground. Blue mask, white ink. No enclosure designed yet.
- Upstream USB-C and TRS (tip/ring/sleeve) audio jack left; keyboard USB-C
  right; external 5 V PCF8574T character LCD through the top JST-PH connector;
  12-detent push encoder lower right. No VFD or expansion connector.
- Powered Anker hub **data port via USB-A-to-C data cable only**. No USB Power
  Delivery, arbitrary laptop/hub support, or second keyboard supply. Brightness
  capped at 175/255; measured breadboard whole-system peak 0.724 A. Upstream
  firmware declares 500 mA; downstream TPS25821 advertises 1.5 A. This known
  compliance limitation is not proof that inrush, voltage drop, or heating is safe.
- JLCPCB assembly including through-hole connectors/encoder. No paid
  filled/capped-via process assumed. No board enlargement, extra layers, new
  features, or architecture changes without user approval. Identify necessary
  corrections within this scope first; flag genuine conflicts explicitly.

## Highest-value checks

Check power/startup/faults and fuse derating; RP2354A pinout, boot, crystal,
local regulator and bypassing; both USB connector orientations and return
paths/impedance; audio bias/filter/clamps and unpowered input; display voltage
translation and cable polarity; encoder/mechanics; actual pads/drills/paste,
especially processor/controller ground-pad vias. Zero ERC/DRC findings alone
do not establish functionality or manufacturability. Challenge overlooked
problems as well as the listed concerns. Separate a prototype bring-up
measurement from a design defect that must be corrected before ordering.

## Return findings, not a tutorial

Lead with **BLOCKED / CONDITIONALLY READY / NO BLOCKERS FOUND**, with a one-line
scope/coverage statement. Then prioritize actionable findings by severity.
For each give: exact reference/pin/net and board coordinate or source location;
failure mechanism; evidence and primary-source section; smallest correction;
confidence (**confirmed / plausible / missing evidence**). Consolidate duplicate
symptoms into root causes. List only material issues; do not invent a quota.

Finish with unresolved manufacturer/user decisions and checks you could not
perform. Aim for roughly 1,200 words or fewer; use extra space only for a
critical finding. Do not claim simulation, compliance, assembly approval, or
bench validation that did not actually happen. Do not fabricate citations.
