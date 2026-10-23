"""Validate the encounter catalog and its generated representation offline."""
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


class AdventureCatalogTests(unittest.TestCase):
    def setUp(self):
        self.data = LOAD()

    def test_deterministic_generated_header(self):
        generated = GENERATE(self.data)
        self.assertEqual(generated, GENERATE(copy.deepcopy(self.data)))
        self.assertEqual(generated, (ROOT / "firmware/apps/20_pokedex/adventure_catalog_generated.hpp").read_text())

    def test_invalid_catalogs_are_rejected(self):
        mutations = [
            ("missing ID", lambda d: d["species"].pop()),
            ("duplicate ID", lambda d: d["species"][1].update(id=1)),
            ("boolean ID", lambda d: d["species"][0].update(id=True)),
            ("zero weight", lambda d: d["species"][0].update(weight=0)),
            ("negative weight", lambda d: d["species"][0].update(weight=-1)),
            ("weight overflow", lambda d: d["species"][0].update(weight=65536)),
            ("invalid rarity", lambda d: d["species"][0].update(rare=1)),
            ("unknown route", lambda d: d["species"][0].update(routes=["Mountain"])),
            ("empty route", lambda d: d["species"][0].update(routes=[])),
            ("duplicate route", lambda d: d["species"][0].update(routes=["Forest", "Forest"])),
            ("empty pools", lambda d: [e.update(routes=["Forest"]) for e in d["species"]]),
            ("ordinary route mismatch", lambda d: d["species"][0].update(routes=["Cave"])),
            ("unknown rule", lambda d: d.update(ruleVersion=2)),
        ]
        for label, mutate in mutations:
            with self.subTest(label=label):
                damaged = copy.deepcopy(self.data)
                mutate(damaged)
                with self.assertRaises(ValueError):
                    GENERATE(damaged)

    def test_partition_order_does_not_change_generated_catalog(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            manifest = {"ruleVersion": 1, "speciesFiles": ["even.json", "odd.json"]}
            (root / "catalog.json").write_text(json.dumps(manifest))
            for name, species in (("even.json", self.data["species"][1::2]),
                                  ("odd.json", self.data["species"][::2])):
                (root / name).write_text(json.dumps({"species": list(reversed(species))}))
            self.assertEqual(GENERATE(LOAD(root / "catalog.json")), GENERATE(self.data))

    def test_invalid_partition_inputs_are_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            path = root / "catalog.json"
            manifests = [
                [], None,
                {"ruleVersion": 1},
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
