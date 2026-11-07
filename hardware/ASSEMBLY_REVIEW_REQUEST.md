# JLCPCB assembly review request — draft, not sent

Please assess these features before we order a small prototype assembly.
Board: LanderViz, 48 × 42 mm, four layers, JLC04161H-3313, nominal 1.6 mm,
blue solder mask / white silkscreen. Surface finish and any special via process
are not yet approved. All purchased components are on top; 70 populated
positions. R22/R23 are not populated. Bare test pads and mounting holes are
not assembly components.

Attach corrected draft Gerbers, separate plated/unplated drill files,
`board-layers.pdf`, component list and native placement CSV from
`fabrication/marking-20261105/`. These are review files, not an
authorization to manufacture. Do not use the superseded `astra-review` or
`review-corrections-20261104`, `input-power-20261104` or
`followup-fixes-20261104` files.

Coordinates below use the native KiCad origin, in millimetres.

| Feature | Geometry / question |
|---|---|
| U1 RP2354A exposed ground pad | 3.4 × 3.4 mm pad; four 0.30 mm drilled ground vias with 0.60 mm lands at (118.2,116.2), (119.8,116.2), (118.2,117.8), (119.8,117.8). Open top solder-mask region; holes intersect the paste-window area. Bottom tenting does not fill their barrels. Can you accept the existing stencil and unfilled-hole process? If not, quote resin fill and copper cap for these specific holes, or provide the required stencil/land changes. |
| U2 TPS25821DSSR exposed ground pad | 1.00 × 2.65 mm pad; three 0.20 mm drilled plated thermal holes, 0.45 mm lands, at (132,111.925), (132,113), (132,114.075). Two 0.36 × 2.30 mm paste strips centred x=131.69 and 132.31 give approximately 62.5% nominal coverage. Does this process provide an acceptable ground/thermal joint? Please assess the actual pattern rather than assuming these are ordinary lead holes or filled vias. Quote the required alternative if necessary. |
| U6 TPS22953DQCR | DQC0010A, 0.84 × 2.4 mm exposed solder pad centred at (115.05,102), rotated 180°. Two 0.68 × 0.97 mm paste windows give **65.4% nominal exposed-pad coverage**, versus approximately 86% in TI's example. No drilled holes or via lands overlap this component's paste. Confirm acceptance of this exact stencil pattern, assembly process and orientation; specify required local changes if necessary. Do not substitute TPS22954. |
| J1/J2 USB-C ground lands | The 0.60 × 1.15 mm A1/B12 and A12/B1 lands each have a single 0.30 mm corner chamfer beside a 0.65 mm locating hole. Land centres and bounding sizes are unchanged. Confirm solder-joint and assembly acceptance. |
| J3 audio-jack slots and minimum drill | Confirm the plated 0.40 mm-wide slots and the board's 0.20 mm drill option, including any surcharge. |
| L1 marked inductor | Abracon AOTA-B201610S3R3-101-T. **Physical dot must face the +1V1 output/pad-1 end, left on this board**, matching the F.Fab dot at (120.35,109.20). Pad 1 centre (120,109.65); pad 2/LX centre (122,109.65). Confirm tape/pick-and-place orientation visually; a zero-degree CSV entry is not sufficient approval. |
| U1 silicon and regulator capacitors | Confirm RP2354A A4 stepping, or ask before substituting a later stepping. C6/C7/C9/C10 must be Murata GRM155R60J475ME47D (C82453); no nominal-value-only substitution. Confirm available assembly stock and quote. |
| Through-hole parts | Confirm assembly of the exact audio jack, 12-detent push encoder and outward-facing JST-PH LCD header, including any manual operations. |

Please return the accepted stencil/via process, surface-finish recommendation,
any additional charges, and the assembly orientation preview before production.
Do not silently enlarge holes, omit thermal holes, substitute regulator parts,
change the stack, or select a paid filled-via process. We will approve a concrete
quote/process after reviewing it.

Factory acceptance is still outstanding. No message, upload or order has been
sent by the coding agent.
