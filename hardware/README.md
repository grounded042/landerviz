# LanderViz revision-A hardware

**2026-11-07:** the reviewed [PGA2350 carrier](carrier/README.md) Rev A
bare boards are ordered from OSH Park. The 90 × 75 mm two-layer design is
frozen; see [order status](carrier/ORDER_STATUS.md),
[manufacturing release](carrier/releases/rev-a/README.md) and
[parts sourcing](carrier/SHOPPING.md). The processor purchase assumes A4
silicon; user assembly and the PGA2350 firmware port remain ahead.
Open [the carrier project](carrier/landerviz-carrier.kicad_pro) in KiCad.
The existing `landerviz.kicad_*` files and historical sections below remain
the preserved bare-RP2354A candidate, which was not ordered.

This directory contains the KiCad 10 schematic and, after schematic approval,
the printed-circuit-board layout for LanderViz revision A.

The source of truth for requirements and open decisions is
[`../PCB.md`](../PCB.md). Do not infer electrical decisions from a footprint or
unfinished sheet when `PCB.md` still marks the item open.

## Tooling

The project currently targets KiCad 10.0.6, the version installed on the
design Mac. Firmware and software continue to use the repository's Nix flake.
The pinned Nix package set marks KiCad broken on Apple Silicon, so KiCad is not
added to the development shell until that package is usable. Open
`landerviz.kicad_pro` with the installed KiCad application.

Generated backups, lock files, and working manufacturing outputs are ignored.
The exact ordered carrier Gerber ZIP and its manifest are preserved under
`carrier/releases/rev-a/`. Source
schematics, board files, symbol/footprint libraries, design rules, and reviewed
fabrication settings belong in version control.

The project-local `sym-lib-table` and `fp-lib-table` deliberately name only the
KiCad libraries used by the capture. They also make command-line checks and a
fresh checkout independent of this Mac's global KiCad-library preferences.
`landerviz.kicad_sym` contains the reviewed TPS25821 and TLV75533PDBV pin
definitions, RP2354A, and the approved TPS22953DQC input-switch definition.
The schematic is ordinary KiCad source; opening or editing it
does not run Python or install a Python package.

## Upstream electrical reference

Raspberry Pi's `RP2350A Minimal` KiCad design R4-S1 (July 2026) is a read-only
reference for circuitry shared with RP2354A:

- <https://pip.raspberrypi.com/documents/RP-010328-CA-RP2350A%20Minimal%20KiCAD.zip>
- SHA-256:
  `c0b858d78f56fa271a827064198e8ee81e4ec246234ce6659be8a1fcd877d06b`
- License: MIT, Copyright Raspberry Pi Ltd 2026

The archive is not vendored. Its local processor regulator placement is used
as a reference-derived layout input, with attribution in `REFERENCE_LICENSE.txt`.
The complete reference board is not copied. In particular, do not copy its external QSPI flash,
Micro-USB connector, board outline, or routing. LanderViz uses RP2354A with 2
MB of in-package flash. See `PCB.md` for the exact reuse boundary.

## Capture plan

The initial hierarchy deliberately separates review domains:

| Sheet | Responsibility |
|---|---|
| `mcu.kicad_sch` | RP2354A, 12 MHz clock, core supply, decoupling, RUN, BOOTSEL, and SWD |
| `usb_power.kicad_sch` | both USB-C ports, native and PIO USB data paths, 5 V protection/switching, 3.3 V regulation, and bulk capacitance |
| `audio.kicad_sch` | stereo TRS input, per-channel bias, protection, anti-alias filters, ADC inputs, and test points |
| `controls_display.kicad_sch` | EC11 encoder, push switch, I2C level translation, and LCD connector |

Capture proceeds in reviewable slices. Component values that remain open in
`PCB.md` must be selected from calculations and manufacturer documentation,
not copied from an unrelated example.

## Current capture status — 2026-11-02

### Physical layout draft — 2026-11-04

Open `landerviz.kicad_pcb` in KiCad's PCB Editor. All 79 schematic footprints
now have working positions inside the 48 by 42 mm outline and retain their
original schematic paths, component fields, and pad connections. Three
board-only M2 mounting holes are provisional. All purchased components are
on top; the twelve bare test pads are on the underside, without adding an
assembly operation. The user approved larger and rectangular outline studies.
The regulator, crystal, processor supplies, USB, stereo audio, display,
encoder, and service connections are routed on the approved four-layer board.

Upstream USB-C and the audio jack face left. Moonlander USB-C faces right,
opposite the inputs. The display connector's opening faces outward at the top
edge, and the encoder is at the lower right. The audio jack
requires an edge notch, which is included in the draft outline. The layout
now uses four copper layers. Physical packing fits; electrical placement and routing are
still under review. The power blocks are now local, and the analog and display
groups are closer to their target positions than in the 40 mm study. Processor
bypass capacitors have priority around their respective
supply pins. Component reference text is on the fabrication layers (the assembly
drawing), keeping the tiny top side free of unreadable overlapping references.

KiCad's initial placement check reports zero schematic-parity findings (the
board components and connections match the schematic). The outline closes,
and the current placement has no courtyard overlaps, shorts, or copper-edge
clearance findings. A courtyard is the reserved assembly space around a part.
The board has zero unconnected findings. The USB locating-hole clearances
and library mismatches are resolved; see `LIBRARY_CHANGES.md` for the exact
local variants. The processor's farthest core-supply bypass, C10, is now
4.7 microfarads, as required by the datasheet, and closer to pin 23.
Ground copper fills all four layers and is joined by
stitching vias; there are no disconnected ground findings. A stitching via
is a plated hole that joins ground copper on the two sides.
There are no shorts, trace-clearance failures, courtyard overlaps, or
schematic-parity findings. No findings were waived. USB geometry/impedance, power, analog, and
assembly reviews also remain necessary.
This draft is not manufacturing-ready.

### Four-layer stack and checks — 2026-11-04

The selected working stack is JLCPCB's nominal 1.6 mm `JLC04161H-3313`, using
the manufacturer's [published copper and insulation dimensions](https://jlcpcb.com/impedance):

| Position | KiCad layer | Role |
|---|---|---|
| Top | `F.Cu` | components, local power/analog connections, USB escapes, ground fill |
| Inner 1 | `In1.Cu` | dedicated ground; no routed tracks |
| Inner 2 | `In2.Cu` | additional signal routing and ground fill |
| Bottom | `B.Cu` | existing USB/power/signal routes, bare probes, ground fill |

Outer copper is 0.035 mm; inner copper is 0.0152 mm. The insulating spacings
are 0.0994 / 1.265 / 0.0994 mm. The insulating material is FR-4, a glass/epoxy
laminate. KiCad retains nominal 1.6 mm board thickness and blue/white finish;
the 0.01 mm mask values remain rendering approximations. The selected stack
must be specified in the fabrication order, not left to an arbitrary stack.

`In1.Cu` fills as one connected ground region. The regulator has named
inner-layer keepouts beneath the inductor and switching node, following
RP2350 datasheet section 6.3.8.1 / Figure 27. The top-side fill keepout follows
the existing switching polygon's edge so that the required LX connection
remains intact. The reference regulator's component placement is retained.
C10 is corrected to the datasheet's 4.7 microfarad bypass for DVDD pin 23.

New escapes use ordinary 0.4 mm diameter / 0.2 mm drilled through vias;
these span all four layers. The project's minimum drill and via diameters now
match those values, within JLCPCB's [published capabilities](https://jlcpcb.com/capabilities/pcb-capabilities/).
The minimum annular ring remains 0.1 mm and general copper clearance 0.2 mm.
There are no blind/buried vias or newly assumed filling/capping processes.
This also resolves the previous three 0.2 mm controller-hole drill findings.

C12 moved 0.1 mm upward, with its ground via also moved upward. The right ADC
probe now connects through a small via inside the bare bottom test pad, with
shorter inner-layer branches to the series resistor and clamp. The probe has
no solder-paste aperture. Obsolete display-data detours and trial stubs were
removed. The LCD SDA via at (27.8, 8.8) mm moved to (27.8, 8.9) mm to improve
the ground reference beneath the downstream USB data pair.

Validation with KiCad 10.0.6: zero unconnected items, shorts, trace-clearance
errors, courtyard overlaps, dangling tracks/vias, or schematic-parity findings.
A separate geometric check sampled USB track centers and edges every 0.1 mm,
excluding 0.8 mm around USB via launches: all 3,798 samples have ground on
the adjacent inner layer. This verifies sampled reference coverage; it does
not establish 90-ohm differential impedance or USB electrical compliance.

### Board artwork

The working finish is blue solder mask with white silkscreen. Solder mask is
the protective coating over the copper; silkscreen is the printed lettering.
Both sides use those colors in the KiCad stackup. Enable "Use board stackup
colors" in the 3D Viewer if local appearance settings override them. The
fabrication order must separately select the desired color; the preview does
not place or configure an order. Black remains an acceptable alternative, but
mask clearances must be reviewed for that manufacturing option before release.
The stackup retains nominal 1.6 mm thickness with 35 micrometer outer and
15.2 micrometer inner copper; fabrication settings remain unreleased.

The underside has a bold `LANDERVIZ` wordmark centered at (27, 21) mm from
the upper-left board corner, with `REV A / AUDIO IN. LIGHT OUT.` at (27, 23.5)
mm. The ink clears the probe labels and through-hole hardware. The lander and flowing-ribbon
drawings were removed at the user's request. Decorative drawings are deferred
while the electrical layout is developed. Final routing and test access take
priority over ink. Press Alt+3 in the PCB Editor for KiCad's 3D Viewer and turn
the board over to inspect it; top-side service labels will be finalized after
placement stabilizes.

### Electrical placement review — continuation

The initial 40 mm physical packing was rejected because it did not adequately
preserve local power and analog circuits. See `PLACEMENT_REVIEW.md` for that
history, the subsequent outline comparison, and current routing status. A clean
overlap check must not be presented as complete electrical placement approval.
The current 48 by 42 mm draft follows the user's permission to explore larger
and rectangular outlines.

The processor regulator group now follows the local geometry of the pinned
Raspberry Pi R4-S1 reference, adjusted for this project's pad/courtyard sizes.
Its marked inductor terminal remains connected to the switch node. The crystal
signal paths are routed on top without signal vias. The local regulator
input/feedback/output, switching-node copper, filtered supply, and capacitor
ground returns are now connected. Most processor supply pins now have short
branches to their bypass capacitors, whose ground ends join the back plane.
The U1 pin 20 / C14 supply escape is now connected by a back-side branch;
the clock remains on top and the ground plane stays connected. The wider
3.3 V and 1.1 V distribution is connected. The main input fuse, protected 5 V
trunk, keyboard power switch and bulk reservoir, switched keyboard output,
and fused display feed are now connected. Short 0.25–0.4 mm pad escapes join
0.8–1.0 mm trunks; paired 0.8/0.4 mm vias carry the keyboard output between
layers. These widths need final current/temperature review before release.
The project's minimum trace width is now 0.15 mm for fine-pitch escapes; default
net-class routing width stays at 0.2 mm and local regulator power traces at
0.4 mm. Fabrication-rule signoff remains pending.

`landerviz.kicad_dru` permits 0.10 mm clearance only between the switching
node and C6/C7 pads, allowing the reference-style escape through their pad
gap. Other clearance rules remain unchanged. The local rule is based on
[JLCPCB's 1 oz pad-to-track capability](https://jlcpcb.com/capabilities/pcb-);
changing copper weight requires rechecking it. No findings are suppressed.

Four 0.3 mm drill ground vias join the processor's exposed pad (the large
ground contact underneath the chip) to the back plane. Their interaction with
solder paste and possible solder wicking through the holes must be checked
in assembly review; no premium filled/capped-via process has been selected.

The upstream USB-C A6/B6 and A7/B7 contacts are connected to their respective
data nets and to U3's electrostatic-discharge protection pads. The alternating
contacts join on opposite sides of the connector tail row, avoiding crossed
data nets. U3 has a local ground via. Both upstream and downstream data paths
now continue through their series resistors to the processor. Each signal uses
two layer transitions in the upstream link and four in the downstream link.
The processor-end vias sit outside the exposed pad, under the package body,
with tented mask rather than premium filled/capped vias. Main connector power
is routed. Configuration-channel (CC) wiring is connected on both ports.
The power controller's reference
resistor is connected. Whole-link geometry and impedance review are pending.
This is connectivity/clearance progress, not functional USB or signal-integrity
signoff; the board is not ready to power or order.

Both audio coupling capacitors now face their negative input terminals toward
the jack. C29 moved beside the right analog-to-digital converter (ADC) input.
Both filter-capacitor-to-processor paths stay on the front; the right-channel
resistor/clamp branches use the back as well. Bias, protection, filter ground
returns, and all four audio test pads are connected. Longer analog branches,
probe stubs, and their return paths remain electrical-review items.

The display clock, encoder A, debug clock/data, and reset escapes are now
connected. Display pull-up links, display data, encoder B/press, spare GPIO
probes, and boot-select are connected. R22 is now
at (38.5, 1.8) mm. Ground stitching resolved pockets created by back-side control traces;
the ground fills discard isolated copper. USB shield lugs, the display ground
pin, and U2's three thermal holes use selected solid ground connections, with
solderability still to review. No design-rule findings were waived.

Clipped package-outline ink was removed from the board; the assembly drawings
are retained, and a smaller processor pin-one dot replaces the clipped marker.
The project-owned encoder and inductor footprints retain these changes in
their source files. The edited upstream-library footprints now have matching
project-local variants. A spare-GPIO trace was rerouted around
the audio jack's non-plated mounting hole.

The user approved and the board now implements four copper layers with the
same outline and components. Earlier two-layer experiments that introduced
excessive analog detours or ground fragmentation were discarded. Those
experiments do not prove that a two-layer design is impossible.

The original same-edge USB arrangement and inward-facing LCD opening were
corrected after visual review. The local regulator block and its first traces
were moved to accommodate the left-edge USB input, USB protection was placed
behind that connector, and the probe pads were rechecked against the relocated
through-hole hardware. Pin assignments and the schematic are unchanged.

Underside probe labels: `CLK`/`DIO` are Serial Wire Debug clock/data;
`BL`/`BR` are left/right bias; `AL`/`AR` are left/right analog-to-digital
converter inputs; `GP6`/`GP7` are spare GPIO6/GPIO7 pads. The schematic test-point
references remain visible on the bottom fabrication layer.

The enclosure will be designed after the PCB is ordered, using the user's
library makerspace or a friend's 3D printer. No enclosure supplier is selected.

### MCU core

The MCU-core sheet is populated and the project-level MCU sheet symbol exposes
the frozen USB, audio, encoder, and display signals. The capture
includes the RP2354A, its 1.1 V switching-regulator network, supply bypassing,
12 MHz crystal, BOOTSEL and reset buttons, Serial Wire Debug (SWD) pads, native
USB and PIO-USB series resistors, and explicit no-connect flags on unused pins.

The exact L1 inductor, Y1 crystal, reset/BOOTSEL switches, RP2354A, and ordinary
passives now have manufacturer, JLCPCB/LCSC catalog, and footprint assignments.
L1 uses a project-local footprint copied from Abracon's land pattern; its marked
terminal is **pad 1, the +1V1 output side**, matching Raspberry Pi's R4-S1
reference. The fabrication dot was corrected after independent review; the
assembler must verify the physical dot orientation, not just the placement
rotation number. C6/C7/C9/C10 now specify the reference-design Murata
`GRM155R60J475ME47D` (JLCPCB `C82453`) instead of the unqualified CCTC
substitute. C9 has an independent ground via; the switching return uses two
adjacent vias. See [`REVIEW_RESOLUTION.md`](REVIEW_RESOLUTION.md).
The Serial Wire Debug (SWD)
and rail test points are 2 mm exposed surface pads and require no purchased
component.

### USB and power

The USB/power sheet is electrically captured and connected to the MCU sheet:

- J1 is the upstream USB-C sink/device port. Each configuration-channel (CC)
  pin has its own 5.1 kilohm pull-down; the two orientation copies of each USB
  data signal join before the MCU's 27 ohm series resistor.
- J2 is the downstream USB-C source/host port. A TPS25821 monitors both CC
  pins, advertises 1.5 A, and switches the keyboard's 5 V VBUS rail only after
  attachment. Both connector orientations join before the MCU's 22 ohm series
  resistors.
- Each USB data pair has a TPD2EUSB30DRTR two-channel electrostatic-discharge
  (ESD) protector. ESD means a short high-voltage event such as a static shock.
- A stocked 2 A-hold LUTE `1206L200/16NR` resettable polymer fuse protects the
  input. Its hold current falls to 1.42 A at 60°C, so it does not establish
  a 1.5 A continuous port rating across temperature. Supported ambient/current
  limits still require a release decision.
- The proven 470 microfarad reservoir is on the protected input side
  of the TPS25821, after the approved TPS22953 controlled-charging input
  switch. Its keyboard-port output has 4.7 microfarads, below the
  controller's 10 microfarad limit.
  C25 now sits beside J2 at (37.1,14.2) mm. U6 ramps the reservoir and every
  existing powered branch; only 1 µF bypass and the enable circuit precede it.
  Its estimated startup behavior still requires assembled-board validation.
  See [`INPUT_POWER_PROPOSAL.md`](INPUT_POWER_PROPOSAL.md) for the approved
  five-part circuit, enable threshold, ramp calculation and reverse-blocking limits.
- A TLV75533PDBVR fixed-output, 500 mA low-dropout regulator creates the 3.3 V
  logic rail. The display receives a separately protected 5 V branch.
- Labeled 5 V, 3.3 V, and ground test points are included.

The USB/power page has no electrical-rules-check findings. The exported netlist
was also checked to confirm that connector-facing data nets stop at the correct
side of each series resistor; the corresponding chip-facing nets reach only
the intended native-USB or PIO-USB pin.

Both USB-C ports use the GCT `USB4105-GF-A-060`. The TPS25821 uses a
project-local footprint built from TI's `DSS0012B` recommended WSON land pattern;
**WSON** means a very small leadless package with an exposed thermal pad. The
470 microfarad reservoir, display-branch fuse, reset/BOOTSEL switches, crystal,
inductor, and test-pad style are also assigned. Placement, cable clearance, and
assembly-orientation review remain fabrication gates, but no schematic part is
waiting for a footprint.

### Stereo audio frontend

The audio sheet is populated, electrically connected to GPIO26/ADC0 and
GPIO27/ADC1, and clean under the project-level electrical rules check. Tip is
left, ring is right, and sleeve is ground on J3. Each channel is independent:

- A 10 microfarad coupling capacitor blocks direct current from the mixer. If
  the selected part is polarized, its positive terminal faces the bias node.
- Two 22 kilohm, 1% resistors bias that node near half of 3.3 V. They present
  an approximately 11 kilohm load; with 10 microfarads, the nominal high-pass
  corner is about 1.45 Hz.
- A 10 kilohm, 1% series resistor and 2.2 nanofarad C0G, 5% capacitor form a
  first-order low-pass at about 7.2 kHz. This is intentional modest
  anti-aliasing for the firmware's 15.625 kHz-per-channel sampling, whose
  Nyquist frequency is about 7.8 kHz.
- A BAT54S-7-F dual Schottky diode in SOT-23 clamps the ADC node toward ground
  and 3.3 V. The series resistor limits fault and unpowered-input current. The
  package and ordering code match the
  [Diodes Incorporated datasheet](https://www.diodes.com/datasheet/download/BAT54S.pdf);
  leakage and assembly stock remain bill-of-materials release checks.
- TP7/TP9 expose the left/right bias nodes; TP8/TP10 expose the left/right ADC
  nodes.

J3 is frozen as the CUI `SJ1-3523N` right-angle through-hole jack using KiCad's
matching `Jack_3.5mm_CUI_SJ1-3523N_Horizontal` footprint. JLCPCB/LCSC catalog
identifier `C20182914` records the sourcing target; assembly inventory must be
rechecked when the order is released. C26/C27 are polarized 10 microfarad,
16 V surface-mount electrolytics, with the positive terminal toward the biased
ADC node. Audio test points use the same 2 mm exposed pads as the power and SWD
points. Assembled-board bring-up must confirm bias voltage, silence/noise,
bass response, high-frequency texture, and clipping before firmware gain or
calibration constants are retuned.

### Encoder and external display

The controls/display sheet now captures the proven encoder circuit and
revision-A serial-display interface:

- SW3 is frozen as the top-facing Bourns `PEC12R-4120F-S0012`, JLCPCB/LCSC
  `C143801`: 12 detents, 12 pulses, a push switch, and a 20 mm shaft. It
  connects encoder A, encoder B, and the push switch to GPIO4, GPIO5, and
  GPIO8. Its encoder common and other switch terminal return to digital ground.
  Firmware supplies all three pull-ups and debounce/decoding; the PCB adds no
  filtering that could change the validated behavior. This no-bushing variant
  requires PCB/enclosure support close to the control so that repeated pushes
  do not flex the board.
- Q1 and Q2 are BSS138 N-channel MOSFETs
  (metal-oxide-semiconductor field-effect transistors) in SOT-23
  (small-outline transistor) packages. Their sources face the 3.3 V MCU bus,
  drains face the 5 V display bus, and gates connect to 3.3 V. This is the same
  bidirectional open-drain I2C (Inter-Integrated Circuit) level-shifter
  topology demonstrated by the breadboard breakout. The schematic pin order
  matches the
  [onsemi BSS138 datasheet](https://www.onsemi.com/download/data-sheet/pdf/bss138-d.pdf).
- R20/R21 are populated 4.7 kilohm, 1% pull-ups on the 3.3 V side. R22/R23 are
  matching DNP, or do-not-populate, 5 V-side options because the accepted LCD
  backpack already contains that side's pull-ups.
- J4 is the right-angle JST `S4B-PH-K-S(LF)(SN)`, JLCPCB/LCSC `C157926`, with
  the matching KiCad footprint. Its logical pinout is pin 1 ground, pin 2
  protected display 5 V, pin 3 SDA, and pin 4 SCL. The cable has a `PHR-4`
  female JST housing at the board and one 1x4 2.54 mm female socket at the
  LCD backpack. LCSC custom cable assembly and combined JLCPCB shipping are
  the intended procurement path; cable length waits for enclosure placement.
  Individual 2.54 mm female Dupont jumper wires can substitute during bench
  testing only; they do not fit the board's 2.0 mm JST connector directly and
  do not provide keying or retention in the enclosure.

SW3 uses the project-local
`RotaryEncoder_Bourns_Vertical_PEC12R-4x20F-Sxxxx` footprint. Its five
electrical holes, two retaining-tab slots, body outline, and `A-C-B` pin order
were captured from Bourns' configuration-4 mechanical drawing and checked by
exporting the footprint from KiCad. Confirm the physical sample and JLCPCB's
assembly orientation before release. Q1/Q2 are frozen as onsemi `BSS138LT1G`
(`C82045`). Revision A intentionally has no digital expansion
connector; GPIO6 and GPIO7 terminate at labeled test pads for diagnostics or
rework only.

### Independent-review handoff

The reviewed RP2354A symbol is now in the project library; the former library
warning is resolved. Read [`REVIEW_NOTES.md`](REVIEW_NOTES.md) for the current
candidate, checks, and unresolved electrical/assembly questions. Share
[`ASTRA_REVIEW.md`](ASTRA_REVIEW.md) as the concise independent-review prompt.
`prepare_review.py` builds a source snapshot and review artifacts under the
ignored `fabrication/` directory. It uses installed KiCad and no downloaded
Python dependencies, accesses no network, and checks that sources stay unchanged.
Neither a clean automated report nor the generated files authorize an order.

For the separate Gerber ZIP, bill of materials and placement CSV required by
JLCPCB, see [`JLCPCB_ORDERING.md`](JLCPCB_ORDERING.md). `prepare_jlc.py` converts
a checked snapshot using only Python's standard library, verifies file hashes
and coordinate consistency, and leaves assembly rotations awaiting approval.
