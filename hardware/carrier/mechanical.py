"""Carrier mounting hardware and revision markings; installed pcbnew only.

Coordinates are millimetres relative to the upper-left board corner. Dedicated
mounts are independent of the four holes under the two USB breakout modules.
"""
import math
from pathlib import Path

import pcbnew as pcb

HERE = Path(__file__).resolve().parent
MOUNTS = {'H1': (27.25, 12), 'H2': (63, 11.75),
          'H3': (24.5, 58.5), 'H4': (86, 71)}
KEEP_OUT_RADIUS = 3.65
FOOTPRINT = 'MountingHole_3.2mm_M3_7mmHardware'
# Keep removed SWIG wrappers alive through SaveBoard. In KiCad 10.0.6 macOS,
# collecting a detached PCB_TEXT can invalidate subsequent wrapper type lookup.
_DETACHED = []


def point(x, y):
    return pcb.VECTOR2I(pcb.FromMM(x + 50), pcb.FromMM(y + 50))


def write_library():
    lib = HERE / 'carrier.pretty'
    lib.mkdir(exist_ok=True)
    (lib / (FOOTPRINT + '.kicad_mod')).write_text('''(footprint "MountingHole_3.2mm_M3_7mmHardware"
      (version 20240108) (generator "pcbnew") (layer "F.Cu")
      (descr "3.2mm unplated M3 hole; maximum screw head/spacer/washer diameter 7mm")
      (attr board_only exclude_from_pos_files exclude_from_bom)
      (property "Reference" "REF**" (at 0 0) (layer "F.Fab")
        (effects (font (size .8 .8) (thickness .12))))
      (property "Value" "M3 / max OD 7mm" (at 0 4.5) (layer "F.Fab")
        (effects (font (size .8 .8) (thickness .12)) hide))
      (fp_circle (center 0 0) (end 3.5 0) (stroke (width .1) (type default))
        (fill none) (layer "F.Fab"))
      (fp_circle (center 0 0) (end 3.5 0) (stroke (width .05) (type default))
        (fill none) (layer "F.CrtYd"))
      (fp_circle (center 0 0) (end 3.5 0) (stroke (width .05) (type default))
        (fill none) (layer "B.CrtYd"))
      (pad "" np_thru_hole circle (at 0 0) (size 3.2 3.2) (drill 3.2)
        (layers "*.Cu" "*.Mask")))
''')


def apply(board):
    """Idempotently update only owned mechanical objects and branding."""
    write_library()
    for fp in list(board.GetFootprints()):
        if fp.GetReference() in MOUNTS:
            board.Remove(fp)
            _DETACHED.append(fp)
    for zone in list(board.Zones()):
        if zone.GetZoneName() in {'M3 clearance ' + ref for ref in MOUNTS}:
            board.Remove(zone)
            _DETACHED.append(zone)
    owned = {'LANDERVIZ', 'CARRIER / DRAFT', 'CARRIER REV A', '2026-11', 'BOOT', 'RESET'}
    for drawing in list(board.GetDrawings()):
        if isinstance(drawing, pcb.PCB_TEXT) and drawing.GetText() in owned:
            board.Remove(drawing)
            _DETACHED.append(drawing)
    for ref, (x, y) in MOUNTS.items():
        # Build the simple mechanical footprint directly. Avoid mixing the
        # footprint plugin and board loader, which is unstable in this macOS
        # pcbnew build. The matching library file is generated above.
        fp = pcb.FOOTPRINT(board)
        fp.SetFPIDAsString('Carrier:' + FOOTPRINT)
        fp.SetLibDescription('3.2mm unplated M3 hole; maximum screw head/spacer/washer diameter 7mm')
        fp.SetReference(ref)
        fp.SetValue('M3 / max OD 7mm')
        fp.Reference().SetLayer(pcb.F_Fab)
        fp.Reference().SetTextSize(pcb.VECTOR2I(pcb.FromMM(.8), pcb.FromMM(.8)))
        fp.Reference().SetTextThickness(pcb.FromMM(.12))
        fp.Value().SetLayer(pcb.F_Fab)
        fp.Value().SetVisible(False)
        pad = pcb.PAD(fp)
        pad.SetShape(pcb.PAD_SHAPE_CIRCLE)
        pad.SetAttribute(pcb.PAD_ATTRIB_NPTH)
        pad.SetSize(pcb.VECTOR2I(pcb.FromMM(3.2), pcb.FromMM(3.2)))
        pad.SetDrillSize(pcb.VECTOR2I(pcb.FromMM(3.2), pcb.FromMM(3.2)))
        # AllCuMask() exposes a shared native constant: copy before adding
        # mask layers, otherwise SaveBoard's wildcard serialization changes.
        pad_layers = pcb.LSET(pcb.LSET.AllCuMask())
        pad_layers.AddLayer(pcb.F_Mask)
        pad_layers.AddLayer(pcb.B_Mask)
        pad.SetLayerSet(pad_layers)
        fp.Add(pad)
        for layer, width in ((pcb.F_Fab, .1), (pcb.F_CrtYd, .05), (pcb.B_CrtYd, .05)):
            circle = pcb.PCB_SHAPE(fp)
            circle.SetShape(pcb.SHAPE_T_CIRCLE)
            circle.SetCenter(pcb.VECTOR2I(0, 0))
            circle.SetEnd(pcb.VECTOR2I(pcb.FromMM(3.5), 0))
            circle.SetLayer(layer)
            circle.SetWidth(pcb.FromMM(width))
            fp.Add(circle)
        fp.SetAttributes(pcb.FP_BOARD_ONLY | pcb.FP_EXCLUDE_FROM_BOM | pcb.FP_EXCLUDE_FROM_POS_FILES)
        board.Add(fp)
        fp.SetPosition(point(x, y))
        zone = pcb.ZONE(board)
        zone.SetIsRuleArea(True)
        zone.SetZoneName('M3 clearance ' + ref)
        layers = pcb.LSET()
        layers.AddLayer(pcb.F_Cu)
        layers.AddLayer(pcb.B_Cu)
        zone.SetLayerSet(layers)
        zone.SetDoNotAllowTracks(True)
        zone.SetDoNotAllowVias(True)
        zone.SetDoNotAllowZoneFills(True)
        zone.SetDoNotAllowPads(False)  # Own unplated drill must be allowed.
        zone.SetDoNotAllowFootprints(False)
        outline = zone.Outline()
        outline.NewOutline()
        for i in range(96):
            angle = i * 2 * math.pi / 96
            p = point(x + KEEP_OUT_RADIUS * math.cos(angle),
                      y + KEEP_OUT_RADIUS * math.sin(angle))
            outline.Append(p.x, p.y)
        board.Add(zone)
    for value, x, y, size, layer in [
        ('LANDERVIZ', 69, 68.5, 1.8, pcb.F_SilkS),
        ('CARRIER REV A', 69, 71, .9, pcb.F_SilkS),
        ('2026-11', 69, 73, .8, pcb.F_SilkS),
        ('LANDERVIZ', 45, 21.5, 1.8, pcb.B_SilkS),
        ('CARRIER REV A', 45, 24.5, .9, pcb.B_SilkS),
        ('2026-11', 45, 27, .8, pcb.B_SilkS),
        ('RESET', 22.3, 4.5, .8, pcb.F_SilkS),
        ('BOOT', 67.8, 4.5, .8, pcb.F_SilkS),
    ]:
        text = pcb.PCB_TEXT(board)
        text.SetText(value)
        text.SetPosition(point(x, y))
        text.SetTextSize(pcb.VECTOR2I(pcb.FromMM(size), pcb.FromMM(size)))
        text.SetTextThickness(pcb.FromMM(.12))
        text.SetLayer(layer)
        text.SetMirrored(layer == pcb.B_SilkS)
        if value in ('RESET', 'BOOT'):
            text.SetTextAngle(pcb.EDA_ANGLE(90, pcb.DEGREES_T))
        board.Add(text)
    for fp in board.GetFootprints():
        if fp.GetReference() in ('SW1', 'SW2'):
            fp.Reference().SetLayer(pcb.F_Fab)
            fp.Reference().SetPosition(point(62.75 if fp.GetReference() == 'SW1' else 27.25, 4.25))
    for drawing in board.GetDrawings():
        if isinstance(drawing, pcb.PCB_TEXT) and drawing.GetText() == 'LCD':
            drawing.SetPosition(point(28.6, 62.3))


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('board', type=Path)
    args = parser.parse_args()
    board = pcb.LoadBoard(str(args.board))
    apply(board)
    pcb.SaveBoard(str(args.board), board)
