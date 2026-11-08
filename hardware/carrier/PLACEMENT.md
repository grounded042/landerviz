# Carrier placement study — 2026-11-07

**90 × 75 mm, two copper layers, 32 electrical positions plus four dedicated M3 mounting holes.**
The agreed placement is retained in `placement-source.kicad_pcb` (unrouted).
The main `landerviz-carrier.kicad_pcb` now contains the [routing draft](ROUTING.md).
The user authorized a larger carrier on 2026-11-06. This outline provides
hand-soldering access; it is a study, not a frozen size or an ordering release.
Open [landerviz-carrier.kicad_pcb](landerviz-carrier.kicad_pcb) in KiCad's PCB
Editor. [placement-overview.svg](placement-overview.svg) is a labelled top view.
Amber modules have assumed dimensions. It is not a photorealistic rendering.

## Arrangement

- USB input and auxiliary audio enter on the left; Moonlander USB exits right.
- The PGA2350 is central. Its manufacturer's top edge faces down on the carrier;
  this puts its analog pins toward the audio circuitry and host pins rightward.
- The external LCD cable connects at the lower left through an ordinary four-pin
  header, centred in the gap between U2 and the C2 audio column, and can run
  out over the bottom edge. Pins 1–4 run top to
  bottom: GND, 5 V, SDA, SCL. This leaves the top boot/reset area clear of the cable.
- The encoder is at the lower right. The study retains the earlier selected
  **Same Sky ACZ11BR1E-20FD1-12C, 12 detents**. It replaces the Bourns after
  Astra review; the owned 20-detent Taiss part is not assumed interchangeable.
- Audio components occupy the lower middle; both complete columns starting at
  C2/C4 were shifted 4 mm right to give the LCD header more working room.
  The level converter is lower left.
  Boot/reset buttons are accessible along the top, away from the display cable.
- A 3 mm green status LED is at the top centre, with its 1k series resistor
  beside it. The user authorized these two added parts on 2026-11-06.

All 32 electrical positions have schematic identities, footprints and pad nets.
No SMD (surface-mount device) assembly is required on the carrier. Purchased
modules have their tiny components already soldered. The PGA2350 itself ships
without headers: full population needs 64 male pins plus 64 carrier socket
contacts. Socket strip housing sizes/stack height still need selection; a
correct hole grid does not establish that arbitrary socket bodies fit together.

## Mechanical evidence and remaining measurements

| Position | Geometry used | Status |
|---|---|---|
| U1 PGA2350 | 25.4 mm square, outer two rows of a 10 × 10 grid, 2.54 mm pitch | Manufacturer drawing/pinout; user confirms owned 2×3 socket corner paper fit; user accepts manufacturer dimensions/paper fit for ordering; full frame/mating are assembly steps |
| J1/J2 Soldered 333011 | 22 mm square; header 9.5 mm right of centre, eight pins at 2.54 mm pitch; mounting holes at local (−8, ±8) mm | Manufacturer v1.2.0 PCB; verify delivered revision |
| J3 Same Sky SJ1-3533NG | Direct right-angle jack; 18 mm overall depth, 8.2 mm body width; three 2 mm circular terminal holes | Manufacturer drawing matched to installed KiCad footprint; replaces the breakout |
| U2 existing level converter | Approx. **12.7 × 14.3 mm** body from user inch-ruler measurements; **10.16 mm** between header rows from E/G fit | 15 × 18 mm clearance reservation includes ruler uncertainty/socket room; labelled-side HV-left/LV-right row order confirmed: 4,3,GND,supply,2,1 |
| C1 470 µF | User reports approx. 1/4 inch (6.35 mm) diameter; study retains Ø8 mm envelope, 3.5 mm lead pitch | Body fits the reserved envelope; user confirms capacitor lead fit on November 7 |
| C2/C4 10 µF | User reports approx. 3/16 inch (4.76 mm) diameter; study retains Ø6.3 mm envelope, 2.5 mm lead pitch | Body fits the reserved envelope; user confirms capacitor lead fit on November 7 |
| C3/C5 2.2 nF | Generic body outline 5 × 2.5 mm; 7.1 × 3.0 mm courtyard; 5 mm lead pitch | K222J15C0GF5TH5 candidate has 4.0 × 2.6 mm maximum body and H5 formed leads; fits holes/courtyard, approximate fabrication outline |
| R1–R12 | Axial 6.3 × 2.5 mm body, 7.62 mm lead pitch | Candidate ¼ W package; confirm kit dimensions/tolerance |
| D1–D4 | Vishay BAT85S-TR DO-35, 7.62 mm lead pitch | Datasheet package, exact selected part |
| D5 | Kingbright WP710A10LZGCK green low-current LED, 3 mm body class, 2.54 mm lead pitch | Selected through-hole part; R13 is a 1k axial series resistor |
| SW1/SW2 | Generic 6 mm tactile switch footprint | Select actual switches before release |
| SW3 | Same Sky ACZ11BR1E-20FD1-12C; 1.2 mm contact drills, shell centres 9.4 mm apart | Dedicated footprint; deliberate 3.2 mm round shell holes, see ENCODER.md |

Connector body projections are reserved on the front fabrication layer
(`F.Fab`, assembly outlines); courtyards (`F.CrtYd`) reserve component clearance.
The USB module mounting holes are repeated as carrier clearance holes. Hardware
and standoff heights are not selected. Module heights, plug clearance, knob
diameter and underside solder clearance still need a fit check. No enclosure
has been designed. Four dedicated carrier mounting holes and their hardware
clearances are now specified in [MECHANICAL.md](MECHANICAL.md); they are
separate from the USB module clearance holes.

Sources:

- [Pimoroni mechanical drawing](https://cdn.shopify.com/s/files/1/0174/1800/files/PGA2350-mechanical-diagram.pdf?v=1725452657),
  [pinout](https://cdn.shopify.com/s/files/1/0174/1800/files/pga2350_pinout_diagram.pdf?v=1723124465),
  [product/header requirements](https://shop.pimoroni.com/products/pga2350).
- [Soldered v1.2.0 PCB](https://github.com/SolderedElectronics/USB-C-female-connector-breakout-hardware-design/blob/main/CAD/V1.2.0/USB-C%20female%20connector%20breakout.kicad_pcb).
  Carrier footprints were authored from dimensional/pin facts, not copied from
  the manufacturer's board. Opening the carrier does not download these files.
- [Same Sky SJ1-353XNG datasheet](https://www.sameskydevices.com/product/resource/sj1-353xng.pdf),
  2025-03-10 revision, page 2. **SJ1-3533NG** has sleeve/tip/ring only; pins 4/5
  are not populated. Pad names S/T/R match manufacturer pins 1/2/3. Relative to
  S, T is (2, 2.4) mm and R is (2, 7.9) mm in the footprint's top view.
  The existing KiCad circular-hole variant uses 2 mm holes around the blade
  terminals, avoiding plated slots. The jack needs no board-edge notch.

J3's opening faces left, 0.5 mm inside the carrier edge, keeping its printed
outline clear of the edge. Its connector axis is approximately 7 mm above the
PCB; final enclosure/plug-body clearance remains part of the mechanical check.
This is a direct three-terminal part: no extra analog breakout or socket.

## Cost checkpoint

At this outline, area is 6,750 mm² = 10.462 in².
[OSH Park's US two-layer prototype rate](https://docs.oshpark.com/services/two-layer/)
is $5/in² for **three bare boards**, giving an area-based estimate of **$52.31**
(rate checked 2026-11-06). The user subsequently uploaded the November 6
quote snapshot: OSH Park quoted **$52.30 for three boards** on November 7.

Adding the previously checked $16.35 for one PGA2350 and two USB modules gives
**$68.66 for three bare carriers plus modules for ONE build**, before missing
components, socket/header purchases, component shipping and tax. This is not
a complete delivered total. Reuse the existing audio/display modules and kit
parts when confirmed compatible. No factory-assembly/setup/feeder fee applies
to the proposed hand assembly. The user selected OSH Park on November 7;
independent Astra review remains pending before ordering.
The [parts worksheet](PARTS.md) adds priced missing-part candidates and records
the socket housing fit issue; it does not turn this subtotal into a full quote.

The larger outline increases bare-board cost at area-priced vendors. Before
fabrication, confirm these mechanical placeholders and finish the missing-parts
list so a real delivered-cost comparison can be made. Do not order from the
study or represent the subtotal as the finished project price.

## Validation and next work

The electrical draft still passes its ERC (electrical-rules check), independent
netlist audit and twelve fault-injection tests. `check_placement.py` additionally
compares all saved pad nets and schematic identities, spot-checks critical
PGA/USB/LCD/LED coordinates, confirms 64 module pads, 32 electrical positions and four board-only mounts, checks
non-overlapping courtyard envelopes, and requires zero tracks/copper-fill zones by default. Mechanical rule areas are allowed.
Pass `--allow-routing` for the main board. Fourteen placement checks cover a clean
board, wrong USB rotation, swapped ADC net, colliding modules, missing/moved
mounting holes, wrong/plated mounting drills, a missing screw keepout and a
missing solder-mask opening, wrong encoder shell coordinates, the old transposed
slot, undersized encoder terminal drills and reversed converter rows. These checks do not validate the assumed physical dimensions.

The original placement-only KiCad 10.0.6 DRC (design-rules check) reported
**0 placement violations, 0 schematic parity issues,
78 unconnected items**, with no exclusions added. The report is in ignored
`../fabrication/carrier-draft-20261106/placement-drc.json`.

The main routed board's newer DRC results are in ROUTING.md. Neither report
establishes physical fit or functional operation.

Next: focused independent follow-up of the corrected converter/encoder. The
user has confirmed converter row order and capacitor fit and accepted the
processor drawing/paper check as the order basis; full mating remains assembly work. The existing bare-chip PCB and frozen exports remain
unchanged. The separate PGA2350 firmware port has not been implemented.

## Reproduce

`placement.py` generates an unrouted board, its local study footprints,
footprint map and overview. It refuses to overwrite a board containing copper
tracks or zones. Use an explicit `--output` for a new placement study; review
changes before replacing `placement-source.kicad_pcb` or regenerating routing.
It uses installed KiCad's `pcbnew` module; no pip dependencies are installed.
`footprints.json` is generated from the explicit map in `placement.py` and
read by `capture.py`; regenerate the schematic after changing that map.

From the repo root, after exporting the schematic netlist as in README:

```sh
/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3 \
  hardware/carrier/placement.py /tmp/landerviz-carrier.xml --output /tmp/landerviz-placement.kicad_pcb
/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3 \
  hardware/carrier/check_placement.py /tmp/landerviz-carrier.xml --allow-routing
/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3 \
  hardware/carrier/test_check_placement.py /tmp/landerviz-carrier.xml
```
