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
| `audio.kicad_sch` | stereo TRS input, per-channel bias, protection, anti-alias filters, ADC supply, and test points |
| `controls_display.kicad_sch` | EC11 encoder, push switch, I2C level translation, LCD connector, and digital expansion footprint |

Capture proceeds in reviewable slices. Component values that remain open in
`PCB.md` must be selected from calculations and manufacturer documentation,
not copied from an unrelated example.

## Current capture status — 2026-10-29

### MCU core

The MCU-core sheet is populated and the project-level MCU sheet symbol exposes
the frozen USB, audio, encoder, display, and expansion signals. The capture
includes the RP2354A, its 1.1 V switching-regulator network, supply bypassing,
12 MHz crystal, BOOTSEL and reset buttons, Serial Wire Debug (SWD) pads, native
USB and PIO-USB series resistors, and explicit no-connect flags on unused pins.

The exact L1 inductor and Y1 crystal manufacturer part numbers are stored as
hidden schematic fields. Their footprints, along with the reset/BOOTSEL switch
and SWD test-pad footprints, remain deliberately unassigned until the sourcing
and assembly review. The ordinary 0402 resistors/capacitors and RP2354A package
use standard KiCad footprints.

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
- A 2 A-hold Littelfuse 1206L200SL resettable polymer fuse is the current input
  protection candidate. It avoids the temperature-derating problem of using a
  nominal 1.5 A part on a port that advertises 1.5 A; stock and worst-case
  temperature still require review before the bill of materials is released.
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

The USB-C connector footprints, TPS25821 WSON land pattern, 470 microfarad
capacitor footprint, display-branch fuse part/footprint, and physical test-pad
style remain deliberately unassigned. Those choices depend on the mechanical
placement and JLCPCB assembly review and are fabrication blockers, not open
circuit-topology questions.

### Expected project-level findings

KiCad currently reports nine unconnected hierarchical pins and their matching
isolated-label warnings because the audio and controls/display sheets are still
blank. It also reports the known RP2354A library-copy warning: the installed
symbol is derived from RP2350A while the MCU sheet embeds the reviewed RP2354A
pin definition. These expected findings must disappear or be explicitly
resolved before final schematic approval; none comes from the USB/power sheet.
