# LanderViz carrier Rev A — original review brief

**Historical:** Astra completed this review in `ASTRA_CARRIER_REVIEW.md`.
The encoder correction and remaining gates are in [REVIEW_RESOLUTION.md](REVIEW_RESOLUTION.md).
The original frozen archive is preserved and superseded for quoting.

Review the **PGA2350 carrier only**, before ordering bare boards for hand
soldering. Keep the response concise: investigate material failures first;
avoid tutorials, feature ideas, repeated context and unnecessary token use.
Do not modify files. No purchase or manufacturing release is authorized.

## Files and scope

Frozen snapshot: `hardware/fabrication/carrier-review-20261107/`.
Shareable bundle: `landerviz-carrier-ASTRA-REVIEW.zip` in that directory.
Start with `source/landerviz-carrier.kicad_{pro,sch,pcb}`, `schematic.pdf`,
`assembly.pdf`, `front.pdf`, `back.pdf`, `checks/` and `manifest.json`.
`back.pdf` is mirrored to show the physical underside. `source/` also includes
the custom symbol/footprint libraries and unrouted placement baseline.
`context/` contains the carrier documents and generators for traceability.

Board SHA-256:
`ba67ca873a5cc0932ab86485646e4cc05abf5f74f68be782860f986aed48c762`.

Do not confuse this with `hardware/landerviz.kicad_pcb`: that is the superseded
four-layer bare-chip RP2354A design. Its reviews are historical, not approval
of this new carrier. The proven RP2040 breadboard is evidence of the system
concept, not evidence that this new PCB or RP2350 firmware works.

## Intended build and accepted limits

- 90 × 75 mm, two-layer, nominal 1.6 mm FR-4, 1 oz copper. All 32 electrical
  positions are through-hole/module interfaces; four board-only M3 mounts.
- Pimoroni PGA2350 (RP2350B), **assume A4 stepping**. A4 supplier confirmation
  and purchase are pending. Module includes regulator, crystal, flash and
  native USB series resistors. Firmware port is pending; no board-specific
  firmware has been flashed. No Raspberry Pi single-board computer is used.
- Native USB device toward Mac; Pico-PIO-USB host on GP0/GP1 through 22 Ω
  toward Moonlander. Header-mounted Soldered 333011 USB-C breakouts at J1/J2.
- Supported powered Anker data port, brightness cap 175. Direct shared 5 V,
  470 µF bulk C1, fixed CC resistors, **always-powered downstream VBUS**.
  The user explicitly accepted no attachment-controlled switch, input ramp,
  local electronic current limiter or reverse blocking for this prototype.
  Do not silently reintroduce that architecture or call this fully USB-C
  compliant. Downstream 1.5 A advertisement is not upstream entitlement.
- Stereo line inputs to separate biased/filter/clamp channels, RP2350B
  **GP40/GP41 ADC0/ADC1**; mono visualizer analysis in firmware. No codec.
- BSS138 module translates GP2/GP3 serial LCD signals between 3.3 V and 5 V.
  Encoder A/B/push GP4/GP5/GP8; GP6 status LED; module BS/RUN switches.
- Common ground pours; no analog-ground island. No optional expansion,
  VFD, wireless, extra supply, enclosure design or new visualization scope.

## Checks to challenge independently

1. Module pin mapping/rotation: manufacturer A1 is carrier lower-right.
   Confirm supplies, six grounds, native USB, ADC assignments, BS/RUN, LCD
   voltage domains, encoder and LED polarity. Do not infer RP2040 ADC pins.
2. USB breakout header mapping and both CC pins per port; no accidental
   data swap or missing flip support. Assess USB trace/return geometry and
   module/header transitions. All data traces are top-layer, no vias; the
   native pair is 0.8 mm wide with 0.2 mm edge spacing on long straights.
   No verified 90 Ω impedance or USB compliance is claimed. Dense PGA pad
   escapes interrupt some ground beneath host traces; see inspection report.
3. Main 5 V path branches at C1 and does not run through module VB. Check
   copper, header and ground-return practicality within the accepted power
   scope. State any actual new blocker with evidence, not hypothetical scope.
4. Audio bias isolation, coupling-cap polarity, filter topology, BAT85S clamp
   polarity/leakage/overdrive limitations and USB/power noise coupling. Earlier
   small-signal simulations do not certify hot-input or powered-off safety.
5. Actual hand-solderable footprints: jack SJ1-3533NG is **three terminals**,
   encoder is PEC12R-4120F-S0012, not arbitrary EC11. Check pad/drill geometry,
   polarity marks and socket frame practicality. U2 body registration, full
   PGA socket mating/cutting and some owned-kit lead fits remain provisional;
   user accepted the paper corner fit, not every manufacturing tolerance.
6. Four 3.2 mm unplated mounts at coordinates in MECHANICAL.md; maximum
   hardware diameter 7 mm, copper keepout diameter 7.3 mm on both layers.
   Check hardware/component/edge clearances, including H2 near host escapes.
7. Manufacturing: current filled zones, no missing mask openings, correct
   outline, eight total unplated holes (four USB clearances plus four mounts),
   two plated encoder slots and Gerber/drill registration. OSH Park quoted
   the earlier same-outline snapshot at $52.30/three bare boards; this is a
   pricing reference, not a manufacturing sign-off. CircuitHub is another
   bare-board quote path, not a factory-assembly decision.

Evidence: KiCad **10.0.6**, zero ERC findings, zero DRC violations/unconnected
items/schematic parity issues; independent 16-requirement netlist audit,
12 netlist tests, 10 placement checks and four routing-search tests. No new
rule exclusions. Check raw reports; clean rules do not prove function.

Primary sources already identified (verify relevant pin/dimensional claims):

- [PGA2350 schematic](https://cdn.shopify.com/s/files/1/0174/1800/files/Pimoroni_PGA2350_Schematic.pdf?v=1724926880),
  [pinout](https://cdn.shopify.com/s/files/1/0174/1800/files/pga2350_pinout_diagram.pdf?v=1723124465),
  [mechanical drawing](https://cdn.shopify.com/s/files/1/0174/1800/files/PGA2350-mechanical-diagram.pdf?v=1725452657).
- [Soldered 333011 hardware](https://github.com/SolderedElectronics/USB-C-female-connector-breakout-hardware-design), v1.2.0.
- [Same Sky jack](https://www.sameskydevices.com/product/resource/sj1-353xng.pdf),
  [Vishay BAT85S](https://www.vishay.com/docs/85513/bat85s.pdf).
- [RP2350 hardware guide](https://datasheets.raspberrypi.com/rp2350/hardware-design-with-rp2350.pdf),
  [OSH Park two-layer limits](https://docs.oshpark.com/services/two-layer/),
  [slot support](https://docs.oshpark.com/submitting-orders/slots/).

## Response format

Write `ASTRA_CARRIER_REVIEW.md`. Lead with blockers, then material concerns,
then minor corrections. Each finding needs severity, exact file/net/pin or
coordinate, failure mechanism, supporting evidence and smallest practical fix.
Distinguish confirmed error from unresolved assumption. Do not invent numbers
or guarantee success. End with a short go/no-go recommendation for **ordering
this prototype**, listing only unresolved items that materially affect it.
