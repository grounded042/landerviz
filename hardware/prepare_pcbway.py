"""Build a PCBWay quote package from a frozen, checked review snapshot.

Standard library only; no uploads, PCB edits or vendor rotation guesses.
Reuses the existing snapshot/hash/coordinate validation before converting.
"""

import argparse
from collections import defaultdict
import contextlib
from decimal import Decimal
import io
import json
from pathlib import Path
import shutil
import tempfile
import zipfile

from prepare_jlc import digest, prepare, rows, write_csv

HW = Path(__file__).resolve().parent


def assembly_type(component):
    pads = [p for p in component["pads"] if p["number"]]
    drilled = [bool(p["drill"][0]) for p in pads]
    if not pads:
        raise ValueError(f"No numbered pads: {component['reference']}")
    return "Hybrid" if any(drilled) and not all(drilled) else "thru-hole" if all(drilled) else "SMD"


def grouped_bom(parts, components):
    groups = defaultdict(list)
    for part in parts:
        if not part["MPN"] or part["DNP"]:
            continue
        key = (part["Manufacturer"], part["MPN"], part["Footprint"],
               assembly_type(components[part["Reference"]]))
        groups[key].append(part)
    result = []
    for (manufacturer, mpn, footprint, kind), members in groups.items():
        refs = [p["Reference"] for p in members]
        note = "Quantity per board. Exact MPN; ask before substitution."
        if "U1" in refs:
            note += " RP2354A A4 stepping required; later stepping only by explicit approval."
        if "L1" in refs:
            note += " Physical dot toward left-hand +1V1/pad 1; see assembly instructions."
        if set(refs) & {"C6", "C7", "C9", "C10"}:
            note += " Qualified regulator capacitor: no nominal-value-only replacement."
        result.append([len(result) + 1, ",".join(refs), len(refs), manufacturer, mpn,
                       members[0]["Value"], footprint.split(":")[-1], kind, note])
    return result


def placement_rows(positions, components):
    all_parts, surface = [], []
    for part in positions:
        if part["Side"] != "top":
            raise ValueError("Unexpected bottom assembly")
        angle = (Decimal(part["Rot"]) % 360 + 360) % 360
        row = [part["Ref"], part["Package"], f"{Decimal(part['PosX']):.6f}mm",
               f"{Decimal(part['PosY']):.6f}mm", "T", f"{angle:.6f}", part["Val"]]
        all_parts.append(row)
        if assembly_type(components[part["Ref"]]) != "thru-hole":
            surface.append(row)
    return surface, all_parts


def build(snapshot, output):
    if output.exists():
        raise ValueError(f"Refusing to overwrite {output}")
    # Validate the exact frozen artifacts, ERC/DRC, references, placements,
    # Gerber coordinates and drills using the established exporter.
    with tempfile.TemporaryDirectory(prefix="landerviz-pcbway-") as temp:
        checked = Path(temp) / "checked"
        with contextlib.redirect_stdout(io.StringIO()):
            prepare(snapshot, checked)
        audit = json.loads((snapshot / "audit.json").read_text())
        components = {c["reference"]: c for c in audit["components"]}
        bom = grouped_bom(rows(snapshot / "components.csv"), components)
        surface, all_parts = placement_rows(rows(snapshot / "positions-populated.csv"), components)
        expected = {r[0] for r in all_parts}
        supplied = [ref for group in bom for ref in group[1].split(",")]
        if len(supplied) != len(expected) or set(supplied) != expected:
            raise ValueError("Grouped BOM/placement reference mismatch")
        output.mkdir(parents=True)
        shutil.copyfile(checked / "landerviz-gerbers.zip", output / "landerviz-gerbers.zip")
        with zipfile.ZipFile(output / "landerviz-gerbers.zip", "a", zipfile.ZIP_DEFLATED) as archive:
            archive.write(HW / "PCBWAY_FABRICATION_NOTES.md", "FABRICATION_NOTES.md")
        write_csv(output / "landerviz-pcbway-bom.csv",
                  ("Item #", "Designator", "Qty", "Manufacturer", "Mfg Part #",
                   "Description / Value", "Package/Footprint", "Type", "Your Instructions / Notes"), bom)
        columns = ("Designator", "Footprint", "Mid X", "Mid Y", "TB", "Rotation", "Comment")
        write_csv(output / "landerviz-pcbway-centroid.csv", columns, surface)
        instructions = output / "instructions"
        instructions.mkdir()
        write_csv(instructions / "all-70-placements.csv", columns, all_parts)
        ox, oy = audit["coordinate_origin_native_mm"]
        pad_rows = []
        for ref in sorted(expected):
            for pad in components[ref]["pads"]:
                if not pad["number"]:
                    continue
                x, y = pad["xy"]
                pad_rows.append([ref, pad["number"], f"{x + ox:.6f}", f"{-y - oy:.6f}", pad["net"]])
        write_csv(instructions / "numbered-pad-locations.csv",
                  ("Designator", "Pad", "X (mm)", "Y (mm)", "Net"), pad_rows)
        for name in ("board-layers.pdf",):
            shutil.copyfile(snapshot / name, instructions / name)
        shutil.copyfile(HW / "PCBWAY_ASSEMBLY_NOTES.md", instructions / "READ_FIRST.md")
        with zipfile.ZipFile(output / "landerviz-assembly-instructions.zip", "w", zipfile.ZIP_DEFLATED) as archive:
            for name in ("READ_FIRST.md", "all-70-placements.csv", "board-layers.pdf"):
                archive.write(instructions / name, name)
        result = {
            "status": "QUOTE ONLY; manufacturing acceptance and assembly orientations pending",
            "snapshot": snapshot.name, "snapshot_manifest_sha256": digest(snapshot / "manifest.json"),
            "exporter_sha256": digest(Path(__file__)),
            "validator_sha256": digest(HW / "prepare_jlc.py"),
            "assembly_notes_sha256": digest(HW / "PCBWAY_ASSEMBLY_NOTES.md"),
            "fabrication_notes_sha256": digest(HW / "PCBWAY_FABRICATION_NOTES.md"),
            "bom_lines": len(bom), "populated_positions_per_board": len(expected),
            "centroid_positions": len(surface),
            "pure_through_hole_in_instructions": sorted(expected - {r[0] for r in surface}),
            "coordinates": "Original Gerber origin; X right, Y up (negative native KiCad Y); millimetres",
            "rotation_status": "Native CAD anchors/rotations, not approved vendor machine offsets",
            "sha256": {str(p.relative_to(output)): digest(p) for p in sorted(output.rglob("*")) if p.is_file()},
        }
        (output / "manifest.json").write_text(json.dumps(result, indent=2) + "\n")
        print(f"PCBWay quote package: {output}\nBOM: {len(bom)} lines / {len(expected)} positions; "
              f"centroid: {len(surface)}; all 70 positions included in instructions.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("snapshot", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        build(args.snapshot, args.output)
    except (ValueError, KeyError, FileNotFoundError) as error:
        raise SystemExit(str(error))
