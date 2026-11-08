# PGA2350 carrier — Rev A

2026-11-07. Open **`landerviz-carrier.kicad_pro`** in KiCad, then open its
schematic or PCB Editor. This is a separate project from the preserved bare-chip design.

**Carrier Rev A bare boards are ordered from OSH Park.** The reviewed
90 × 75 mm two-layer hardware is frozen. See [ORDER_STATUS.md](ORDER_STATUS.md)
and [SHOPPING.md](SHOPPING.md) for the next component purchases. Full assembly,
firmware port and functional bring-up remain work for the arriving boards.
Current revision markings and four dedicated M3 mounting holes are described in
[MECHANICAL.md](MECHANICAL.md). The separate [Astra handoff](ASTRA_REVIEW_BRIEF.md)
identifies the original frozen review package. The completed review and
current corrections are tracked in [REVIEW_RESOLUTION.md](REVIEW_RESOLUTION.md).
See [ROUTING.md](ROUTING.md), [PLACEMENT.md](PLACEMENT.md) and
[the routed overview](routing-overview.svg).
The [manufacturing release](releases/rev-a/README.md) preserves the exact
ordered ZIP. The [bare-board quote guide](QUOTING.md) records its settings
and the US pricing comparison.
The [parts/fit checklist](PARTS.md) and [inventory CSV](parts.csv) separate owned
parts, missing-part candidates and header/socket contacts still to confirm.
The [assembly preparation guide](ASSEMBLY.md) records soldering order, polarity
and connector orientation for the current draft.
`prepare_fit_template.py` produces a one-page US Letter PDF at actual size
with a one-inch scale check, for comparing owned parts against the saved board.
Approved direction and accepted always-powered-output limitations are in
[../CARRIER.md](../CARRIER.md).

## Included circuits

- Assumed A4 PGA2350 module; native USB goes to the input connector and
  GPIO0/1 provide the keyboard USB host through 22 ohm resistors.
- Two Soldered **333011** USB-C breakout modules. Both arrive with the tiny
  connector soldered; the user solders ordinary header pins. Input CC1/CC2
  each have their own 5.1k pull-down. Output CC1/CC2 each have their own 22k
  pull-up to 5 V, advertising 1.5 A as in the original downstream design.
  This is not proof of upstream entitlement or continuous system capability;
  supported use remains the measured Anker data port and brightness cap.
- Direct shared 5 V supply to keyboard, LCD and module VB, with 470 µF bulk
  capacitance. No input-ramp chip, attach-controlled power switch, local
  electronic current limiter or reverse blocking. The bulk capacitor charges
  directly at plug-in, as on the prototype. Previously discussed limitations
  were accepted for this prototype; do not silently add back those circuits.
- Separate left/right 22k/22k bias dividers, 10 µF coupling, 10k series and
  2.2 nF shunt filters. ADC0/1 move to GP40/41 on RP2350B.
- Direct-mounted Same Sky **SJ1-3533NG** right-angle TRS audio jack, with
  three downward-facing solder terminals. It replaces the old five-pin breakout.
- Four through-hole Vishay **BAT85S-TR** Schottky diodes replace the two tiny
  BAT54S dual-diode packages in the earlier PCB candidate. This retains the
  rail-clamp topology with solderable individual parts. Cathode pin 1 has the
  black band. Leakage/input-overdrive behaviour is not established by the
  earlier small-signal simulation; do not claim a guaranteed safe audio range.
- Existing BSS138 four-channel level converter, using channels 1/2 for the
  serial LCD; encoder A/B/push on GP4/5/8 and accessible boot/reset switches.
- Green 3 mm status LED D5 on GP6 through R13 (1k). GPIO high lights it;
  firmware-driven status behaviour will be implemented in the PGA2350 port.

There are **32 schematic assembly positions**: 13 resistors, 5 capacitors,
4 clamp diodes, 1 status LED, 4 connector/module positions J1–J4, 3 switches/encoder positions,
and 2 modules U1/U2. Headers and sockets add solder joints and must be included
in the physical bill of materials, not treated as free or already installed.
No optional expansion connectors or new visualizer features are included.

## Verified sources and boundaries

[PGA2350 schematic](https://cdn.shopify.com/s/files/1/0174/1800/files/Pimoroni_PGA2350_Schematic.pdf?v=1724926880)
and [pinout](https://cdn.shopify.com/s/files/1/0174/1800/files/pga2350_pinout_diagram.pdf?v=1723124465)
establish native USB access, module supplies and RP2350B ADC assignments.
The module includes native USB series resistors; no duplicate pair is added.
Carrier module pad identifiers are signal names (GP0, VB, etc.), not an
unverified numeric connector mapping. The study's physical footprint maps
them to the manufacturer's grid. The user accepts that drawing and the earlier
paper fit for ordering; full socket/module mating remains an assembly step.

Soldered 333011 was checked by exporting the manufacturer's actual v1.2.0
KiCad schematic to a netlist. Its eight-pin header is:

| Pin | Signal |
|---|---|
| 1 | GND |
| 2 | CC2 |
| 3 | SBU2 (unused) |
| 4 | D− |
| 5 | D+ |
| 6 | SBU1 (unused) |
| 7 | CC1 |
| 8 | VBUS / 5 V |

The manufacturer netlist connects A6/B6 together for D+, A7/B7 for D−,
all four VBUS contacts together, and all grounds/shield together. It contains
no termination resistors; the carrier therefore supplies both port roles
without removing small parts from the modules.

Manufacturer source:
[333011 product](https://soldered.com/products/usb-c-female-connector-breakout),
[hardware repository](https://github.com/SolderedElectronics/USB-C-female-connector-breakout-hardware-design),
[examined v1.2.0 schematic](https://github.com/SolderedElectronics/USB-C-female-connector-breakout-hardware-design/blob/main/CAD/V1.2.0/USB-C%20female%20connector%20breakout.kicad_sch).
Downloaded source SHA-256:
`7099bd95e128ead42f08a6231950e0f57268cbae41e147238cb3c97377c63361`.
The examined module source is retained temporarily outside the repo; no
manufacturer board design was copied into our project. Confirm the delivered
module's revision/pin order during mechanical review.

Diode source: [Vishay BAT85S datasheet](https://www.vishay.com/docs/85513/bat85s.pdf),
DO-35 axial case, black cathode band. Ordinary resistors/capacitors can be
sourced from known inventory if values, tolerances, ratings and dimensions
match. Their package sizes remain pending; no generic kit part was assigned
an invented manufacturer number.

The new jack uses the manufacturer's pinout: **1 sleeve/ground, 2 tip/left,
3 ring/right**. KiCad names its corresponding pads S, T and R. Do not reuse the
old breakout's 1/2/5 numbering. The
[manufacturer family datasheet](https://www.sameskydevices.com/product/resource/sj1-353xng.pdf)
covers 3-, 4- and 5-contact variants; the selected **3533NG** omits pins 4/5.
Its drawing was visually checked against the installed circular-hole footprint.
No old audio-breakout measurements or mating header are needed for this choice.
The user confirmed the level converter's reversed row direction and capacitor
fit on November 7. The converter footprint now matches those labels. The
processor's published dimensions and earlier paper fit are accepted for ordering;
full socket/module mating remains an assembly step.
The selected encoder is now **Same Sky ACZ11BR1E-20FD1-12C**, with 12 detents
and a push switch. The manufacturer documents hand-soldering limits; see
[ENCODER.md](ENCODER.md) for sourcing, exact pin pattern and the deliberate
round shell-hole choice. The previous Bourns was superseded after Astra review.

## Validation

KiCad 10.0.6 electrical-rules check (ERC): **zero findings**, with no rule
waivers/exclusions added. The independent exported-netlist audit passes
**16 requirements**: port pin order and terminations, unused sideband pins,
power branches, host series paths, jack channels, both audio networks and
clamp/coupling polarity, isolated ADC nodes, display voltage sides, controls,
module grounds, separate bias nodes, and the LED's polarity/series resistor.

Twelve tests run against the actual exported netlist: one accepts the clean
export; eleven deliberately corrupt a copy in memory and verify detection of
reversed coupling capacitor, reversed clamp, shared bias, wrong source CC
termination, wrong ADC routing, 5 V connected into audio, reversed LED,
bypassed/wrong LED resistor, 5 V fed into its GPIO branch and swapped jack channels. These tests do
not edit the schematic or merely repeat a zero-error ERC report.

During drafting, the independent audit caught a rotation/pin-coordinate error
that ERC did not detect. It was corrected and both checks rerun before this
draft was reported. The audit is connectivity evidence, not physical or
functional validation. The saved placement adds pad/coordinate/courtyard
auditing and fourteen placement checks, including injected faults, described in PLACEMENT.md.
The routed board is checked separately from the saved, unrouted placement
baseline. See ROUTING.md for the copper checks and their limits.
Generated exports live in ignored
`../fabrication/carrier-draft-20261106/` (PDF, SVG, netlist, ERC and audit JSON).

## Cost and next work

Prices checked 2026-11-06, excluding shipping/tax:

| Item | Quantity | Unit price | Subtotal |
|---|---:|---:|---:|
| [PGA2350 at PiShop](https://www.pishop.us/product/pga2350/) | 1 | $11.95 | $11.95 |
| [333011 at DigiKey](https://www.digikey.com/en/products/detail/soldered-electronics/333011/21720502) | 2 | $2.20 | $4.40 |
| **Processor and USB modules only** | | | **$16.35** |

This replaces the earlier $19.85 mixed-breakout comparison. Remaining costs:
missing diodes/passives, headers/sockets, bare carrier boards and shipping/tax.
DigiKey listed 399 modules when checked; availability/checkout price can change.
Soldered's own 22 × 22 mm module dimension is a reference, not a chosen carrier
outline. The user subsequently authorized a larger placement study: 90 × 75 mm.
Its area-based US bare-board estimate and remaining costs are in PLACEMENT.md.

Next: source remaining components for the ordered boards. The independent
Astra follow-up is GO and the user has placed the OSH Park order. Full socket/module mating and the
PGA2350 firmware port remain assembly/bring-up work.
Consolidate missing small parts at DigiKey to avoid repeated shipping charges.
The user will ensure A4 before buying the PGA2350; DigiKey's
module listing currently has no stock, so a single-module source remains open.
A4 stock confirmation remains an assumption, not a
reason to stop the independent schematic work. Firmware port is separate and
has not been implemented or flashed.

## Reproduce

Opening this project does not execute Python. `capture.py` explicitly
reproduces the generated draft and library; it overwrites those generated files,
so review/save manual schematic edits before invoking it again.

From the repo root:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 hardware/carrier/capture.py
/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli sch erc \
  --format json --exit-code-violations \
  -o /tmp/landerviz-carrier-erc.json hardware/carrier/landerviz-carrier.kicad_sch
/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli sch export netlist \
  --format kicadxml -o /tmp/landerviz-carrier.xml hardware/carrier/landerviz-carrier.kicad_sch
PYTHONDONTWRITEBYTECODE=1 python3 hardware/carrier/check_netlist.py /tmp/landerviz-carrier.xml
PYTHONDONTWRITEBYTECODE=1 python3 hardware/carrier/test_check_netlist.py /tmp/landerviz-carrier.xml
```

Tools use Python's standard library and installed KiCad only. Cached KiCad
Device/Switch/power symbols carry the upstream library's licence and schematic
exception; see [../KICAD_LIBRARY_LICENSE.md](../KICAD_LIBRARY_LICENSE.md).
Custom module interface symbols were authored here from verified signal names.
No packages installed, firmware flashed, external files uploaded or commits made.
