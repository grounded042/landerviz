# Focused LanderViz carrier follow-up

Review the corrected carrier after `ASTRA_CARRIER_REVIEW.md`. Be concise;
prioritize new blockers and closure of original findings. Do not modify files
or redesign accepted scope. Write `ASTRA_CARRIER_FOLLOWUP.md`.

Current packet: `hardware/fabrication/carrier-followup-20261107/`;
shareable bundle `landerviz-carrier-FOLLOWUP-REVIEW.zip`. Use `source/`,
`checks/`, `manifest.json`, `assembly.pdf`, `schematic.pdf` and
`context/REVIEW_RESOLUTION.md`. Earlier snapshots are superseded for ordering.

Check these deltas:

1. SW3 is now **Same Sky ACZ11BR1E-20FD1-12C**, 12 detents/push, available
   through the consolidated DigiKey order. Its May 7, 2026 ACZ11 datasheet
   explicitly permits hand soldering within 350 °C / three-second limits.
   Verify the page-4 vertical/with-switch pin pattern, five 1.2 mm contact
   drills and shell centres at ±4.7 mm. Shells deliberately use **3.2 mm
   round plated holes / 4.2 mm pads**, enclosing the recommended 1.8 × 2.1 mm
   rectangles. Assess the geometry/tolerance/retention rationale in ENCODER.md;
   it is our substitution, not claimed manufacturer-endorsed shape equivalence.
2. **Actual converter observation:** labelled side up, HV left; top-to-bottom
   `HV4 HV3 GND HV HV2 HV1`, right `LV4 LV3 GND LV LV2 LV1`. Both rows were
   explicitly confirmed by the user. Earlier assumed order was reversed.
   Verify corrected U2 ground/supply/channel coordinates and routed nets.
   Grounds must be third and supplies fourth, not vice versa. HV4/LV4 top
   orientation marks are added. Independent audit checks all twelve pads.
3. Capacitor fit is user-confirmed. The user accepts verified processor
   manufacturer dimensions plus earlier paper fit for ordering; full socket
   construction/mating remains assembly work, not claimed physically tested.
   They will ensure A4 silicon before module purchase. Assess closure on that
   explicitly accepted prototype basis; do not demand those same measurements
   again without identifying a new material discrepancy.
4. Stale schematic “unrouted” descriptions are corrected. Audio power sequence
   and clipping/headroom limits are documented without claiming unpowered
   isolation. Main power architecture, outline, MCU and brightness cap remain
   unchanged. No automatic downstream attachment/power control was added.

Recheck changed copper/exports for material regression; don't repeat the entire
original circuit narrative. Primary encoder source:
https://www.sameskydevices.com/product/resource/acz11.pdf.

Report each remaining issue with severity, exact net/pad/location, evidence
and smallest fix. Distinguish design errors, accepted prototype risks and
future bring-up checks. End with go/no-go for ordering the OSH Park bare-board
prototype, without promising functional success.
