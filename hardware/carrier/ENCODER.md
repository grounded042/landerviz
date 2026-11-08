# Carrier encoder selection and footprint — 2026-11-07

Selected replacement: **Same Sky ACZ11BR1E-20FD1-12C**, DigiKey **102-1760-ND**.
This replaces the unpurchased Bourns PEC12R-4120F-S0012 after the independent
review identified transposed shell slots and a hand-soldering restriction.
The original frozen review snapshot is preserved; it is not an order file.

## Sourcing and hand assembly

The replacement retains **12 detents, push switch and a 20 mm flat, 6 mm
diameter shaft**. A detent is the tactile click at each position. The part has
12 pulses per revolution; the two contact signals are read using the existing
quadrature-input circuit. No processor pin or visualization behavior changes.
Its metal threaded bushing is M7 × 0.75, 5 mm long. A bushing is the collar
around the shaft; case/knob fit will use this part's drawing, not the Bourns'.

The [DigiKey listing](https://www.digikey.com/en/products/detail/same-sky-formerly-cui-devices/ACZ11BR1E-20FD1-12C/1923355)
showed **$4.33 for one and 10,544 in stock** when checked November 7. This is
$2.56 above the old $1.77 candidate, before shipping, tax and any checkout
tariff. It stays in the consolidated DigiKey small-parts order. Nothing bought.
Check that checkout is fulfilled by DigiKey rather than adding a marketplace
shipment; live stock and final price can change.

The manufacturer's [ACZ11 datasheet](https://www.sameskydevices.com/product/resource/acz11.pdf),
revision 1.08 dated **2026-05-07**, explicitly specifies **350 °C maximum,
three seconds maximum for hand soldering** (page 1). These are maximum limits,
not permission to heat repeatedly without cooling. Use a temperature-controlled
iron, prepare joints beforehand, and let the part cool between attempts. Do
not force the body against a mismatched pattern or use the iron to push it in.
Check rotation contacts and push switch after assembly. The purchased part has
not yet been physically fitted or soldered; a datasheet match is not that test.

## Independent footprint facts

Page 4's **vertical / with-switch / top-view** drawing is the source, not a
generic EC11 or Bourns library model. Coordinates below are relative to shaft
centre, X rightward and Y toward the three rotation contacts:

| Terminal | X, mm | Y, mm | Carrier signal / drill |
|---|---:|---:|---|
| A | −2.5 | +7.5 | GP4 / 1.2 mm |
| C, common | 0 | +7.5 | GND / 1.2 mm |
| B | +2.5 | +7.5 | GP5 / 1.2 mm |
| D (carrier pad S1) | −2.5 | −7.0 | GP8 push / 1.2 mm |
| E (carrier pad S2) | +2.5 | −7.0 | GND / 1.2 mm |
| Shell tab | −4.7 | 0 | Unconnected plated mounting hole, 3.2 mm |
| Shell tab | +4.7 | 0 | Unconnected plated mounting hole, 3.2 mm |

The five electrical-hole centres remain at the previous carrier coordinates.
Their holes increase from 1.0 to the manufacturer's recommended **1.2 mm**;
their copper pads are **1.8 mm** diameter. Shell centres change from ±6.6 to
**±4.7 mm**, matching the new part's 9.4 mm mounting-centre spacing. Shaft
centre stays at carrier **(77,55) mm**; no other component moves. Body drawing
is 11.7 mm across, extending −7.25 to +6.5 mm along Y. The existing larger
16.8 × 16.6 mm courtyard (reserved component-clearance envelope) is retained.

## Deliberate mounting-hole substitution

The manufacturer recommends **1.8 × 2.1 mm rectangular shell openings**.
The carrier uses **3.2 mm round plated holes with 4.2 mm copper pads**, rather
than silently treating a small rounded slot as that rectangle. This is our
hand-assembly choice, not the manufacturer's specified opening shape.

The recommended rectangle's corner radius from its centre is
`sqrt(0.9² + 1.05²) = 1.383 mm`; the round hole's nominal radius is 1.600 mm.
Thus the entire recommended rectangle fits within the nominal round hole.
As a manufacturing-margin comparison, OSH Park publishes ±0.0635 mm maximum
drill-size and 0.0508 mm maximum drill-position tolerances. Applying the size
reduction and conservatively shifting the corner by 0.0508 mm on each axis
leaves about **0.114 mm radial clearance**. This comparison assumes the
published size tolerance applies to the requested hole dimension; it is not
a tab-profile measurement or guarantee of every production tolerance.

The wider holes give less friction retention before soldering. Hold the part
seated and square while initially soldering; both metal tabs are soldered as
mechanical anchors, not relied upon as a snap fit. The 0.5 mm nominal copper
ring around each hole provides soldering area. No extra component or enclosure
mount is introduced. All holes in this carrier are now round: no encoder slots
remain in the updated drill export.

Source: [OSH Park two-layer drill limits](https://docs.oshpark.com/services/two-layer/).
The board's independent audit checks shell centres, round 3.2 mm holes and all
five electrical drills, with fault tests for wrong shell position, the old
transposed slot and undersized contact holes. Native KiCad checks still need
to pass after the changed footprint is routed and ground is refilled.
