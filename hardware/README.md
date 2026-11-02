# LanderViz revision-A hardware

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

Generated backups, lock files, and manufacturing outputs are ignored. Source
schematics, board files, symbol/footprint libraries, design rules, and reviewed
fabrication settings belong in version control.

The project-local `sym-lib-table` and `fp-lib-table` deliberately name only the
KiCad libraries used by the capture. They also make command-line checks and a
fresh checkout independent of this Mac's global KiCad-library preferences.
`landerviz.kicad_sym` contains the reviewed TPS25821 and TLV75533PDBV pin
definitions. The schematic is ordinary KiCad source; opening or editing it
does not run Python or install a Python package.

## Upstream electrical reference

Raspberry Pi's `RP2350A Minimal` KiCad design R4-S1 (July 2026) is a read-only
reference for circuitry shared with RP2354A:

- <https://pip.raspberrypi.com/documents/RP-010328-CA-RP2350A%20Minimal%20KiCAD.zip>
- SHA-256:
  `c0b858d78f56fa271a827064198e8ee81e4ec246234ce6659be8a1fcd877d06b`
- License: MIT, Copyright Raspberry Pi Ltd 2026

The archive is not vendored because its complete board layout is not a
LanderViz design input. In particular, do not copy its external QSPI flash,
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

### MCU core

The MCU-core sheet is populated and the project-level MCU sheet symbol exposes
the frozen USB, audio, encoder, and display signals. The capture
includes the RP2354A, its 1.1 V switching-regulator network, supply bypassing,
12 MHz crystal, BOOTSEL and reset buttons, Serial Wire Debug (SWD) pads, native
USB and PIO-USB series resistors, and explicit no-connect flags on unused pins.

The exact L1 inductor, Y1 crystal, reset/BOOTSEL switches, RP2354A, and ordinary
passives now have manufacturer, JLCPCB/LCSC catalog, and footprint assignments.
L1 uses a project-local footprint copied from Abracon's land pattern; its marked
terminal is pad 2 on the RP2354 switch-node side. The Serial Wire Debug (SWD)
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
  input. It avoids the temperature-derating problem of using a nominal 1.5 A
  part on a port that advertises 1.5 A; worst-case temperature still requires
  review before the bill of materials is released.
- The proven 470 microfarad reservoir is on the protected, always-powered side
  of the TPS25821. Its switched output has 4.7 microfarads, below the
  controller's 10 microfarad limit.
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

### Expected project-level findings

KiCad currently reports only the known RP2354A library-copy warning: the
installed symbol is derived from RP2350A while the MCU sheet embeds the
reviewed RP2354A pin definition. It must be explicitly resolved before final
schematic approval; no current finding comes from the USB/power, audio,
encoder, display, or GPIO6/GPIO7 test-pad circuits.
