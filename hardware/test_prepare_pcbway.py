"""Check quantities, assembly types and reference coverage without KiCad."""

import unittest

from prepare_pcbway import assembly_type, grouped_bom, placement_rows


class HandoffTests(unittest.TestCase):
    def component(self, ref, drilled=False, mixed=False):
        pads = [{"number": "1", "xy": [2, 3], "drill": [1, 1] if drilled else [0, 0]}]
        if mixed:
            pads.append({"number": "2", "xy": [4, 3], "drill": [0, 0]})
        return {"reference": ref, "xy": [3, 3], "pads": pads}

    def test_assembly_types(self):
        self.assertEqual(assembly_type(self.component("R1")), "SMD")
        self.assertEqual(assembly_type(self.component("J3", drilled=True)), "thru-hole")
        self.assertEqual(assembly_type(self.component("J1", drilled=True, mixed=True)), "Hybrid")

    def test_grouped_quantities_are_per_board_and_exclude_dnp(self):
        parts = [{"Reference": r, "MPN": "EXACT-PART", "Manufacturer": "Maker",
                  "Footprint": "Library:Package", "Value": "10k", "DNP": dnp}
                 for r, dnp in (("R1", ""), ("R2", ""), ("R3", "1"))]
        components = {r: self.component(r) for r in ("R1", "R2", "R3")}
        result = grouped_bom(parts, components)
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0][1:3], ["R1,R2", 2])
        self.assertEqual(result[0][4], "EXACT-PART")

    def test_centroid_excludes_only_pure_through_hole(self):
        components = {r: self.component(r, drilled=r.startswith("J"), mixed=r == "J1")
                      for r in ("R1", "J1", "J3")}
        positions = [{"Ref": r, "Package": "pkg", "PosX": "103", "PosY": "-103",
                      "Rot": "-90", "Side": "top", "Val": "value"} for r in components]
        main, all_parts = placement_rows(positions, components)
        self.assertEqual({r[0] for r in main}, {"R1", "J1"})
        self.assertEqual({r[0] for r in all_parts}, set(components))
        self.assertEqual(main[0][2:6], ["103.000000mm", "-103.000000mm", "T", "270.000000"])


if __name__ == "__main__":
    unittest.main()
