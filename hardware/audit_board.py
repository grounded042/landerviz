"""Read-only review diagnostics. Uses KiCad's bundled pcbnew, no pip packages.

Run through prepare_review.py. Coordinates are millimetres from the board's
upper-left corner (100,100 mm in the native KiCad file).
"""

import argparse
import heapq
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


def review_checks(board, netlist):
    """Screen the reviewed layout mistakes; this does not model regulator stability.

    Union actual top-layer pads, tracks and filled ground. Separate top islands
    deliberately join through the inner ground plane; don't union other layers.
    """
    footprints = {f.GetReference(): f for f in board.GetFootprints()}
    pads = {(ref, q.GetNumber()): q for ref, f in footprints.items() for q in f.Pads()}
    checks = []

    def record(name, passed, **evidence):
        checks.append({"name": name, "pass": bool(passed), **evidence})

    def distance(a, b):
        return math.hypot(pcb.ToMM(a.x - b.x), pcb.ToMM(a.y - b.y))

    dots = [g.GetCenter() for g in footprints["L1"].GraphicalItems()
            if isinstance(g, pcb.PCB_SHAPE) and g.GetShape() == pcb.SHAPE_T_CIRCLE
            and g.GetLayer() == pcb.F_Fab]
    record("L1 physical dot faces output pad 1", len(dots) == 1 and
           distance(dots[0], pads["L1", "1"].GetPosition()) <
           distance(dots[0], pads["L1", "2"].GetPosition()))

    ground = pcb.SHAPE_POLY_SET()
    for f in footprints.values():
        for pad in f.Pads():
            if pad.GetNetname() == "GND" and pad.IsOnLayer(pcb.F_Cu):
                pad.TransformShapeToPolygon(ground, pcb.F_Cu, 0, pcb.FromMM(.001), pcb.ERROR_INSIDE)
    for track in board.GetTracks():
        if track.GetNetname() == "GND" and track.IsOnLayer(pcb.F_Cu):
            track.TransformShapeToPolygon(ground, pcb.F_Cu, 0, pcb.FromMM(.001), pcb.ERROR_INSIDE)
    for zone in board.Zones():
        if zone.GetNetname() == "GND" and zone.GetLayer() == pcb.F_Cu and not zone.GetIsRuleArea():
            ground.BooleanAdd(zone.GetFilledPolysList(pcb.F_Cu))
    ground.Simplify()

    def islands(point):
        return {i for i in range(ground.OutlineCount()) if ground.Contains(point, i)}

    quiet_pad = pads["C9", "2"].GetPosition()
    switch_pad = pads["U1", "47"].GetPosition()
    quiet, switching = islands(quiet_pad), islands(switch_pad)
    record("C9 has no top-copper connection to switching return",
           quiet and switching and quiet.isdisjoint(switching))
    vias = [v for v in board.GetTracks() if isinstance(v, pcb.PCB_VIA) and v.GetNetname() == "GND"]
    quiet_vias = [v for v in vias if distance(v.GetPosition(), quiet_pad) <= .9 and
                  islands(v.GetPosition()) & quiet and not islands(v.GetPosition()) & switching]
    record("C9 has its own nearby ground via", bool(quiet_vias), maximum_distance_mm=.9)
    switch_vias = [v for v in vias if distance(v.GetPosition(), switch_pad) <= 2.5 and
                   islands(v.GetPosition()) & switching]
    pairs = [round(distance(a.GetPosition(), b.GetPosition()), 4)
             for i, a in enumerate(switch_vias) for b in switch_vias[i + 1:]
             if distance(a.GetPosition(), b.GetPosition()) <= .8]
    record("Switching return has adjacent ground vias", bool(pairs), pair_distances_mm=pairs,
           maximum_distance_from_pgnd_mm=2.5)

    output_pad = pads["C25", "1"].GetPosition()
    closest = min(distance(output_pad, pads["J2", number].GetPosition())
                  for number in ("A4", "A9", "B4", "B9"))
    record("C25 is local to J2 VBUS", closest <= 5, straight_line_distance_mm=round(closest, 4),
           limitation="Placement screen, not connection inductance or transient simulation.")

    schematic = {c.get("ref"): {f.get("name"): f.text for f in c.findall("fields/field")}
                 for c in netlist.findall("components/comp")}
    for ref in ("C6", "C7", "C9", "C10"):
        expected = {"MPN": "GRM155R60J475ME47D", "LCSC": "C82453"}
        actual = {name: footprints[ref].GetField(name).GetText() for name in expected}
        record(f"{ref} uses reference-design capacitor in board and schematic",
               actual == expected and all(schematic.get(ref, {}).get(k) == v for k, v in expected.items()),
               expected=expected, board=actual)
    return checks


def input_power_checks(board, netlist, pin_nets):
    footprints = {f.GetReference(): f for f in board.GetFootprints()}
    expected = {
        "U6": ("TPS22953DQCR", "C471046"),
        "C30": ("CL10A105KA8NNNC", "C5673"),
        "C31": ("CL05B103KB5NNNC", "C15195"),
        "R24": ("0402WGF1003TCE", "C25741"),
        "R25": ("0402WGF2002TCE", "C25765"),
    }
    fields = {c.get("ref"): {f.get("name"): f.text for f in c.findall("fields/field")}
              for c in netlist.findall("components/comp")}
    checks = []
    for ref, (mpn, lcsc) in expected.items():
        wanted = {"MPN": mpn, "LCSC": lcsc}
        actual = {k: footprints[ref].GetField(k).GetText() for k in wanted} if ref in footprints else {}
        checks.append({"name": f"{ref} approved charging-circuit part",
                       "pass": actual == wanted and all(fields.get(ref, {}).get(k) == v for k, v in wanted.items()),
                       "expected": wanted, "board": actual})
    new_pads = [q for ref in expected if ref in footprints for q in footprints[ref].Pads() if q.GetNumber()]
    checks.append({"name": "Charging-circuit board pad nets match schematic",
                   "pass": len(new_pads) == 19 and all(str(q.GetNetname()) == pin_nets.get(
                       (q.GetParentFootprint().GetReference(), q.GetNumber())) for q in new_pads)})
    if "U6" not in footprints:
        checks.append({"name": "U6 physical layout present", "pass": False})
        return checks
    assembled = list(expected) + ["C21", "C22"]
    paste = [q.GetEffectiveShape(pcb.F_Paste) for ref in assembled
             for q in footprints[ref].Pads() if q.IsOnLayer(pcb.F_Paste)]
    holes = [v.GetEffectiveShape(pcb.F_Cu) for v in board.GetTracks() if isinstance(v, pcb.PCB_VIA)]
    holes += [q.GetEffectiveShape(pcb.F_Cu) for ref in assembled
              for q in footprints[ref].Pads() if q.GetDrillSize().x]
    checks.append({"name": "New and moved components have no via lands or drilled pads beneath paste",
                   "pass": not any(pcb.SHAPE.Collide(h, aperture, 0) for h in holes for aperture in paste)})
    timing_net = pin_nets.get(("U6", "6"))
    timing = [t for t in board.GetTracks() if str(t.GetNetname()) == timing_net]
    length = sum(pcb.ToMM(t.GetLength()) for t in timing if not isinstance(t, pcb.PCB_VIA))
    checks.append({"name": "Timing connection is short, top-only and without vias",
                   "length_mm": round(length, 4),
                   "pass": bool(timing) and length <= 5 and all(
                       not isinstance(t, pcb.PCB_VIA) and t.GetLayer() == pcb.F_Cu for t in timing)})
    # Restrict the connectivity graph to top copper, excluding vias. Counting
    # entire touched segments is conservative; pad spreading is not modelled.
    connectivity = board.GetConnectivity()
    items = [q for f in footprints.values() for q in f.Pads() if q.IsOnLayer(pcb.F_Cu)]
    items += [t for t in board.GetTracks() if not isinstance(t, pcb.PCB_VIA)
              and t.GetLayer() == pcb.F_Cu]
    nodes = {i.m_Uuid.AsString(): i for i in items}
    edges = defaultdict(set)
    for key, item in nodes.items():
        for other in list(connectivity.GetConnectedTracks(item)) + list(connectivity.GetConnectedPads(item)):
            target = other.m_Uuid.AsString()
            if target in nodes and target != key and item.GetNetCode() == other.GetNetCode():
                edges[key].add(target)
                edges[target].add(key)

    def top_length(a, b):
        start, end = a.m_Uuid.AsString(), b.m_Uuid.AsString()
        queue, distances = [(0, start)], {start: 0}
        while queue:
            length, key = heapq.heappop(queue)
            if length != distances[key]:
                continue
            if key == end:
                return round(length, 4)
            for target in edges[key]:
                item = nodes[target]
                candidate = length + (pcb.ToMM(item.GetLength()) if isinstance(item, pcb.PCB_TRACK) else 0)
                if candidate < distances.get(target, math.inf):
                    distances[target] = candidate
                    heapq.heappush(queue, (candidate, target))
        return None

    def pad(ref, number):
        return next(q for q in footprints[ref].Pads() if q.GetNumber() == number)

    for name, a, b, limit in (
        ("U6 output ceramic bypass is direct on top copper", ("U6", "9"), ("C21", "1"), 3),
        ("U5 retains local input ceramic bypass on top copper", ("U5", "1"), ("C21", "1"), 3),
        ("U6 input ceramic bypass is local on top copper", ("U6", "1"), ("C30", "1"), 3),
    ):
        length = top_length(pad(*a), pad(*b))
        checks.append({"name": name, "top_track_length_mm": length, "maximum_mm": limit,
                       "pass": length is not None and length <= limit,
                       "limitation": "Geometry screen; no transient or inductance simulation."})
    return checks


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
        [("J1", "A4"), ("F1", "1")],
        [("F1", "2"), ("U6", "1"), ("U6", "2"), ("U6", "3"),
         ("C30", "1"), ("R24", "1")],
        [("U6", "9"), ("U6", "10"), ("U6", "8"), ("C20", "1"), ("U2", "1"), ("U2", "2"),
         ("U2", "3"), ("U2", "4"), ("U5", "1"), ("U5", "3"), ("F2", "1")],
        [("U6", "4"), ("R24", "2"), ("R25", "1")],
        [("U6", "6"), ("C31", "1")],
        [("U6", "5"), ("U6", "7"), ("U6", "11"), ("C30", "2"),
         ("C31", "2"), ("R25", "2"), ("U1", "61")],
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
    raw = pin_nets.get(("F1", "2"))
    expected_raw = {("F1", "2"), ("U6", "1"), ("U6", "2"), ("U6", "3"), ("C30", "1"), ("R24", "1")}
    actual_raw = {pin for pin, name in pin_nets.items() if name == raw and not pin[0].startswith("#")}
    checks.append({"name": "Only approved input loads precede the ramp switch",
                   "pass": raw is not None and actual_raw == expected_raw,
                   "actual": sorted(actual_raw), "expected": sorted(expected_raw)})
    power_nets = [pin_nets.get(pin) for pin in (("F1", "1"), ("F1", "2"), ("U6", "9"))]
    checks.append({"name": "USB input, fused input and ramped output are distinct",
                   "nets": power_nets, "pass": None not in power_nets and len(set(power_nets)) == 3})

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
                          "R11": "5.1k", "R12": "5.1k", "R13": "100k 0.1%",
                          "U6": "TPS22953DQCR", "C30": "1uF", "C31": "10nF 50V",
                          "R24": "100k", "R25": "20k"}.items():
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
    reviewed = review_checks(board, netlist)
    charging = input_power_checks(board, netlist, pin_nets)
    return {
        "coordinate_origin_native_mm": [100, 100],
        "topology_checks": checks, "topology_failures": [c for c in checks if not c["pass"]],
        "value_checks": value_checks, "value_failures": [c for c in value_checks if not c["pass"]],
        "review_checks": reviewed, "review_failures": [c for c in reviewed if not c["pass"]],
        "input_power_checks": charging, "input_power_failures": [c for c in charging if not c["pass"]],
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
          f"review: {len(result['review_checks'])} checks, {len(result['review_failures'])} failures; "
          f"input power: {len(result['input_power_checks'])} checks, {len(result['input_power_failures'])} failures; "
          f"USB reference: {result['usb_reference_sampling']['samples']} samples, "
          f"{len(result['usb_reference_sampling']['misses'])} misses")
    raise SystemExit(bool(result["topology_failures"] or result["value_failures"] or
                          result["review_failures"] or result["input_power_failures"] or
                          result["usb_reference_sampling"]["misses"]))
