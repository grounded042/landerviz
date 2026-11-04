"""Read-only review diagnostics. Uses KiCad's bundled pcbnew, no pip packages.

Run through prepare_review.py. Coordinates are millimetres from the board's
upper-left corner (100,100 mm in the native KiCad file).
"""

import argparse
import json
import math
from collections import defaultdict
from pathlib import Path
import xml.etree.ElementTree as ET

import pcbnew as pcb


def usb(net):
    return any(s in net for s in (
        "USB_DP", "USB_DM", "_D+", "_D-", "Net-(U1-GPIO0)", "Net-(U1-GPIO1)"
    ))


def audit(board_path, netlist_path):
    board = pcb.LoadBoard(str(board_path))
    netlist = ET.parse(netlist_path).getroot()
    nets = {
        net.get("name"): [dict(node.attrib) for node in net]
        for net in netlist.find("nets")
    }
    pin_nets = {(n["ref"], n["pin"]): name for name, ns in nets.items() for n in ns}
    checks = []

    def same(*pins):
        values = [pin_nets.get(tuple(pin)) for pin in pins]
        checks.append({"pins": pins, "nets": values,
                       "pass": values[0] is not None and len(set(values)) == 1})

    for port in ("J1", "J2"):
        for numbers in (("A6", "B6"), ("A7", "B7"),
                        ("A4", "A9", "B4", "B9"), ("A1", "A12", "B1", "B12", "SH")):
            same(*[(port, number) for number in numbers])
    for pins in [
        [("J1", "A6"), ("U3", "1"), ("R7", "1")],
        [("J1", "A7"), ("U3", "2"), ("R8", "1")],
        [("R7", "2"), ("U1", "52")], [("R8", "2"), ("U1", "51")],
        [("J2", "A6"), ("U4", "1"), ("R9", "2")],
        [("J2", "A7"), ("U4", "2"), ("R10", "2")],
        [("R9", "1"), ("U1", "2")], [("R10", "1"), ("U1", "3")],
        [("J1", "A5"), ("R11", "1")], [("J1", "B5"), ("R12", "1")],
        [("J2", "A5"), ("U2", "9")], [("J2", "B5"), ("U2", "11")],
        [("J2", "A4"), ("U2", "12"), ("C25", "1")],
        [("F1", "2"), ("C20", "1"), ("U2", "1"), ("U2", "2"),
         ("U2", "3"), ("U2", "4"), ("U5", "1"), ("U5", "3"), ("F2", "1")],
        [("U2", "8"), ("R13", "1")],
        [("U1", "48"), ("L1", "2")],
        [("L1", "1"), ("C7", "1"), ("C10", "1"), ("U1", "6"),
         ("U1", "23"), ("U1", "39"), ("U1", "50")],
        [("U1", "46"), ("R3", "2"), ("C9", "1")],
        [("U1", "60"), ("R6", "2")], [("R6", "1"), ("SW1", "2")],
        [("U1", "26"), ("R4", "2")], [("R4", "1"), ("SW2", "2")],
        [("J4", "2"), ("F2", "2")],
        [("U1", "4"), ("Q1", "2"), ("R20", "2")],
        [("U1", "5"), ("Q2", "2"), ("R21", "2")],
        [("J4", "3"), ("Q1", "3"), ("R22", "2")],
        [("J4", "4"), ("Q2", "3"), ("R23", "2")],
        [("C26", "1"), ("R14", "2"), ("R15", "1"), ("R16", "1"), ("TP7", "1")],
        [("C27", "1"), ("R17", "2"), ("R18", "1"), ("R19", "1"), ("TP9", "1")],
        [("U1", "40"), ("R16", "2"), ("C28", "1"), ("D1", "3"), ("TP8", "1")],
        [("U1", "41"), ("R19", "2"), ("C29", "1"), ("D2", "3"), ("TP10", "1")],
        [("U1", "7"), ("SW3", "A")], [("U1", "8"), ("SW3", "B")],
        [("U1", "12"), ("SW3", "S1")],
        [("J3", "T"), ("C26", "2")], [("J3", "R"), ("C27", "2")],
    ]:
        same(*pins)
    for pin in ("1", "11", "20", "30", "38", "44", "45", "49", "53", "54"):
        same(("U1", pin), ("U5", "5"))
    for ref, number in (("R11", "2"), ("R12", "2"), ("R13", "2"),
                        ("D1", "1"), ("D2", "1"), ("J4", "1"),
                        ("U2", "10"), ("U2", "13"), ("U1", "47"),
                        ("J3", "S"), ("SW3", "C"), ("SW3", "S2"),
                        ("SW1", "1"), ("SW2", "1")):
        same((ref, number), ("U1", "61"))
    for ref, number in (("D1", "2"), ("D2", "2"), ("Q1", "1"), ("Q2", "1")):
        same((ref, number), ("U5", "5"))

    def mm(v):
        return round(pcb.ToMM(v), 5)

    def xy(v):
        return [round(pcb.ToMM(v.x) - 100, 5), round(pcb.ToMM(v.y) - 100, 5)]

    components = []
    for f in board.GetFootprints():
        components.append({
            "reference": f.GetReference(), "value": f.GetValue(),
            "footprint": f.GetFPIDAsString(), "xy": xy(f.GetPosition()),
            "rotation_deg": f.GetOrientationDegrees(),
            "pads": [{"number": q.GetNumber(), "net": str(q.GetNetname()),
                      "xy": xy(q.GetPosition()), "size": [mm(q.GetSize().x), mm(q.GetSize().y)],
                      "drill": [mm(q.GetDrillSize().x), mm(q.GetDrillSize().y)]}
                     for q in f.Pads()],
        })
    value_checks = []
    component_values = {c["reference"]: c["value"] for c in components}
    for ref, expected in {"C10": "4.7uF", "C6": "4.7uF", "C7": "4.7uF", "C9": "4.7uF",
                          "R7": "27R", "R8": "27R", "R9": "22R", "R10": "22R",
                          "R11": "5.1k", "R12": "5.1k", "R13": "100k 0.1%"}.items():
        value_checks.append({"reference": ref, "expected": expected,
                             "actual": component_values.get(ref), "pass": component_values.get(ref) == expected})
    vias, tracks = [], []
    for t in board.GetTracks():
        if isinstance(t, pcb.PCB_VIA):
            vias.append({"net": str(t.GetNetname()), "xy": xy(t.GetPosition()),
                         "diameter": mm(t.GetWidth(pcb.F_Cu)), "drill": mm(t.GetDrill())})
        else:
            tracks.append({"net": str(t.GetNetname()), "layer": board.GetLayerName(t.GetLayer()),
                           "start": xy(t.GetStart()), "end": xy(t.GetEnd()),
                           "width": mm(t.GetWidth()), "length": mm(t.GetLength())})
    by_usb_net = defaultdict(lambda: {"length_mm": 0, "vias": 0, "widths_mm": set()})
    for t in tracks:
        if usb(t["net"]):
            by_usb_net[t["net"]]["length_mm"] += t["length"]
            by_usb_net[t["net"]]["widths_mm"].add(t["width"])
    for v in vias:
        if usb(v["net"]):
            by_usb_net[v["net"]]["vias"] += 1
    for values in by_usb_net.values():
        values["length_mm"] = round(values["length_mm"], 4)
        values["widths_mm"] = sorted(values["widths_mm"])

    planes = {z.GetLayer(): z.GetFilledPolysList(z.GetLayer())
              for z in board.Zones() if str(z.GetNetname()) == "GND" and not z.GetIsRuleArea()}
    launches = [v["xy"] for v in vias if usb(v["net"])]
    samples, skipped, misses = 0, 0, []
    for t in tracks:
        if not usb(t["net"]) or t["layer"] not in ("F.Cu", "B.Cu"):
            continue
        a, c = t["start"], t["end"]
        length = math.dist(a, c)
        if length < 1e-6:
            continue
        normal = (-(c[1] - a[1]) / length, (c[0] - a[0]) / length)
        plane = planes[pcb.In1_Cu if t["layer"] == "F.Cu" else pcb.In2_Cu]
        for i in range(math.ceil(length / .1) + 1):
            f = min(1, i * .1 / length)
            center = [a[k] + (c[k] - a[k]) * f for k in range(2)]
            if any(math.dist(center, v) < .8 for v in launches):
                skipped += 3
                continue
            for off in (-t["width"] / 2, 0, t["width"] / 2):
                point = [center[k] + normal[k] * off for k in range(2)]
                samples += 1
                if not plane.Contains(pcb.VECTOR2I(*[pcb.FromMM(v + 100) for v in point])):
                    misses.append({"net": t["net"], "layer": t["layer"], "xy": point})
    ground = [v["xy"] for v in vias if v["net"] == "GND"]
    return {
        "coordinate_origin_native_mm": [100, 100],
        "topology_checks": checks, "topology_failures": [c for c in checks if not c["pass"]],
        "value_checks": value_checks, "value_failures": [c for c in value_checks if not c["pass"]],
        "nets": nets, "components": sorted(components, key=lambda f: f["reference"]),
        "tracks": tracks, "vias": vias, "usb_net_totals": dict(by_usb_net),
        "ground_regions": {board.GetLayerName(layer): poly.OutlineCount() for layer, poly in planes.items()},
        "usb_reference_sampling": {"step_mm": .1, "launch_exclusion_radius_mm": .8,
                                   "samples": samples, "skipped_samples": skipped, "misses": misses,
                                   "limitation": "Geometric coverage only; no impedance, field-solver, launch, or compliance claim."},
        "usb_nearest_ground_via": [{"xy": v, "distance_mm": round(min(math.dist(v, g) for g in ground), 4)} for v in launches],
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("board", type=Path)
    parser.add_argument("netlist", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    result = audit(args.board, args.netlist)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(f"Topology: {len(result['topology_checks'])} checks, {len(result['topology_failures'])} failures; "
          f"values: {len(result['value_checks'])} checks, {len(result['value_failures'])} failures; "
          f"USB reference: {result['usb_reference_sampling']['samples']} samples, "
          f"{len(result['usb_reference_sampling']['misses'])} misses")
    raise SystemExit(bool(result["topology_failures"] or result["value_failures"] or result["usb_reference_sampling"]["misses"]))
