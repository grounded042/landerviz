# Independent hardware review corrections — 2026-11-04

Status: **layout corrections complete; not released for fabrication**.
Addresses `ASTRA_HARDWARE_REVIEW.md` and `DEEPSEEK_HARDWARE_REVIEW.md` in the
repository root. Neither review was edited. The original review snapshot and
ZIP remain unchanged, as does the first correction snapshot. Current exports
use `marking-20261105`; the reviewed `input-power-20261104` snapshot
is also preserved unchanged.

The 48 × 42 mm outline, four-layer JLC04161H-3313 stack and processor pin
assignments remain unchanged. The user subsequently approved a TPS22953
input-ramp stage, bringing the board to 87 footprints and 70 populated
purchased positions. No firmware change or manufacturing order. The independent
initial reviewers saw the pre-U6 candidate. Astra's follow-up reviewed U6;
the subsequent local bypass corrections below were checked locally.
Coordinates below are millimetres from the upper-left outline; add (100,100)
for native KiCad coordinates.

## Corrections

| Review finding | Disposition and evidence |
|---|---|
| Astra 1: L1 orientation | **Corrected.** Moved the fabrication dot to the pad-1/output end in the library and board. Pad 1 remains +1V1 at (20,9.65); pad 2 remains LX at (22,9.65). Corrected dot: (20.35,9.20). No pad swap or electrical reroute. The R4-S1 reference dot is likewise beside its +1V1 pad 1. Assembly rotation still needs visual approval against the actual physical dot. |
| Astra 2: C9 return and switching vias | **Corrected.** Removed the C9-to-switching-return trace. C9 pin 2 now has a short independent top trace to a 0.4/0.2 mm ground via at (22.7,12.3). A named top fill-only keepout prevents the pour from recreating the shared path. Added the second switching-return via at (22.2,11.7), 0.6 mm from the existing (22.2,12.3) via. Actual filled-copper union confirms C9 and the switching return occupy different top copper islands, each connecting into the ground plane. |
| Astra 3 / DeepSeek 5: regulator capacitor qualification | **Undocumented substitution removed.** C6/C7/C9/C10 now specify Murata GRM155R60J475ME47D, JLCPCB C82453: same nominal 4.7 µF, same 0402 footprint. R4-S1 specifies this exact part for its regulator capacitors. Manufacturer, part and catalog fields agree in the schematic and board. This is reference-design precedent, not a claim of new bias/impedance measurements or complete qualification across temperature. |
| Astra 5: C25 placement | **Corrected.** Moved the existing 4.7 µF capacitor from (29,15.5) to (37.1,14.2), rotated 180°. Its supply pad is now 3.8305 mm straight-line from J2 VBUS rather than approximately 12.48 mm. New 0.5 mm top branch joins the connector-side VBUS feed; ground runs 0.825 mm to an ordinary via at (35.5,14.2). Existing power trunk and output capacitance are retained. This improves placement without claiming measured transient performance. |

The inductor marking and original shared C9 return were design mistakes, not
DRC exceptions. The reference establishes the physical orientation; electrical
net correctness alone does not establish it.

## Remaining release decisions

**Exposed-pad assembly — Astra 4 / DeepSeek 4.** The holes beneath U1/U2
remain unfilled, and paste geometry is unchanged. Ordinary tenting means a
solder-mask covering; it is not resin filling with a copper cap. Obtain
manufacturer acceptance of this exact geometry/process or approve a costed
alternative. The ready-to-send request is
[`ASSEMBLY_REVIEW_REQUEST.md`](ASSEMBLY_REVIEW_REQUEST.md); it has not been sent.
No filled-via process or additional layers have been selected.

**Upstream charging surge — DeepSeek 2 / Astra release condition.** The user
approved the five-part circuit in [`INPUT_POWER_PROPOSAL.md`](INPUT_POWER_PROPOSAL.md).
It is implemented: F1 -> VBUS_FUSED -> TPS22953 U6 -> +5V / C20 / existing loads.
Only the 1 µF input bypass and enable/bias circuit precede U6. The estimated
18 ms full ramp and 142 mA capacitor-charging component are calculated, not
measured. C20 remains before the separate TPS25821 keyboard-port switch.
The uncontrolled-reservoir connection is corrected; actual startup behavior
still needs assembled-board validation. U6 adds neither a hard current limit
nor universal USB compliance, and blocks reverse current only when disabled.
The existing fuse and downstream protection remain necessary.

**Current/temperature claim — DeepSeek 3.** The existing fuse's hold current
falls to 1.42 A at 60°C. The 1.5 A advertisement is not a verified continuous
system rating. Supported ambient and load limits remain to be frozen; neither
a larger fuse nor a changed advertisement was selected in this pass.

## Other findings

- **DeepSeek 1, silicon stepping:** PCB.md already requires A4, or an explicitly
  accepted later stepping. Confirm the actual purchased part; no assumption
  that its catalog number proves the lot's stepping. Do not add 8.2 kΩ
  pull-downs as a universal USB termination fix. The review's A2 workaround
  does not qualify the complete PIO-USB host circuit; firmware support and
  attach/detach remain raw-board bring-up checks.
- **DeepSeek 6, unpowered audio:** plausible rail injection through the input
  network remains a bring-up condition. The review's specific 2.7 V rail
  prediction is not established by this AC-coupled, loaded circuit. No rail
  disconnect or extra protection components were added.
- **DeepSeek 7, right bias routing:** longer routing is confirmed; a resulting
  noise/channel-balance failure is not. No analog reroute in this local
  regulator/power-bypass correction pass. Verify both channels on the PCB.
- **DeepSeek 8, USB:** unchanged and explicitly not controlled-impedance
  certified. Both links are 12 Mbit/s full-speed. Existing ground-coverage
  sampling still passes; it does not replace waveform or functional checks.
  No gap-only reroute based on an approximate impedance formula.
- **C14:** retained at (18.5,26), with its disclosed 4.85 mm back branch.
  The previous relocation trial crowded the crystal. Astra did not consider
  this distance alone a separate prototype blocker; no new clock-layout risk
  was introduced to shorten it here.
- **U2 REF:** the longer path is retained. Its immunity has not been measured;
  it is a lower-priority routing concern, not a confirmed pin/net error.
- **VBUS transient suppression:** no new suppressor added. This suggestion
  would add circuitry and needs a defined transient/protection target.
- **Procurement/mechanics:** assembly rotations, modified USB ground lands,
  plated slots, surface finish, live stock/quote, encoder support, provisional
  mounting holes and LCD harness polarity remain release checks. No new
  enclosure work or broad breadboard test campaign is requested.

## Astra follow-up dispositions

- **Finding 2 — output bypass: corrected.** Rotate U6 to 180° at
  (15.05,2), C21 to 180° at (11.8,2.8), and shift/rotate F1 at (7.8,1.8)
  for assembly clearance and a clean input route. The OUT-to-C21 branch is
  1.5993 mm of drawn top copper with no vias, replacing the reviewed indirect
  path. C21-to-U5-IN remains direct top copper, 2.2522 mm. Input bypass C30
  moves to (17.6,1.9), with a 1.6846 mm top branch; CT's drawn connection to
  C31 at (12.5,1) is 1.07 mm. Lengths exclude pad spreading and do not model
  inductance or prove transient performance. No parts, layers or outline added.
- **Finding 1 — factory acceptance: open.** The request now explicitly includes
  U6's 65.4% exposed-pad paste coverage versus TI's 86% example, in addition
  to U1/U2's unfilled holes. No stencil enlargement inferred without process
  advice. The draft refers to the new snapshot; it remains unsent.
- **Finding 3 — input hot-plug ringing: open, not demonstrated.** With the
  actual Anker data port and cable, capture U6 IN/BIAS during insertion and
  compare excursions with its 6 V absolute maximum. A USB meter cannot resolve
  fast spikes; this needs an oscilloscope (a voltage-versus-time instrument)
  with a short ground connection. Add damping/clamping only if the evidence
  requires a defined circuit change; none added here.
- **Finding 4 — loaded start, disable and fault behavior: open.** On assembled
  hardware, capture IN/OUT/EN and current during loaded cold starts, brownouts
  and rapid reconnects. Controlled faults require a current-limited setup,
  not a direct short on the user's Mac/hub. The estimated capacitor-charging
  current is not total startup current or a hard current limit. Reverse
  blocking acts only after disable; there is no output-discharge feature.
  These are checks of the new PCB power stage, not repeat breadboard tests.

## Board identification — 2026-11-05

Added underside `2026-11 / © 2026 Jon Carl` beneath the existing LANDERVIZ
wordmark and REV A line. Text is mirrored correctly for the bottom side,
0.85 mm high with 0.15 mm strokes. This marking-only update changes no
copper, components, connections or board geometry. The prior
`followup-fixes-20261104` package remains frozen; current upload files use
`jlc-marking-20261105`.

## Validation

- KiCad 10.0.6: all-sheet electrical-rules check (ERC) reports zero findings;
  full design-rules check (DRC), with zone refill, all track errors and
  schematic parity, reports zero violations, zero unconnected items and zero
  parity issues. No rule relaxation or exclusion was added.
- Audit: 79 topology, 16 value, 9 prior-review and 11 input-power checks
  pass. All 3,798 USB reference samples pass, with the existing 0.8 mm launch
  exclusion. The nine new checks fail on the frozen original candidate and
  pass on the corrected source, including the copper isolation check.
- Source comparison verifies unchanged existing pad nets except F1 pin 2,
  which intentionally moves to VBUS_FUSED. The outline is unchanged; the three
  approved existing placements moved and the five new parts were added.
  Relative to Astra's follow-up snapshot, only F1/C21/U6/C30/C31 placements
  change; every pad net, USB trace/via geometry and the outline are identical.
  New direct-bypass checks fail for the old U6 input/output paths and pass
  after correction; the existing U5 input-bypass check passes in both.
  These are geometry/connectivity checks, not regulator-stability, thermal,
  inrush, solder-process or USB-compliance measurements.

Reproduce the corrected local export without overwriting the reviewed baseline:

```sh
/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3 \
  hardware/prepare_review.py --snapshot-name marking-20261105 \
  --references /tmp/landerviz-inrush-references
```

## Primary evidence

- [RP2350 datasheet §6.3.8](https://pip-assets.raspberrypi.com/categories/1214-rp2350/documents/RP-008373-DS-2-rp2350-datasheet.pdf): physical inductor orientation, separate filtered return, adjacent switching-return vias and capacitor requirements. Use the frozen PDF's figure/page numbers when comparing the reviewed snapshot; the live document can change.
- [Official R4-S1 KiCad reference](https://pip.raspberrypi.com/documents/RP-010328-CA-RP2350A%20Minimal%20KiCAD.zip): L1 physical dot/output relationship and exact Murata regulator part, also captured in the original review package.
- [JLCPCB C82453 catalog](https://jlcpcb.com/partdetail/MurataElectronics-GRM155R60J475ME47D/C82453): exact part and 0402 package verified 2026-11-04; quantity, price and reservation are not established by the listing.
- [TPS25821 §8.2.1 / §10](https://www.ti.com/lit/ds/symlink/tps25821.pdf): local output bypass at the connector and power-path layout.
- [JLC via covering](https://jlcpcb.com/help/article/pcb-via-covering): tenting versus filling/capping and requirements for identifying holes in solder pads.

- [TPS22953 datasheet](https://www.ti.com/lit/ds/symlink/tps22953.pdf): the approved addition, pinout, unused PG grounding, ramp timing, reverse blocking and package drawing. Captured in the current reference package.
