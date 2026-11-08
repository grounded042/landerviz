"""Export a 1:1 paper fit aid from the saved carrier; never modify its source.

Run with KiCad's bundled Python. Uses pcbnew and the installed kicad-cli only.
The drawing checks space and hole locations, not mating depth or electrical fit.
"""
import argparse
from pathlib import Path
import re
import subprocess

import pcbnew as pcb

HERE = Path(__file__).resolve().parent
CLI = Path('/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli')


def point(x, y):
    return pcb.VECTOR2I(pcb.FromMM(x), pcb.FromMM(y))


def text(board, value, x, y, size=1.7):
    item = pcb.PCB_TEXT(board)
    item.SetText(value)
    item.SetPosition(point(x, y))
    item.SetTextSize(point(size, size))
    item.SetTextThickness(pcb.FromMM(.2))
    item.SetHorizJustify(pcb.GR_TEXT_H_ALIGN_LEFT)
    item.SetLayer(pcb.Dwgs_User)
    board.Add(item)


def line(board, a, b):
    item = pcb.PCB_SHAPE()
    item.SetShape(pcb.SHAPE_T_SEGMENT)
    item.SetStart(point(*a))
    item.SetEnd(point(*b))
    item.SetWidth(pcb.FromMM(.2))
    item.SetLayer(pcb.Dwgs_User)
    board.Add(item)


def prepare(source, directory):
    snapshot = directory / 'carrier-fit-template.kicad_pcb'
    if source.resolve() == snapshot.resolve():
        raise ValueError('Fit-template output must differ from the source board')
    directory.mkdir(parents=True, exist_ok=True)
    board = pcb.LoadBoard(str(source))
    # Remove only drafting notes on the disposable copy. Footprints, board
    # outline and pad coordinates stay exactly where they are in the source.
    detached=[]  # Retain removed SWIG wrappers until SaveBoard finishes.
    for item in list(board.GetDrawings()):
        if item.GetLayer() == pcb.Dwgs_User:
            board.Remove(item)
            detached.append(item)
    text(board, 'LANDERVIZ / CARRIER FIT CHECK / TOP VIEW', 20, 18, 2.6)
    text(board, 'Print at 100% or Actual Size. Disable Fit to Page.', 20, 27)
    text(board, 'Check the ruler below before using the drawing. Board: 90 x 75 mm.', 20, 33)
    text(board, 'Module outlines are envelopes; some reused-part dimensions remain provisional.', 20, 39, 1.5)
    text(board, 'PGA2350', 88, 69, 1.3)
    text(board, 'Top view', 88, 72, 1.1)
    text(board, 'A1: lower-right', 86, 75, .95)

    # 25.4 mm is exactly one inch. Place the ruler outside the board so the
    # user can check printer scaling with an ordinary inch ruler.
    text(board, 'THIS LINE MUST MEASURE EXACTLY 1 INCH', 20, 142, 1.7)
    line(board, (20, 149), (45.4, 149))
    for x in (20, 45.4):
        line(board, (x, 146.5), (x, 151.5))
    text(board, '1 inch = 25.4 mm', 20, 157, 1.5)

    notes = [
        '1. Check the one-inch ruler first. A scaled print cannot establish fit.',
        '2. At the printed PGA top-left, socket A covers the top 2 rows and left 3 holes.',
        '3. Socket B covers the left 2 columns and next 3 rows BELOW A. No holes are shared.',
        '4. Check their plastic edges for overlap. Keep long strips uncut until the frame is confirmed.',
        '5. Compare the unpowered level converter with U2: rows, labels and body position.',
        '6. Spare capacitors can check lead fit; leave the working breadboard assembled.',
        'Paper cannot check socket contact depth, stacking height or manufacturing tolerance.',
        'Do not force parts or use this as a drilling guide. NOT A FABRICATION RELEASE.',
    ]
    for i, note in enumerate(notes):
        text(board, note, 20, 171 + i * 7, 1.55)

    output = directory / 'carrier-fit-template.pdf'
    pcb.SaveBoard(str(snapshot), board)
    # pcbnew exposes PAGE_INFO as an opaque pointer in this KiCad build.
    # Set the paper token only in the disposable snapshot, never the source.
    content, count = re.subn(r'\(paper [^)]*\)', '(paper "USLetter" portrait)',
                             snapshot.read_text(), count=1)
    if count != 1:
        raise RuntimeError('Snapshot has no paper definition')
    snapshot.write_text(content)
    subprocess.run([
        str(CLI), 'pcb', 'export', 'pdf', '--mode-single', '--black-and-white',
        '--scale', '1', '--layers', 'F.Fab,F.SilkS,Edge.Cuts,Dwgs.User',
        '--sketch-pads-on-fab-layers', '--exclude-value', '--output', str(output),
        str(snapshot),
    ], check=True)
    print(output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=HERE / 'landerviz-carrier.kicad_pcb')
    parser.add_argument('--output-dir', type=Path,
                        default=HERE.parent / 'fabrication' / 'carrier-fit-20261106')
    args = parser.parse_args()
    prepare(args.source, args.output_dir)
