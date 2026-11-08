# Carrier Rev A — ordered manufacturing release

The user ordered these bare boards from OSH Park on November 7, 2026.
The ZIP is byte-for-byte identical to the package accepted by the independent
Astra follow-up and supplied for ordering. Keep it unchanged.

- [Manufacturing ZIP](landerviz-carrier-RevA-gerbers.zip)
- [Release manifest](release.json)
- [Order status](../../ORDER_STATUS.md)
- [Review resolutions](../../REVIEW_RESOLUTION.md)

Settings: 90 × 75 mm, two copper layers, nominal 1.6 mm FR-4 and 1 oz copper.
No component assembly or stencil. Seven Gerber layers and two Excellon drill
files; 182 plated drill features and eight unplated holes, all round.
Color, finish, quantity and shipping are order settings outside the Gerbers.

Board SHA-256:
`73a8fa9c8e2926b336ab9a96fe3c684fc772ab46f2bac94f24df9b723bde404f`

ZIP SHA-256:
`6efba88ea4c479ada24e96dabca6f6cfd9fdf925c13644ab10652f613084da2b`

Recorded checks: KiCad 10.0.6 electrical and design rules report zero findings,
including unconnected items and schematic parity; 16 independent netlist
requirements and 30 carrier tests pass. The follow-up independently confirmed
the board hash and design-rule result. Full local review snapshots remain
under ignored `hardware/fabrication/`; raw reviewer exchanges are not included.
These checks establish the prototype order basis. Assembly, the PGA2350
firmware port and functional bring-up remain to be completed.
