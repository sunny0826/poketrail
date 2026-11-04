"""Product encounter pools and region boundaries, independent of generated C++."""
import json
from pathlib import Path
import runpy
import unittest

ROOT = Path(__file__).resolve().parent.parent
CATALOG = runpy.run_path(str(ROOT / "tools/build_adventure_catalog.py"))
LOAD = CATALOG["load_catalog"]


class AdventureCatalogTests(unittest.TestCase):
    def test_regions_follow_pokedex_generations(self):
        national = LOAD(ROOT / "assets/adventure-649.json")
        spans = [(r["key"], r["first"], r["last"]) for r in national["regions"]]
        self.assertEqual(spans, [("Kanto", 1, 151), ("Johto", 152, 251), ("Hoenn", 252, 386),
                                 ("Sinnoh", 387, 493), ("Unova", 494, 649)])
        pools = CATALOG["region_pools"](national, national["ordinaryRoots"])
        self.assertEqual({key: len(pool) for key, pool in pools.items()},
                         {"Kanto": 73, "Johto": 51, "Hoenn": 63, "Sinnoh": 38, "Unova": 69})
        self.assertIn(25, pools["Kanto"])
        self.assertIn(172, pools["Johto"])

    def test_ordinary_and_reserved_accounting(self):
        data = LOAD(ROOT / "assets/adventure-649.json")
        ordinary = set(data["ordinaryRoots"])
        self.assertEqual(len(ordinary), 294)
        held = {species for group in data["reserved"].values() for species in group}
        self.assertFalse(ordinary & held)
        self.assertTrue({144, 145, 146, 150, 151, 243, 244, 245, 249, 250, 251,
                         147, 148, 149, 246} <= held)
        progression = json.loads((ROOT / "assets/evolution-649.json").read_text())
        targets = {rule["to"] for rule in progression["rules"]}
        for species in range(1, 650):
            self.assertTrue(species in ordinary or species in held or species in targets,
                            f"#{species} has no acquisition owner")


if __name__ == "__main__":
    unittest.main()
