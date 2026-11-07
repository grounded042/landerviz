# Project library variants

KiCad 10.0.6 library data is the source of the three footprints below and the
RP2354A symbol. The KiCad community's attribution and library license are in
`KICAD_LIBRARY_LICENSE.md`; its CC-BY-SA-4.0 terms and design exception apply
to this derived library data. This does not relicense the rest of LanderViz.

| Project item | Upstream library item | Changes |
|---|---|---|
| `USB_C_GCT_USB4105_LanderViz` | `Connector_USB:USB_C_Receptacle_GCT_USB4105-xx-A_16P_TopMnt_Horizontal` | A1/B12 and A12/B1 have a 0.30 mm chamfer on the single corner nearest the locating hole. Pad centers, bounding dimensions, other corners, connector body, and every hole are unchanged. |
| `QFN-60-1EP_7x7mm_P0.4mm_EP3.4x3.4mm_LanderViz` | `Package_DFN_QFN:QFN-60-1EP_7x7mm_P0.4mm_EP3.4x3.4mm` | Removed silkscreen that crossed solder openings. Copper, mask, paste, and fabrication drawing are unchanged. A board-level dot marks pin 1. |
| `Jack_3.5mm_CUI_SJ1-3523N_Horizontal_LanderViz` | `Connector_Audio:Jack_3.5mm_CUI_SJ1-3523N_Horizontal` | Removed silkscreen crossing the board notch. Pads, holes, fabrication drawing, and body model are unchanged. |
| `LanderViz:RP2354A` | reviewed embedded `MCU_RaspberryPi:RP2354A` | Moved the existing reviewed definition to the project library. No pin number, name, type, position, or connection changed. Default footprint points to the project variant. |

The USB chamfer removes a small area of the solder land, so it remains an
assembly-review item. It clears the project's 0.25 mm hole-to-copper rule
without moving the manufacturer's 0.65 mm locating holes or weakening the
clearance rule. GCT's drawing is the mechanical authority:
<https://gct.co/files/drawings/usb4105.pdf>.

Upstream library sources:

- <https://gitlab.com/kicad/libraries/kicad-footprints>
- <https://gitlab.com/kicad/libraries/kicad-symbols>
- <https://www.kicad.org/libraries/license/>

The other local footprints were built from their manufacturer land patterns:
TI DSS0012B, Abracon AOTA-B201610, and Bourns PEC12R configuration 4. Their
current source files preserve the board's silkscreen/ground-connection edits.
Independent review corrected the Abracon L1 fabrication dot to the pad-1
(`+1V1`) end, matching Raspberry Pi R4-S1. Pad numbering, land sizes and
electrical routing are unchanged. The former pad-2/LX marking was wrong.
Raspberry Pi reference-derived regulator placement is separately attributed
in `REFERENCE_LICENSE.txt`.

`LanderViz:TPS22953DQC` is a new project symbol drawn from TI's pin table, not
a modified standard-library symbol. The physical PG output is open drain;
its unused pin is typed passive solely to permit TI's required direct GND tie
without a false conflict with the ground power flag. This tie is audited.
No other output type or project ERC rule was changed. Its standard
`Package_SON:WSON-10-1EP_2x3mm_P0.5mm_EP0.84x2.4mm` footprint is unmodified;
the ground vias are outside the solder pad.
