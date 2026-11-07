"""Convert a checked review snapshot into separate JLCPCB upload files.

Standard library only. Does not export KiCad, modify a snapshot, upload, or
approve production. Native rotations still require JLCPCB preview approval.
"""

import argparse
import csv
from decimal import Decimal
import hashlib
import json
from pathlib import Path
import re
import zipfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rows(path):
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def write_csv(path, columns, data):
    with path.open("w", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(columns)
        writer.writerows(data)


def prepare(snapshot, out):
    if out.exists():
        raise ValueError(f"Refusing to overwrite {out}")
    manifest = json.loads((snapshot / "manifest.json").read_text())
    for name, expected in manifest["artifact_sha256"].items():
        path = snapshot / name
        if not path.is_file() or digest(path) != expected:
            raise ValueError(f"Snapshot artifact changed: {name}")
    audit = json.loads((snapshot / "audit.json").read_text())
    if any(audit[k] for k in ("topology_failures", "value_failures", "review_failures", "input_power_failures")):
        raise ValueError("Snapshot audit has failures")
    if audit["usb_reference_sampling"]["misses"]:
        raise ValueError("Snapshot USB coverage has misses")
    drc = json.loads((snapshot / "drc.json").read_text())
    if any(drc[k] for k in ("violations", "unconnected_items", "schematic_parity")):
        raise ValueError("Snapshot DRC has findings")
    erc = json.loads((snapshot / "erc.json").read_text())
    if any(sheet["violations"] for sheet in erc["sheets"]):
        raise ValueError("Snapshot ERC has findings")

    bom = [r for r in rows(snapshot / "components.csv") if r["MPN"] and not r["DNP"]]
    positions = rows(snapshot / "positions-populated.csv")
    refs = {r["Reference"] for r in bom}
    if len(refs) != len(bom) or len({r["Ref"] for r in positions}) != len(positions):
        raise ValueError("Duplicate component reference")
    if refs != {r["Ref"] for r in positions} or len(refs) != manifest["purchased_populated_positions"]:
        raise ValueError("BOM and placement references/count differ")
    if any(not re.fullmatch(r"C\d+", r["LCSC"]) for r in bom):
        raise ValueError("Missing or invalid LCSC part number")

    # Preserve the original Gerber/Excellon coordinate origin. Confirm the
    # native CPL transform against the independent native-board audit, and
    # confirm that transform against tagged Gerber pad flashes at U6/C3/C21.
    components = {c["reference"]: c for c in audit["components"]}
    ox, oy = audit["coordinate_origin_native_mm"]
    cpl = []
    for r in positions:
        c = components[r["Ref"]]
        x, y, rotation = (Decimal(r[k]) for k in ("PosX", "PosY", "Rot"))
        expected = (Decimal(str(c["xy"][0])) + Decimal(str(ox)),
                    -Decimal(str(c["xy"][1])) - Decimal(str(oy)))
        if not all(v.is_finite() for v in (x, y, rotation)) or any(
                abs(a - b) > Decimal("0.00001") for a, b in zip((x, y), expected)):
            raise ValueError(f"Placement origin/coordinates differ: {r['Ref']}")
        if r["Side"] != "top":
            raise ValueError("This board's assembly is top-only; inspect unexpected side")
        if abs((rotation - Decimal(str(c["rotation_deg"]))) % 360) > Decimal("0.00001"):
            raise ValueError(f"Native rotation differs: {r['Ref']}")
        rotation = (rotation % 360 + 360) % 360
        cpl.append((r["Ref"], f"{x:.6f}mm", f"{y:.6f}mm", "Top", f"{rotation:.6f}"))

    fab = snapshot / "draft-manufacturing"
    suffixes = ("F_Cu.gtl", "In1_Cu.g1", "In2_Cu.g2", "B_Cu.gbl", "F_Mask.gts",
                "B_Mask.gbs", "F_Paste.gtp", "F_Silkscreen.gto", "B_Silkscreen.gbo",
                "Edge_Cuts.gm1", "PTH.drl", "NPTH.drl")
    files = [fab / f"landerviz-{suffix}" for suffix in suffixes]
    if not all(p.is_file() for p in files):
        raise ValueError("Missing Gerber layer or drill file")
    copper = files[0].read_text()
    if "%FSLAX46Y46*%" not in copper or "%MOMM*%" not in copper:
        raise ValueError("Unexpected Gerber coordinate format")
    flashes = {}
    tag = None
    for line in copper.splitlines():
        m = re.fullmatch(r"%TO\.P,([^,]+),([^,*]+)\*%", line)
        if m:
            tag = m.groups()
        elif line == "%TD*%":
            tag = None
        m = re.fullmatch(r"X(-?\d+)Y(-?\d+)D03\*", line)
        if m and tag:
            flashes[tag] = tuple(Decimal(v) / 1000000 for v in m.groups())
    for ref in ("U6", "C3", "C21"):
        for pad in components[ref]["pads"]:
            if not pad["number"]:
                continue
            key = (ref, pad["number"])
            expected = (Decimal(str(pad["xy"][0])) + Decimal(str(ox)),
                        -Decimal(str(pad["xy"][1])) - Decimal(str(oy)))
            if key not in flashes or any(abs(a - b) > Decimal("0.00001")
                                         for a, b in zip(flashes[key], expected)):
                raise ValueError(f"Gerber and placement origins differ at {key}")

    drill_points = set()
    precision = Decimal("0.0001")
    for path in files[-2:]:
        text = path.read_text()
        if "METRIC" not in text:
            raise ValueError("Unexpected drill units")
        drill_points.update(tuple(Decimal(v).quantize(precision) for v in match)
                            for match in re.findall(r"^X(-?[\d.]+)Y(-?[\d.]+)$", text, re.M))
    for via in audit["vias"]:
        point = (Decimal(str(via["xy"][0])) + Decimal(str(ox)),
                 -Decimal(str(via["xy"][1])) - Decimal(str(oy)))
        # Native Excellon output rounds to 0.001 mm; retain that tolerance.
        if not any(all(abs(a - b) <= Decimal("0.00051") for a, b in zip(point, drilled))
                   for drilled in drill_points):
            raise ValueError(f"Drill and placement origins differ at via {via['xy']}")

    out.mkdir(parents=True)
    with zipfile.ZipFile(out / "landerviz-gerbers.zip", "w", zipfile.ZIP_DEFLATED) as archive:
        for path in files:
            archive.write(path, path.name)
    write_csv(out / "landerviz-bom.csv",
              ("Comment", "Designator", "Footprint", "Manufacturer", "MPN", "LCSC Part #"),
              [(r["Value"], r["Reference"], r["Footprint"].split(":")[-1],
                r["Manufacturer"], r["MPN"], r["LCSC"]) for r in bom])
    write_csv(out / "landerviz-cpl.csv", ("Designator", "Mid X", "Mid Y", "Layer", "Rotation"), cpl)
    result = {"status": "UPLOAD FORMAT ONLY; MANUFACTURER ACCEPTANCE AND ROTATIONS NOT APPROVED",
              "snapshot": snapshot.name, "snapshot_manifest_sha256": digest(snapshot / "manifest.json"),
              "populated_positions": len(refs), "gerber_and_drill_files": len(files),
              "coordinate_transform": "Native X, negative native Y; no origin shift",
              "sha256": {p.name: digest(p) for p in sorted(out.iterdir())}}
    (out / "manifest.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("snapshot", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        prepare(args.snapshot, args.output)
    except (ValueError, KeyError, FileNotFoundError) as error:
        raise SystemExit(str(error))
