"""Offline product costs, source integrity and real C++ catalog behavior."""
import json
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ProgressionCatalogTests(unittest.TestCase):
    def setUp(self):
        self.data = json.loads((ROOT / "assets/evolution-649.json").read_text())

    def test_rules_and_costs(self):
        rules = self.data["rules"]
        self.assertEqual(len(rules), 320)
        costs = {(r["from"], r["to"]): r["steps"] for r in rules}
        for edge, cost in {(1, 2): 7500, (2, 3): 8000, (5, 6): 10000,
                           (10, 11): 3000, (11, 12): 1500, (129, 130): 9500,
                           (147, 148): 14500, (148, 149): 12500,
                           (133, 134): 10000, (133, 135): 10000,
                           (133, 136): 10000,
                           (152, 153): 7500, (153, 154): 8000, (156, 157): 11000,
                           (188, 189): 4500, (204, 205): 15000,
                           (172, 25): 10000, (42, 169): 10000, (44, 182): 10000,
                           (133, 196): 10000, (133, 197): 10000, (95, 208): 10000,
                           (113, 242): 10000, (238, 124): 14500,
                           (246, 247): 14500, (247, 248): 12500,
                           (290, 291): 9500, (290, 292): 10000}.items():
            self.assertEqual(costs[edge], cost)
        for target in (106, 107, 237):
            self.assertEqual(costs[(236, target)], 10000)
        self.assertEqual(sorted(rule["to"] for rule in rules if rule["from"] == 133),
                         [134, 135, 136, 196, 197, 470, 471])

    def compile_and_run(self, name, text):
        with tempfile.TemporaryDirectory(prefix="poketrail-catalog-") as directory:
            source = Path(directory) / f"{name}.cpp"
            source.write_text(text)
            binary = Path(directory) / name
            subprocess.run([os.environ.get("CXX", "c++"), "-std=c++11", "-Wall", "-Wextra", "-Werror",
                            "-Ifirmware/apps/20_pokedex", str(source),
                            "firmware/apps/20_pokedex/encounter_catalog.cpp",
                            "firmware/apps/20_pokedex/adventure_model.cpp",
                            "firmware/apps/20_pokedex/calendar.cpp", "-o", str(binary)],
                           cwd=ROOT, check=True)
            subprocess.run([str(binary)], cwd=ROOT, check=True)

    def test_cpp_ordinary_sampling(self):
        ordinary = json.loads((ROOT / "assets/adventure-649.json").read_text())["ordinaryRoots"]
        self.compile_and_run("sampling", r'''
#include "encounter_catalog.hpp"
#include <cassert>
using namespace pokedex::adventure;
int main() {
  const unsigned roots[] = {ROOTS};
  const unsigned first[] = {1, 152, 252, 387, 494}, last[] = {151, 251, 386, 493, 649};
  bool expected[650] = {};
  for (unsigned id : roots) expected[id] = true;
  Save s;
  for (unsigned region = 0; region < 5; ++region) {
    bool seen[650] = {};
    for (unsigned seed = 0; seed < 20000; ++seed) {
      const unsigned got = chooseEncounter(s, Route(region + 3), seed);
      assert(got >= first[region] && got <= last[region] && expected[got]);
      seen[got] = true;
    }
    for (unsigned id = first[region]; id <= last[region]; ++id)
      if (expected[id]) assert(seen[id]);
  }
  for (unsigned route : {0u, 1u, 2u, 8u}) assert(chooseEncounter(s, Route(route), 42) == 0);
}
'''.replace("ROOTS", ",".join(map(str, ordinary))))

    def test_corrupt_source_hash_fails(self):
        gen = runpy.run_path(str(ROOT / "tools/build_evolution_rules.py"))
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "source"
            shutil.copytree(gen["SOURCE"], source)
            path = source / "pokemon_evolution.csv"
            path.write_bytes(path.read_bytes() + b"\n")
            with self.assertRaises(ValueError):
                gen["load_sources"](source)

    def test_cpp_rule_api(self):
        cases = "\n".join("  {%d, %d, %d}," % (r["from"], r["to"], r["steps"])
                          for r in self.data["rules"])
        self.compile_and_run("rule-api", r'''
#include "encounter_catalog.hpp"
#include <cassert>
using namespace pokedex::adventure;
int main() {
  const EvolutionRule expected[] = {CASES};
  for (const auto& rule : expected) {
    const auto* got = evolutionRule(rule.from, rule.to);
    assert(got && got->steps == rule.steps && got->from == rule.from && got->to == rule.to);
  }
  for (unsigned from = 0; from <= 252; ++from) {
    for (unsigned to = 0; to <= 252; ++to) {
      const EvolutionRule* want = nullptr;
      for (const auto& rule : expected)
        if (rule.from == from && rule.to == to) want = &rule;
      const auto* got = evolutionRule(from, to);
      assert(bool(want) == bool(got));
      if (got) assert(got->steps == want->steps && got->from == from && got->to == to);
    }
  }
  assert(!evolutionRule(65535, 1) && !evolutionRule(1, 65535));
}
'''.replace("CASES", cases))


if __name__ == "__main__":
    unittest.main()
