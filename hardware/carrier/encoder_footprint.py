"""Same Sky ACZ11BR1E-20FD1-12C footprint from the May 2026 drawing.

Standard library + installed KiCad only. See ENCODER.md for the deliberate
round-hole substitution for the manufacturer's rectangular shell openings.
"""
from pathlib import Path

HERE = Path(__file__).resolve().parent
NAME = 'Encoder_SameSky_ACZ11BR1E_Vertical'
MPN = 'ACZ11BR1E-20FD1-12C'
DATASHEET = 'https://www.sameskydevices.com/product/resource/acz11.pdf'


def write_library():
    body = []
    for layer in ('F.Fab',):
        # Shaft at origin; body extends 7.25 mm toward switch and 6.5 mm
        # toward quadrature contacts. Existing larger courtyard is retained.
        body.append(f'''(fp_rect (start -5.85 -7.25) (end 5.85 6.5)
          (stroke (width .12) (type default)) (fill none) (layer "{layer}"))''')
    # Break the printed outline around electrical and shell pad openings.
    for a, b in [((-5.97,-7.37),(-3.6,-7.37)),((-1.4,-7.37),(1.4,-7.37)),
                 ((3.6,-7.37),(5.97,-7.37)),((-5.97,6.62),(-3.6,6.62)),
                 ((3.6,6.62),(5.97,6.62)),((-5.97,-7.37),(-5.97,-2.5)),
                 ((-5.97,2.5),(-5.97,6.62)),((5.97,-7.37),(5.97,-2.5)),
                 ((5.97,2.5),(5.97,6.62))]:
        body.append(f'''(fp_line (start {a[0]} {a[1]}) (end {b[0]} {b[1]})
          (stroke (width .12) (type default)) (layer "F.SilkS"))''')
    body.extend(['''(fp_circle (center 0 0) (end 3.5 0)
        (stroke (width .1) (type default)) (fill none) (layer "F.Fab"))''',
        '''(fp_rect (start -8.4 -8.05) (end 8.4 8.55)
        (stroke (width .05) (type default)) (fill none) (layer "F.CrtYd"))'''])
    for number, x, y in [('A', -2.5, 7.5), ('C', 0, 7.5), ('B', 2.5, 7.5),
                         ('S1', -2.5, -7), ('S2', 2.5, -7)]:
        body.append(f'''(pad "{number}" thru_hole circle (at {x} {y})
          (size 1.8 1.8) (drill 1.2) (layers "*.Cu" "*.Mask"))''')
    for x in (-4.7, 4.7):
        body.append(f'''(pad "SH" thru_hole circle (at {x} 0)
          (size 4.2 4.2) (drill 3.2) (layers "*.Cu" "*.Mask"))''')
    content = f'''(footprint "{NAME}"
      (version 20240108) (generator "pcbnew") (layer "F.Cu")
      (descr "Same Sky {MPN}; 12 detents and push; round shell clearance per ENCODER.md")
      (tags "encoder SameSky ACZ11 vertical push 12 detents") (attr through_hole)
      (property "Reference" "REF**" (at 0 -9.25) (layer "F.SilkS")
        (effects (font (size 1 1) (thickness .15))))
      (property "Value" "{MPN}" (at 0 9.75) (layer "F.Fab")
        (effects (font (size 1 1) (thickness .15))))
      (property "Datasheet" "{DATASHEET}" (at 0 0) (layer "F.Fab")
        (effects (font (size 1 1) (thickness .15)) hide))
      {''.join(body)})
'''
    path = HERE / 'carrier.pretty' / (NAME + '.kicad_mod')
    path.parent.mkdir(exist_ok=True)
    path.write_text(content)


def replace_in_board(path):
    import pcbnew as pcb
    write_library()
    replacement = pcb.FootprintLoad(str(HERE / 'carrier.pretty'), NAME)
    board = pcb.LoadBoard(str(path))
    old = next(fp for fp in board.GetFootprints() if fp.GetReference() == 'SW3')
    # Preserve schematic identity and the shaft position. The changed shell
    # pads and wider electrical holes require independent refill/DRC/routing.
    replacement.SetFPIDAsString('Carrier:' + NAME)
    replacement.SetReference('SW3')
    replacement.SetValue(MPN)
    replacement.SetPath(old.GetPath())
    replacement.SetPosition(old.GetPosition())
    replacement.SetOrientationDegrees(old.GetOrientationDegrees())
    replacement.Reference().SetPosition(old.Reference().GetPosition())
    replacement.Reference().SetTextSize(old.Reference().GetTextSize())
    replacement.Reference().SetTextThickness(old.Reference().GetTextThickness())
    replacement.Value().SetVisible(False)
    replacement.SetField('Manufacturer', 'Same Sky')
    replacement.SetField('MPN', MPN)
    replacement.SetField('Assembly', 'Hand solder; physical mounting pending review')
    replacement.GetField('Manufacturer').SetVisible(False)
    replacement.GetField('MPN').SetVisible(False)
    replacement.GetField('Assembly').SetVisible(False)
    nets = {pad.GetNumber(): pad.GetNet() for pad in old.Pads() if pad.GetNumber() != 'SH'}
    for pad in replacement.Pads():
        if pad.GetNumber() in nets:
            pad.SetNet(nets[pad.GetNumber()])
    board.Remove(old)  # Keep its wrapper alive through SaveBoard (macOS SWIG).
    board.Add(replacement)
    pcb.SaveBoard(str(path), board)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board', type=Path)
    args = parser.parse_args()
    write_library()
    if args.board:
        replace_in_board(args.board)
