"""Validate the 251 encounter catalog and its generated representation offline."""
import copy
import json
from pathlib import Path
import runpy
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
CATALOG = runpy.run_path(str(ROOT / "tools/build_adventure_catalog.py"))
GENERATE = CATALOG["generate"]
LOAD = CATALOG["load_catalog"]
PROGRESSION = json.loads((ROOT / "assets/evolution-251.json").read_text())


class AdventureCatalogTests(unittest.TestCase):
    def setUp(self):
        self.data = LOAD()

    def manifest(self, files):
        return {
            "ruleVersion": 3,
            "routeTypes": self.data["routeTypes"],
            "speciesFiles": files,
            "ordinaryRoots": self.data["ordinaryRoots"],
            "reserved": self.data["reserved"],
        }

    def test_deterministic_generated_header(self):
        generated = GENERATE(self.data, PROGRESSION, 251)
        self.assertEqual(generated, GENERATE(copy.deepcopy(self.data), PROGRESSION, 251))
        national = json.loads((ROOT / "assets/evolution-649.json").read_text())
        current = GENERATE(LOAD(ROOT / "assets/adventure-649.json"), national, 649)
        self.assertEqual(current,
                         (ROOT / "firmware/apps/20_pokedex/adventure_catalog_generated.hpp").read_text())

    def test_route_masks_derive_from_types(self):
        types = CATALOG["load_types"]()
        masks, ordinary = CATALOG["ownership"](self.data, PROGRESSION, 251)
        table = self.data["routeTypes"]
        bits = {"Forest": 1, "Coast": 2, "Cave": 4}
        for species in range(1, 252):
            expected = 0
            for name in types[species]:
                expected |= bits[table[name]]
            self.assertEqual(masks[species], expected, f"#{species} mask")
            self.assertTrue(masks[species], f"#{species} needs a route")
        expected_ordinary = {"Forest": 65, "Coast": 50, "Cave": 42}
        for route, count in expected_ordinary.items():
            pool = [i for i in ordinary if masks[i] & bits[route]]
            self.assertEqual(len(pool), count, f"{route} ordinary pool")
        legacy = CATALOG["generate_legacy_masks"]()
        self.assertEqual(legacy, (ROOT / "firmware/apps/20_pokedex/route_masks_v4_generated.hpp").read_text())
        frozen = CATALOG["FROZEN_V4_ROUTES"]
        self.assertEqual(len(frozen), 251)
        self.assertEqual(frozen[3], 1)  # 小火龙 sat in Forest before schema 5.
        self.assertEqual(masks[4], 4)   # Fire now belongs to Cave only.

    def test_ordinary_and_reserved_accounting(self):
        ordinary = self.data["ordinaryRoots"]
        self.assertEqual(len(ordinary), 124)
        self.assertEqual(len([i for i in ordinary if i <= 151]), 73)
        self.assertEqual(len([i for i in ordinary if i > 151]), 51)
        ruins = self.data["reserved"]["ruins"]
        self.assertEqual(ruins, [144, 145, 146, 147, 148, 149, 150, 151,
                                 243, 244, 245, 249, 250, 251])
        self.assertEqual(self.data["reserved"]["dragonCave"], [246])
        self.assertFalse(set(ordinary) & set(ruins))
        self.assertNotIn(246, ordinary)
        targets = {r["to"] for r in PROGRESSION["rules"]}
        held = set(ruins) | {246}
        for species in range(1, 252):
            self.assertTrue(species in ordinary or species in held or species in targets,
                            f"#{species} has no acquisition owner")

    def test_invalid_catalogs_are_rejected(self):
        mutations = [
            ("missing ID", lambda d: d["species"].pop()),
            ("duplicate ID", lambda d: d["species"][1].update(id=1)),
            ("boolean ID", lambda d: d["species"][0].update(id=True)),
            ("zero weight", lambda d: d["species"][0].update(weight=0)),
            ("negative weight", lambda d: d["species"][0].update(weight=-1)),
            ("weight overflow", lambda d: d["species"][0].update(weight=65536)),
            ("invalid rarity", lambda d: d["species"][0].update(rare=1)),
            ("handwritten routes", lambda d: d["species"][0].update(routes=["Mountain"])),
            ("missing routeTypes", lambda d: d.pop("routeTypes")),
            ("empty routeTypes", lambda d: d.update(routeTypes={})),
            ("routeTypes not a mapping", lambda d: d.update(routeTypes=["Forest"])),
            ("unknown type", lambda d: d["routeTypes"].update({"月光": "Forest"})),
            ("unknown route", lambda d: d["routeTypes"].update({"草": "Mountain"})),
            ("missing type", lambda d: d["routeTypes"].pop("钢")),
            ("empty route", lambda d: [d["routeTypes"].pop(name) for name, route in
                                       list(d["routeTypes"].items()) if route == "Cave"]),
            ("unknown rule", lambda d: d.update(ruleVersion=2)),
            ("ordinary unsorted", lambda d: d["ordinaryRoots"].reverse()),
            ("ordinary duplicate", lambda d: d["ordinaryRoots"].append(d["ordinaryRoots"][0])),
            ("ordinary non-int", lambda d: d["ordinaryRoots"].append(True)),
            ("ordinary out of range", lambda d: d["ordinaryRoots"].append(252)),
            ("ordinary missing Kanto root", lambda d: d["ordinaryRoots"].remove(1)),
            ("ordinary Johto evolved", lambda d: d["ordinaryRoots"].append(153)),
            ("ordinary reserved", lambda d: d["ordinaryRoots"].append(243)),
            ("reserved drift", lambda d: d["reserved"]["ruins"].append(246)),
            ("reserved missing", lambda d: d["reserved"].pop("dragonCave")),
        ]
        for label, mutate in mutations:
            with self.subTest(label=label):
                damaged = copy.deepcopy(self.data)
                mutate(damaged)
                with self.assertRaises(ValueError):
                    GENERATE(damaged, PROGRESSION, 251)

    def test_partition_order_does_not_change_generated_catalog(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "catalog.json").write_text(json.dumps(self.manifest(["even.json", "odd.json"])))
            for name, species in (("even.json", self.data["species"][1::2]),
                                  ("odd.json", self.data["species"][::2])):
                (root / name).write_text(json.dumps({"species": list(reversed(species))}))
            self.assertEqual(GENERATE(LOAD(root / "catalog.json"), PROGRESSION, 251),
                             GENERATE(self.data, PROGRESSION, 251))

    def test_invalid_partition_inputs_are_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            path = root / "catalog.json"
            manifests = [
                [], None,
                {"ruleVersion": 2},
                {"speciesFiles": []},
                {"speciesFiles": ["part.json", "part.json"]},
                {"speciesFiles": [False]},
                {"speciesFiles": ["../outside.json"]},
                {"speciesFiles": [str(root / "part.json")]},
                {"speciesFiles": ["part.json"], "species": []},
            ]
            for manifest in manifests:
                with self.subTest(manifest=manifest):
                    path.write_text(json.dumps(manifest))
                    with self.assertRaises(ValueError):
                        LOAD(path)
            path.write_text(json.dumps({"speciesFiles": ["part.json"]}))
            for part in ({}, {"species": "wrong"}, {"species": [False]},
                         {"species": [{"id": True}]}):
                with self.subTest(part=part):
                    (root / "part.json").write_text(json.dumps(part))
                    with self.assertRaises(ValueError):
                        LOAD(path)


if __name__ == "__main__":
    unittest.main()
