"""Build a local, read-only review snapshot using installed KiCad tools.

No network access, package installation, staging, or manufacturing upload.
Use KiCad's bundled Python so audit_board.py can import pcbnew.
Generated files live in ignored hardware/fabrication/.
"""

import argparse
import csv
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parent.parent
HW = ROOT / "hardware"


def sources():
    return sorted([ROOT / "PCB.md"] + [p for p in HW.rglob("*") if p.is_file()
        and "fabrication" not in p.relative_to(HW).parts
        and (p.suffix in (".kicad_pcb", ".kicad_pro", ".kicad_sch", ".kicad_sym",
                          ".kicad_mod", ".kicad_dru", ".md", ".py", ".txt")
             or p.name in ("fp-lib-table", "sym-lib-table"))])


def hashes(paths):
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kicad-cli", default="/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli")
    parser.add_argument("--references", type=Path, help="Optional directory of locally downloaded datasheets")
    args = parser.parse_args()
    out = HW / "fabrication" / "astra-review"
    if out.exists():
        raise SystemExit(f"{out} already exists; choose a new snapshot by moving it aside first.")
    out.mkdir(parents=True)
    before = hashes(sources())
    log = []

    def run(*command):
        command = [str(c) for c in command]
        print(" ".join(command), flush=True)
        result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
        log.append({"command": command, "returncode": result.returncode,
                    "stdout": result.stdout, "stderr": result.stderr})
        (out / "commands.json").write_text(json.dumps(log, indent=2) + "\n")
        if result.returncode:
            raise SystemExit(result.stdout + result.stderr)
        return result.stdout.strip()

    cli = args.kicad_cli
    version = run(cli, "version")
    sch, board = HW / "landerviz.kicad_sch", HW / "landerviz.kicad_pcb"
    run(cli, "sch", "erc", "--format", "json", "--exit-code-violations", "-o", out / "erc.json", sch)
    run(cli, "pcb", "drc", "--refill-zones", "--schematic-parity", "--all-track-errors",
        "--format", "json", "--exit-code-violations", "-o", out / "drc.json", board)
    run(cli, "sch", "export", "netlist", "--format", "kicadxml", "-o", out / "netlist.xml", sch)
    run(sys.executable, HW / "audit_board.py", board, out / "netlist.xml", out / "audit.json")
    run(cli, "sch", "export", "bom", "--fields", "Reference,Value,Footprint,Manufacturer,MPN,LCSC,QUANTITY,DNP",
        "-o", out / "components.csv", sch)
    run(cli, "sch", "export", "pdf", "-o", out / "schematics.pdf", sch)
    run(cli, "pcb", "export", "pdf", "--mode-multipage", "--scale", "3",
        "--layers", "F.Cu,In1.Cu,In2.Cu,B.Cu,F.Fab,B.Fab,F.Paste,F.Mask,B.Mask,F.Silkscreen,B.Silkscreen",
        "--common-layers", "Edge.Cuts", "-o", out / "board-layers.pdf", board)
    # Draft manufacturing files allow inspection of exactly what the fab would see.
    # They are explicitly not a release or an order-ready archive.
    fab = out / "draft-manufacturing"
    fab.mkdir()
    run(cli, "pcb", "export", "gerbers", "--layers",
        "F.Cu,In1.Cu,In2.Cu,B.Cu,F.Mask,B.Mask,F.Paste,F.Silkscreen,B.Silkscreen,Edge.Cuts",
        "-o", str(fab) + "/", board)
    run(cli, "pcb", "export", "drill", "--excellon-separate-th", "--generate-map",
        "--generate-report", "--report-path", fab / "drills.rpt", "-o", str(fab) + "/", board)
    run(cli, "pcb", "export", "pos", "--format", "csv", "--units", "mm", "--exclude-dnp",
        "-o", out / "positions-native.csv", board)
    # Match placement rows to purchased/populated references, not bare probes/holes.
    with (out / "components.csv").open(newline="") as stream:
        components = list(csv.DictReader(stream))
    populated = {r["Reference"] for r in components if r["MPN"] and not r["DNP"]}
    with (out / "positions-native.csv").open(newline="") as stream:
        reader = csv.DictReader(stream)
        fieldnames, rows = reader.fieldnames, list(reader)
    selected = [r for r in rows if r["Ref"] in populated]
    if {r["Ref"] for r in selected} != populated:
        raise SystemExit("Placement/component references differ; inspect the two CSVs.")
    with (out / "positions-populated.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(selected)
    for side in ("top", "bottom"):
        run(cli, "pcb", "render", "--side", side, "--width", "1600", "--height", "1400",
            "--use-board-stackup-colors", "-o", out / f"board-{side}.png", board)
    after = hashes(sources())
    if before != after:
        raise SystemExit("Source changed during export; discard this snapshot and rebuild.")
    for relative in before:
        destination = out / "source" / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / relative, destination)
    if args.references:
        shutil.copytree(args.references, out / "references")
    manifest = {"status": "INDEPENDENT REVIEW ONLY — NOT FOR FABRICATION",
                "kicad_version": version, "source_sha256": before,
                "purchased_populated_positions": len(populated),
                "assembly_rotation_status": "Native KiCad rotations; JLCPCB orientation preview not approved."}
    (out / "START_HERE.md").write_text((HW / "ASTRA_REVIEW.md").read_text())
    manifest["artifact_sha256"] = {str(p.relative_to(out)): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted(out.rglob("*")) if p.is_file()}
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    archive = HW / "fabrication" / "landerviz-astra-review.zip"
    if archive.exists():
        raise SystemExit(f"Refusing to overwrite {archive}")
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as bundle:
        for p in sorted(out.rglob("*")):
            if p.is_file():
                bundle.write(p, p.relative_to(out))
    print(f"Review snapshot: {out}\nArchive: {archive}")


if __name__ == "__main__":
    main()
