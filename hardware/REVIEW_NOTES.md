# Revision A: review candidate, 2026-11-04

Status: **ready for independent review; NOT released for fabrication**.
`ASTRA_REVIEW.md` is the shareable request. `PCB.md` contains requirements.
This document supersedes older routing-progress counts and stack choices in
the placement history. Do not infer that review readiness means order readiness.

## Candidate and changes

- 48 × 42 mm outline; four layers; 82 footprints: 79 schematic items and
  three provisional M2 mounting holes. 67 purchased positions, of which
  R22/R23 are do-not-populate (DNP): 65 intended populated components.
- Purchased parts remain on top. The twelve underside test pads are bare
  copper, not purchased components. Connector directions and outline unchanged.
- Resolved eight USB locating-hole clearance errors with one 0.30 mm corner
  chamfer on each affected ground land. Hole centers/diameters, pad centers,
  pad bounding dimensions, and shell slots remain unchanged. The default
  0.25 mm hole-to-copper constraint is retained. Assembly must assess the
  modified lands; see `LIBRARY_CHANGES.md`.
- Captured the processor/audio/USB footprint variants and reviewed processor
  symbol in project libraries. No processor pin definition changed.
- Corrected **C10 from 100 nF to 4.7 µF**, as required for the farthest DVDD
  supply pin (U1 pin 23) by RP2350 datasheet §6.1.3. Reused C6/C7/C9's
  capacitor part `TCC0402X5R475M6R3AT`, same 0402 footprint. Moved C10 from
  (20,23.5) to (20,22.1) mm, retaining rotation −90°. Its supply connection
  from pin 23 is now about 2 mm on top; ground joins C15's nearby return.
- The attempted C14 relocation was rejected because it crowded the clock
  layout. **C14 remains at (18.5,26)** with the original 4.85 mm back-side
  supply branch. This is a specific residual placement question, not an
  improvement claimed complete.
- Refined the working stack from 7628 to **JLC04161H-3313**, and made all USB
  data tracks 0.15 mm wide, including former 0.20 mm bottom sections. Centerlines,
  pads, and USB via count unchanged. Added a `USB_FS` routing class with
  0.15 mm width / 0.25 mm nominal pair gap; actual routing is not a uniform
  coupled pair. Added three ground vias at (8.5,16), (16.6,9.6), (34.8,7.2).
- Added service labels and the powered-hub-only marking. Preserved the
  underside wordmark. No firmware or visualizer changes in this pass.

All coordinates here are millimetres relative to the upper-left outline;
add (100,100) for native KiCad coordinates. The audit JSON uses relative
coordinates. QFN means quad flat no-lead; the processor is QFN-60.

## Stack and USB screening

The selected [JLCPCB stack](https://jlcpcb.com/impedance) is nominal 1.6 mm:

| Layer/insulation | Thickness | Role/property |
|---|---:|---|
| F.Cu | 0.035 mm | top components, signals/power, ground fill |
| 3313 prepreg | 0.0994 mm | relative dielectric constant 4.1 |
| In1.Cu | 0.0152 mm | ground only, one filled region |
| core | 1.265 mm | relative dielectric constant 4.6 |
| In2.Cu | 0.0152 mm | signals plus ground fill |
| 3313 prepreg | 0.0994 mm | relative dielectric constant 4.1 |
| B.Cu | 0.035 mm | signals/power, ground fill, bare probes |

Mask thickness in KiCad is a rendering approximation, not the impedance
model. Nominal laminate/copper sum is 1.5642 mm before mask and fabrication
tolerances; do not substitute a different stack just because both say 1.6 mm.

An approximate microstrip screening calculation, ignoring mask and same-layer
ground, gives about 50 Ω single-ended for w=0.15, h=0.0994, t=0.035 mm,
relative permittivity 4.1. A simple coupled-pair approximation gives roughly
95 Ω at a 0.25 mm edge gap. The previous 7628 h=0.2104 mm geometry screened
much higher. These are estimates, **not a field-solver result, uniform pair
measurement, or controlled-impedance approval**. Full-speed USB's 90 Ω target,
actual widths/gaps, mask, ground pours, and transitions still need assessment.

Track-centerline sums, including connector joining branches and excluding
resistor bodies/via barrels:

| Link | D+ | D− | Signal vias per line |
|---|---:|---:|---:|
| upstream, across both resistor-side nets | 18.125 mm | 19.740 mm | 2 |
| downstream, across both resistor-side nets | 51.225 mm | 50.632 mm | 4 |

Unequal branch totals are not point-to-point electrical skew. Do not add
meanders merely to equalize these numbers. Native USB resistors are 27 Ω;
PIO-USB resistors are 22 Ω. Both are on the correct connector/chip-side nets.

`audit_board.py` checks critical topology against the exported schematic and
samples USB track centers/edges every 0.1 mm for adjacent-inner-layer ground.
It deliberately excludes 0.8 mm around USB signal vias. All 3,798 sampled
points passed in the current saved fill. Ground-via distances at transitions
are 0.7–1.727 mm; distances alone do not establish return-current quality.
This check does not inspect fields, launches, pad stubs, or USB waveforms.

## Power screening and open limits

The completed breadboard measurements in `PCB.md` remain the baseline; no
additional breadboard power campaign is being requested. The new PCB is not
bench validated. C20 is **before** U2's attachment-controlled switch; C25 is
4.7 µF after it. The 470 µF reservoir therefore remains an upstream inrush
question even though it avoids switching that capacitance onto J2.

A native-connectivity path calculation using 35 µm outer copper and 20 µm
assumed via plating estimates these selected-path resistances at 20°C:

| Portion | Track length counted | Estimated copper resistance |
|---|---:|---:|
| J1 VBUS to F1 input | 6.15 mm | 7.6 mΩ |
| F1 output to U2 input | 38.21 mm | 21.4 mΩ |
| U2 output to J2 VBUS | 21.95 mm | 16.0 mΩ |

Counting whole touched track segments and ignoring parallel paths gives a
45 mΩ screening sum. It excludes connector contacts, return plane, cable,
temperature rise, and component tolerances. F1's listed post-trip resistance
bound is 80 mΩ; U2's on-resistance maximum is 85 mΩ through 85°C junction
temperature (98 mΩ through 125°C). Adding the 85°C figure gives about 210 mΩ,
or 0.152 V at 0.724 A / 0.315 V at 1.5 A, **before** the omitted effects.
This is not a guarantee that the port can supply 1.5 A continuously.

LUTE's [1206L200 derating table, p.8](https://atta.szlcsc.com/upload/public/pdf/source/20240613/0BB7AD6F2D5DCE322359341843F40290.pdf)
lists hold currents 2.00 A at 25°C, 1.74 A at 40°C, 1.58 A at 50°C, and
1.42 A at 60°C. Thus the 2 A fuse does **not** guarantee 1.5 A plus the
controller/display at every temperature. The product's supported temperature
range and current claim remain unset; do not silently enlarge either.

TLV75533PDBVR's 500 mA device rating is not this board's thermal budget.
Its DBV-package JEDEC thermal resistance is 231.1°C/W, with a different TI
evaluation-board result of 100.8°C/W. At 5.2→3.3 V, 100 mA dissipates about
0.19 W: a 44°C rise using the JEDEC figure. Actual board temperature/current
must be checked during bring-up. Moonlander and LCD use 5 V directly.

## Material questions still needing review

1. **Processor regulator:** compare actual L1/C6/C7/R3/C9 copper and returns
   with official R4-S1 / datasheet §6.3.8, including the two-adjacent-via return
   recommendation. Inner keepouts and the top fill cut follow Figure 27.
   Confirm the actual selected capacitors' effective capacitance under DC
   bias, not just nominal µF. The substituted CCTC parts lack a verified
   operating-bias curve in this package. L1 pad 2 is the marked LX terminal.
2. **C14:** judge the remaining IOVDD pin-20 bypass branch. Its capacitor is
   farther away than desirable; do not confuse it with the corrected DVDD C10.
3. **USB:** assess the new stack, partly uncoupled routing, twelve signal vias,
   ESD stubs, and actual ground return at transitions. No simulation or eye
   diagram has been performed. The current class does not prove actual gap.
4. **Power:** upstream reservoir inrush, source constraint/500 mA declaration,
   voltage drop, fuse derating, local narrow pad escapes, regulator heat,
   discharge/fault behavior. Do not convert the 1.5 A CC advertisement into an
   unsupported system-wide compliance claim.
5. **Assembly:** U1 has four 0.6/0.3 mm ground vias inside its exposed pad;
   U2 has three 0.2 mm drilled thermal pads. Ordinary tenting is not filled,
   copper-capped via-in-pad. Check paste openings, solder loss, solid plane
   connections at U2/J4/USB shields, and the modified USB lands. Manufacturer
   acceptance of this assembly process is still required.
6. **Analog:** per-channel 10 µF coupling, 22k/22k bias, 10k series, 2.2 nF
   filter and BAT54S rail clamps; DC midpoint nominal 1.65 V. Approximate
   high-pass corner 1.45 Hz and low-pass 7.23 kHz assume a low-impedance mixer
   and unloaded ADC model. The ADC's capacitive input is not a constant
   100k resistor to ground. First-order filtering gives only about 3.4 dB
   attenuation at the 7.8125 kHz Nyquist limit; strong anti-alias rejection is
   not claimed. Check unpowered audio/backfeeding and analog return paths.
7. **Mechanics/assembly data:** M2 holes remain provisional; support near the
   push encoder and LCD cable polarity must be checked. No enclosure clearances
   have been modeled. Native placement rotations are not a JLCPCB orientation
   approval. Part IDs are sourcing targets, not a verified live assembly quote.

These are explicit review questions, not claims that every item is defective.
Only manufacturer/user decisions and assembled-board measurements can close
some of them. Report real blockers before asking for a fabrication release.

## Evidence and reproduction

`prepare_review.py` uses only Python's standard library plus the already
installed KiCad `pcbnew` module/CLI. It installs nothing, accesses no network,
and exports without saving changes to the source board. It checks source
hashes before/after, runs ERC/DRC with nonzero exit on findings, checks topology,
then builds PDFs, native placement/component lists, draft Gerbers/drills,
renders, source snapshot, and a SHA-256 manifest in ignored `fabrication/`.
It does not upload to JLCPCB or claim its assembly preview has been approved.

```sh
/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3 \
  hardware/prepare_review.py
```

An existing snapshot is never overwritten. Move it aside before regenerating.
`--references DIR` includes an optional locally downloaded manufacturer-document
directory. No song files or FLOW discussion markdown is included.

Primary references (locally captured PDFs are included when available):

- [RP2350 datasheet](https://pip-assets.raspberrypi.com/categories/1214-rp2350/documents/RP-008373-DS-2-rp2350-datasheet.pdf): §6.1, §6.3.8/Figs26–27, §12.4.3, §14.1/14.2, §14.8–14.9 and errata.
- [Hardware design with RP2350](https://pip-assets.raspberrypi.com/categories/1214-rp2350/documents/RP-008280-DS-2-hardware-design-with-rp2350.pdf): regulator, integrated-flash boot, crystal, USB sections.
- [R4-S1 native reference](https://pip.raspberrypi.com/documents/RP-010328-CA-RP2350A%20Minimal%20KiCAD.zip): source/hash/license in `README.md` and `REFERENCE_LICENSE.txt`; RP2350A without integrated flash, not a whole-board template.
- [TPS25821](https://www.ti.com/lit/ds/symlink/tps25821.pdf): pinout, electrical limits, §8.2.1.1 and §10.
- [TLV755P](https://www.ti.com/lit/ds/symlink/tlv755p.pdf): DBV package, capacitors, thermal/layout sections.
- [TPD2EUSB30](https://www.ti.com/lit/ds/symlink/tpd2eusb30.pdf), [BAT54S](https://www.diodes.com/datasheet/download/BAT54S.pdf), [BSS138](https://www.onsemi.com/download/data-sheet/pdf/bss138-d.pdf).
- [USB4105](https://gct.co/files/drawings/usb4105.pdf), [SJ1-352XN](https://www.sameskydevices.com/product/resource/sj1-352xn.pdf), [JST PH](https://www.jst-mfg.com/product/pdf/eng/ePH.pdf), [PEC12R](https://www.bourns.com/docs/product-datasheets/pec12r.pdf).
- [ABM8-272-T3](https://abracon.com/datasheets/ABM8-272-T3.pdf), [AOTA inductor](https://abracon.com/datasheets/AOTA-B201610S3R3-101-T.pdf).
- [JLCPCB capabilities](https://jlcpcb.com/capabilities/pcb-capabilities/), [stackup](https://jlcpcb.com/impedance), [via covering/process distinctions](https://jlcpcb.com/help/article/pcb-via-covering).
