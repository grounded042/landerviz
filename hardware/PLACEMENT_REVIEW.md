# Revision A placement review

Status: **48 by 42 mm four-layer review candidate; all connections routed**.
Current details, corrected C10, stack, and remaining questions are recorded in
[`REVIEW_NOTES.md`](REVIEW_NOTES.md). Subsequent independent-review corrections
are recorded in [`REVIEW_RESOLUTION.md`](REVIEW_RESOLUTION.md).
The routing history below includes earlier
checkpoints; its old finding counts and 7628 stack are superseded.
This is an unfinished
layout, not a fabrication release. The earlier 40 mm findings below are retained
as the reason for the revised outline.

## Approved outline study and current work

The user approved a larger outline and allowed rectangular shapes. Three
candidates were compared with the same placement priorities:

| Outline | Area | Placement-study result |
|---|---:|---|
| 45 by 45 mm | 2025 mm² | Fits; slightly more displacement from block targets |
| 48 by 42 mm | 2016 mm² | Selected; coupling capacitors stay at their targets |
| 50 by 40 mm | 2000 mm² | Fits; lower height displaces coupling and display parts |

These are heuristic placement results, not proof of optimality or routing
feasibility. Target distances are a comparison aid, not electrical signoff.
The selected outline has approximately 26% more area than the original
40 mm square. It was initially two-layer; the approved four-layer conversion
retains the outline and all purchased components on top.

U5/C21/C22 now form a local regulator block, with capacitor centers 3.0 and
3.5 mm from U5. U2 and its local capacitors also form a compact block, with
orientation chosen to bring the correct capacitor terminal toward the relevant
chip pins. The initial local regulator traces are 0.4 mm wide, with three
0.6 mm diameter / 0.3 mm drill ground vias. The back ground plane is now filled
and electrically connected through these and the subsequent ground vias.

KiCad checks show no shorts, trace-clearance errors, component courtyard
overlaps, copper-edge clearance errors, or schematic-parity findings. There
are zero unconnected findings and ten fabrication/library findings.
There are no dangling ground vias. Remaining findings are not waived.

Remaining work:
review analog and USB return paths, finish service labels, and resolve all
fabrication/library findings. Routing connectivity is not electrical signoff.

Manufacturer layout references for the local power blocks:

- [TI TLV755P datasheet](https://www.ti.com/lit/ds/symlink/tlv755p.pdf)
- [TI TPS25821 datasheet](https://www.ti.com/lit/gpn/tps25821)

### Connector-direction correction

After the user's KiCad review, the upstream USB-C connector was moved to the
left edge beside the audio jack; the Moonlander USB-C connector is on the
right edge. The side-entry LCD connector is at the top with its mating opening
facing out. The previous inward-facing rotation was an error. Its opening and
pin-row geometry were checked against the footprint and
[JST's PH connector drawing](https://www.jst-mfg.com/product/pdf/eng/ePH.pdf).
J4's electrical pin numbers remain unchanged.

The 48 by 42 mm outline is unchanged. The power reservoir, regulator block,
USB protection, fuses, mounting holes, and probe pads were repositioned around
the corrected cable directions. The regulator's initial routes were rebuilt
and checked: no shorts, trace-clearance errors, or courtyard overlaps remain.
The placement comparison above records the earlier same-edge studies; it does
not claim those candidates were re-compared with opposite-edge USB ports.

### Processor power and clock routing

The 2026 R4-S1 official reference archive was downloaded from the recorded
Raspberry Pi source and verified against the existing SHA-256 hash in README.
The local L1/C6/C7/R3/C9 geometry was adapted from that reference to this
project's component pads and courtyard sizes. Attribution and the MIT license
are retained in `REFERENCE_LICENSE.txt`. No external-flash, USB connector,
outline, or full-board routing was copied.

The crystal signal paths and their series resistor/load-capacitor branches
are now routed on top, without signal vias. Ground vias are present for the
crystal case and capacitor returns now join the continuous back ground plane.
The processor's regulator input, feedback, inductor-to-output-capacitor,
switching node, filtered supply, and local ground returns are connected.
Processor supply pins now have local bypass branches. U1 pin 20 / C14 is
connected by a back-side branch; the broader power distribution is now
connected. Reference-style topology and connectivity checks do not replace
electrical review of the finished switching loop.

The pin-20 escape required moving C14 to (18.5, 26) mm and adjusting the top
XIN route and two ground returns. A 4.85 mm back-side supply branch joins two
ordinary vias; C14's ground has its own via into the plane. The capacitor is
farther from its supply pin than the other bypass capacitors, so this is a
placement/loop-inductance review item before release, not an idealized short
reference loop. Clock signals remain on top without signal vias.

The supply ends of C8/C12/C13 were turned toward the processor, as were
C10/C15, before adding their short traces. C29 subsequently moved next to
the right ADC input as described below. C8 and C10–C18 each have a short ground via connection;
C19 shares the nearby C18 ground return. The latest check has no new copper
clearance, hole-clearance, or short findings from these routes.

### USB data routing — 2026-11-04

J1's alternating data contacts (A6/B6 for D+, A7/B7 for D-) are joined on
opposite sides of the tail-pad row, then routed to U3's protection pads. U3
was oriented to put the two data pads in matching order, with a nearby ground
via that clears the solder pads. No signal vias were added in this section.
The remainder through R7/R8 to U1 and the downstream PIO USB data path are
now connected. The processor-end escapes use tented ordinary vias under the
package body but outside the exposed ground pad. The pairs use portions of
the back layer to pass the crowded supply circuitry. Subsequent control routes
split the back fill into pockets; front ground copper and stitching vias now
join them, with no disconnected ground findings.

The upstream data nets each have two signal vias; each downstream data net
has four. R6 and R12 moved to clear these paths. No chip pin assignments,
component values, schematic connections, default clearances, or board outline
changed. DRC has no unconnected USB data nets, shorts, crossing tracks,
trace-clearance failures, courtyard overlaps, or schematic-parity findings.

Track-centerline totals across both series-resistor nets are approximately
18.12/19.84 mm upstream D+/D- and 48.52/50.63 mm downstream D+/D-. These
totals include connector-contact joining branches and exclude via-barrel and
resistor-body length; they are geometry diagnostics, not electrical-delay or
controlled-impedance certification. Full-link geometry, return paths, and
signal-integrity review remain pending. The main USB power path, both ports'
CC wiring, and 3.3 V/1.1 V distribution are now routed. Remaining processor
connections and electrical review still prevent functional use.

### Main power and input detection

The incoming VBUS contacts join around the USB connector's locating holes and
feed F1, now at (8.5, 1.8) mm. Its output feeds the regulator and a protected
5 V trunk along the top/right of the board to U2's four input pins, local
capacitors, and the 470 microfarad reservoir. The reservoir remains on the
always-powered input side of U2. The regulator's enable branch was narrowed
to 0.15 mm because it is a logic connection; its VIN path is separate.

The switched U2 output joins C25 and transitions to the back through two
0.8 mm diameter / 0.4 mm drill vias, then returns to the connector side
through another pair. Both keyboard VBUS contact groups are joined while
clearing the locating holes and data pads. The wide routes are 0.8–1.0 mm;
short local escapes are 0.25–0.5 mm. Current density, temperature rise, voltage
drop, and fault behavior still require power-layout review before fabrication;
clearance checks alone do not establish a 1.5 A continuous rating.

Local C23/C24/C25, R13, and controller ground returns join the ground plane.
C20's ground has a pair of power vias. U2's REF pin is connected to R13 via
a short back-side link. The input connector's two independent CC pull-downs
are connected, and their grounds join the plane. R12 is now near that input;
R23's unpopulated display pull-up moved beside Q2 to clear the controller.
Both downstream CC connections now run from U2 to J2.

F2 now supplies the LCD power pin through its own protected branch, routed
above the connector's data-pin row. The display data/level-shifter wiring and
optional pull-up supply branches remain unfinished.

The upstream USB shield lugs at (7.605, 4.18) and (7.605, 12.82) mm have solid
zone connections where routing pinched thermal spokes. J4 ground and U2's
three plated thermal holes also use solid connections. The latter change is
retained in the project-local footprint. J2's overlapping A12/B1 ground pads
use 0.2 mm thermal spokes. These are physical connection changes, not suppressed
findings; solderability is included in assembly review. No new copper, thermal,
or hole-clearance findings remain from this pass.

Fine-pitch signal escapes use 0.15 mm traces. The project minimum was changed
to 0.15 mm; the default net-class width remains 0.2 mm, and the small regulator
power traces use 0.4 mm. No error severities or exclusions were changed. The
latest DRC has no short, trace-clearance, track-width, courtyard, or schematic
parity findings.

The switching-node polygon is adapted from the same verified reference design,
with a local 0.10 mm clearance rule scoped only to that node versus C6/C7.
This allows the narrow pad escape while preserving the default 0.20 mm
clearance elsewhere. The 1 oz pad-to-track requirement was checked against
JLCPCB's capabilities; it must be rechecked if copper weight changes. The
switching zone contains one filled region, and ground copper on both sides is
electrically joined. There are no remaining VREG_LX, VREG_AVDD, or ground
unconnected findings.

Four 0.6/0.3 mm ground vias inside U1's exposed pad connect it to the back
ground plane. Assembly review must address solder wicking and paste coverage;
filled/capped vias have not been ordered or assumed. The back plane removes
isolated copper islands and follows the board outline, including the jack notch.
The front ground fill also removes isolated islands; fifteen added 0.5/0.3 mm
stitching vias join shared ground areas. Return-path review remains necessary,
especially where USB changes layers or analog branches use the back.

### Stereo audio and control routing — continuation

C26/C27 were rotated so their negative line-input terminals face left toward
the jack. Their positive terminals retain their original bias-net connections.
C29 is now at (24.1, 15.15) mm, with its ADC terminal facing the processor.
The filter-to-processor traces on both channels stay on the front, without
signal vias. The left resistor/clamp branches stay on the front; the right
branches also use the back. Bias dividers, coupling capacitors, protection
rails, filter grounds, and all four bottom-side audio probes are connected.

Probe centers are now TP7/BL (23.5, 26), TP8/AL (27.5, 15.3), TP9/BR
(27.5, 33), and TP10/AR (22, 17) mm. Longer right-channel analog branches
and probe stubs need noise/pickup review; passing spacing checks does not
establish analog performance. No schematic values or pin assignments changed.

The display translators, optional pull-up links, and processor clock escape
are connected. R22 moved to (38.5, 1.8) mm. Both encoder channels/press,
GPIO6/7 probes, debug clock/data, reset, boot-select, and power probes are
connected. Connectivity is verified; functional validation still requires the
assembled board and appropriate firmware.

The keyboard-side GPIO0 series-resistor via moved to (12.5, 13.1) mm, and
its back-side route was adjusted to free processor access. Both USB links
remain connected without copper-spacing failures; previous centerline lengths
above are historical and need remeasurement before signal-integrity signoff.
The GPIO6 probe trace now avoids the jack's non-plated mounting hole.

Package-outline ink that crossed mask openings or the jack notch was removed.
Fabrication drawings remain, and a small board-level pin-one dot replaces the
processor's clipped marker. The inductor and encoder source footprints match
their ink edits. Two upstream-library footprints still require project-local
variants, accounting for the remaining library mismatch warnings. The other
eight findings are the existing USB-hole clearances. The three controller-hole
drill findings were resolved by selecting supported 0.2 mm minimum drilled holes
for the four-layer fabrication rules. No severity settings or exclusions were changed.

### Layer-count review gate

The earlier two-layer checkpoint was retained while subsequent candidates connected
more pins but introduced long analog detours, crowded bypass returns, or
additional ground fragmentation. Those candidates were not accepted. Routing
heuristic failures do not prove a two-layer layout impossible.

A four-layer layout is now implemented with the same 48 by 42 mm outline,
connector directions, and component set. A dedicated ground layer and more
routing space relieved the pressure to trade return quality for connectivity.
User agreement is recorded under the existing review gate in `PCB.md`.

The working stack is JLCPCB `JLC04161H-7628`, nominal 1.6 mm, with copper
35 / 15.2 / 15.2 / 35 micrometers and insulation spacing
0.2104 / 1.065 / 0.2104 mm. `In1.Cu` contains only ground fill and fills as
one connected region; no tracks are routed there. `In2.Cu` carries additional
connections with ground fill. Top and bottom retain their existing roles.

Named inner keepouts follow RP2350 datasheet section 6.3.8.1 / Figure 27.
The top fill keepout follows the LX polygon's edge, preserving the connected
switching node while removing extra copper under the inductor. The selected
inductor orientation and regulator components remain unchanged.

C12 and its ground via moved upward by 0.1 mm. New escapes use 0.4/0.2 mm
ordinary through vias. The right ADC probe connects via (21.5, 17) mm inside
the bare bottom probe pad; shorter inner branches replace its prior detour.
Unneeded display-data wraps and trial stubs were removed. The LCD SDA via
at (27.8, 8.8) mm moved to (27.8, 8.9) mm for USB reference-plane clearance.

KiCad 10.0.6 reports zero unconnected items, shorts, trace-clearance errors,
courtyard overlaps, dangling tracks/vias, or schematic-parity findings.
At 0.1 mm sampling intervals, all 3,798 checked USB track center/edge samples
outside 0.8 mm via-launch regions have ground on the adjacent inner layer.
This geometric check does not establish controlled impedance, return quality
at launches, or USB electrical compliance. Pair widths/gaps and launch geometry
still require review for the selected stack before fabrication.

### Working finish

Both board sides now specify blue solder mask and white silkscreen in KiCad.
This is a preview/order preference, not an order placed with the manufacturer.
The nominal 1.6 mm board thickness is retained with the four-layer stack above.
Mask rendering remains approximated as 0.01 mm on each side; final fabrication
settings and the selected stack still require order review.

[JLCPCB's capabilities](https://jlcpcb.com/capabilities/pcb-) list different
mask-bridge requirements for blue and black. Color therefore remains part of
the manufacturing check, even though it adds no electronic components.

The underside name has been moved away from the old upper probe area and
reset as a bold `LANDERVIZ` at (27, 21) mm, with revision/tagline at (27, 23.5)
mm. Checks show no ink-over-pad findings for either text object.

## Earlier 40 mm findings (superseded positions)

The initial packing prioritized fitting footprints without overlap. That is
insufficient: power bypass capacitors and analog filters must be positioned as
local circuits, rather than scattered into available gaps. A net-length
optimization that ignores shared power rails also fails to catch this.

Measured footprint-center separations in the rejected 40 mm board were:

| Circuit | Parts | Separation |
|---|---|---:|
| 3.3 V regulator/input capacitor | U5–C21 | 20.4 mm |
| 3.3 V regulator/output capacitor | U5–C22 | 16.2 mm |
| Keyboard power controller/input capacitor | U2–C24 | 14.1 mm |
| Keyboard power controller/bypass capacitor | U2–C23 | 23.7 mm |
| Processor/right-channel clamp | U1–D2 | 23.0 mm |
| Display connector/clock translator | J4–Q2 | 37.5 mm |

These are straight-line center distances, not copper lengths or manufacturer
maximum-distance limits. The first four demonstrate that the power blocks
need replacement as local groups. The analog protection belongs near the ADC
input, with a short reference to ground. The display translator distance is
less critical electrically but indicates inefficient grouping and routing.

This study does not establish that a good 40 mm layout is impossible. Neither
does it prove that one is feasible. Do not route long power loops merely to
preserve the packing result.

A subsequent constrained 40 mm trial reserved compact U5/C21/C22 and
U2/C23/C24/C25 blocks first, then processor support and audio filters. With
those priorities, the courtyard-based placer could not fit C26, the first
audio coupling capacitor. The trial was discarded without saving its component
positions. This is a failed placement heuristic, not proof that no 40 mm layout
exists; the proposed next experiment is a user-approved 45 mm placement study.

## Next placement pass

1. Place U5 with C21/C22 as one compact power block, and U2 with C23/C24/C25
   as another, checking the exact manufacturer's placement guidance before
   routing. Keep input/output power paths and local ground returns short.
2. Preserve the processor's local regulator, crystal, and supply-bypass
   groups. Review each capacitor against the supply pin it serves.
3. Place each audio filter/clamp near its ADC input; keep bias and coupling
   circuitry together and away from switching power and USB paths.
4. Group the display translators and their pull-ups. Position USB protection
   near connectors and data series resistors near the processor.
5. Check that the remaining controls, probe access, mounting holes, and cable
   clearances fit around those electrical blocks before routing.

The above larger-outline study has now been authorized and performed.
Double-sided component assembly remains outside the selected design.

## Appearance

The lander and ribbon drawings are removed from the board source. Wordmark,
revision text, and probe labels remain. Decorative artwork is deferred during
electrical layout. Routing progress is recorded above; no schematic edits were
made during layout.
