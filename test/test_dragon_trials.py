"""Validate the fixed dragon trial source and its generated firmware catalog."""
import copy
import json
import runpy
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parent.parent
DRAGON = runpy.run_path(str(ROOT / "tools/build_dragon_trials.py"))
GENERATE = DRAGON["generate"]
SOURCE = DRAGON["SOURCE"]
HEADER = DRAGON["HEADER"]
IMPLEMENTATION = DRAGON["IMPLEMENTATION"]


class DragonTrialCatalogTests(unittest.TestCase):
    def setUp(self):
        self.data = json.loads(SOURCE.read_text())
        self.sha = DRAGON["_sha256"](SOURCE)

    def test_generated_files_are_current(self):
        header, implementation = GENERATE(self.data, self.sha)
        self.assertEqual(header, HEADER.read_text())
        self.assertEqual(implementation, IMPLEMENTATION.read_text())
        self.assertIn(f'"{self.sha}"', implementation)

    def test_deterministic(self):
        first = GENERATE(copy.deepcopy(self.data), self.sha)
        second = GENERATE(copy.deepcopy(self.data), self.sha)
        self.assertEqual(first, second)

    def test_frozen_shape(self):
        stages = DRAGON["validate"](self.data)
        self.assertEqual(stages, [(3, 1500), (4, 1450), (3, 1500), (4, 1450), (5, 1350)])
        self.assertEqual([t["species"] for t in self.data["trials"]], [147, 246])
        self.assertEqual([len(t["stages"]) for t in self.data["trials"]], [2, 3])

    def test_invalid_sources_are_rejected(self):
        mutations = [
            ("missing trial", lambda d: d["trials"].pop()),
            ("wrong trial id", lambda d: d["trials"][0].update(id=2)),
            ("wrong species", lambda d: d["trials"][0].update(species=148)),
            ("extra stage", lambda d: d["trials"][0]["stages"].append({"enemyHp": 5, "beatMs": 1300})),
            ("wrong hp", lambda d: d["trials"][1]["stages"][2].update(enemyHp=4)),
            ("wrong beat", lambda d: d["trials"][0]["stages"][0].update(beatMs=1499)),
            ("wrong cues", lambda d: d.update(cuesPerStage=11)),
            ("wrong player hp", lambda d: d.update(playerHp=4)),
            ("wrong window", lambda d: d.update(normalWindowMs=181)),
            ("wrong telegraph", lambda d: d.update(telegraph="AttackOnly")),
            ("wrong ticket", lambda d: d.update(ticketStep=5)),
            ("wrong bonus step", lambda d: d["upgrades"].update(normalStepMs=20)),
            ("wrong bonus cap", lambda d: d["upgrades"].update(maxPerfectBonusMs=70)),
            ("wrong guard cap", lambda d: d["upgrades"].update(maxGuard=2)),
            ("boolean hp", lambda d: d["trials"][0]["stages"][0].update(enemyHp=True)),
            ("unknown field", lambda d: d.update(extra=1)),
            ("empty note", lambda d: d.update(note="")),
        ]
        for label, mutate in mutations:
            with self.subTest(label=label):
                damaged = copy.deepcopy(self.data)
                mutate(damaged)
                with self.assertRaises(ValueError):
                    GENERATE(damaged, self.sha)


if __name__ == "__main__":
    unittest.main()
