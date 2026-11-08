# Carrier Rev A mechanics — 2026-11-07

The outline remains **90 × 75 mm**, with all 32 electrical component positions
unchanged. U2's pad names/nets are reversed along both rows to match the
user-confirmed physical converter; its physical hole grid is unchanged. After Astra review, SW3's shell-hole positions and electrical
drill sizes change for the replacement encoder; see ENCODER.md. Four dedicated mounting holes are now added,
separate from the four clearance holes already underneath the USB modules.
No enclosure design or larger board is introduced.

Coordinates below are millimetres from the upper-left corner, looking down at
the component side; X increases rightward and Y downward.

| Hole | X | Y | Drill |
|---|---:|---:|---:|
| H1 | 27.25 | 12.00 | 3.20 mm, unplated |
| H2 | 63.00 | 11.75 | 3.20 mm, unplated |
| H3 | 24.50 | 58.50 | 3.20 mm, unplated |
| H4 | 86.00 | 71.00 | 3.20 mm, unplated |

M3 denotes a screw with a nominal 3 mm thread diameter. The larger 3.2 mm drill
provides insertion clearance. These are clearance holes, not threaded holes.
They form an asymmetric mounting pattern to use space around the existing
components. A future case should follow these coordinates rather than assume
a rectangular corner pattern.

**Maximum outside diameter of screw heads, spacers and any washers: 7.0 mm.**
The component-clearance circles reserve that diameter on both sides. Separate
7.3 mm diameter rule areas forbid traces, vias and copper fills on both copper
layers. This keeps conductive mounting hardware away from copper. Do not use
larger washers or spacers without checking the layout. Spacer height, screw
length and case clearance remain future mechanical choices; keep underside
solder joints clear. The USB module clearance holes are not substitutes for
these four carrier mounts.

The native USB pair and one host escape were rerouted around mounting
hardware. Electrical component positions and the accepted direct-power
arrangement are unchanged. Both USB paths still use top copper without vias.

Front and back printed markings read **LANDERVIZ / CARRIER REV A / 2026-11**.
Back lettering is mirrored in KiCad's through-the-board view so it reads
normally when physically viewing the underside. BOOT, RESET and LCD labels
were moved clear of pads and mounting holes. Copper-only supplier previews do
not show these printed markings. `mechanical.py` regenerates the owned holes,
rule areas and text; `placement.py` applies it when generating a new placement.

The saved routed board passes KiCad 10.0.6 checks with zero violations,
unconnected items and schematic mismatches. The independent placement audit
also checks the mounting coordinates, drill sizes, unplated attributes,
assembly exclusions, hardware rule areas and solder-mask openings. Checks do
not establish socket mating depth, actual kit-part lead fit or enclosure fit.

The current review exports are in `../fabrication/carrier-followup-20261107/`.
`assembly.pdf` shows component outlines and holes; `fit/carrier-fit-template.pdf`
is an actual-size paper reference. The earlier paper fit was accepted by the
user; this update does not change those electrical hole locations.
