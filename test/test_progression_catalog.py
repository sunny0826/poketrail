"""Offline rule provenance and real C++ encounter behavior checks."""
import copy
import json
import os
from pathlib import Path
import runpy
import subprocess
import tempfile
import shutil
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ProgressionCatalogTests(unittest.TestCase):
    def setUp(self):
        self.data = json.loads((ROOT / "assets/evolution-gen1.json").read_text())

    def test_rules_and_costs(self):
        rules = self.data.get("rules", [])
        self.assertEqual(len(rules), 72)
        self.assertEqual(sum(r["trigger"] == "level-up" for r in rules), 52)
        costs = {(r["from"], r["to"]): r["steps"] for r in rules}
        for edge, cost in {(1, 2): 7500, (2, 3): 8000, (5, 6): 10000,
                           (10, 11): 3000, (11, 12): 1500, (129, 130): 9500,
                           (147, 148): 14500, (148, 149): 12500,
                           (133, 134): 10000, (133, 135): 10000,
                           (133, 136): 10000}.items():
            self.assertEqual(costs[edge], cost)

    def test_cpp_ordinary_sampling(self):
        ordinary = [int(i) for i, p in self.data["parents"].items()
                    if p is None and int(i) < 144]
        self.assertEqual(len(ordinary), 73)
        output = ROOT / ".pio/catalog-checks"
        output.mkdir(parents=True, exist_ok=True)
        source = output / "sampling.cpp"
        source.write_text(r'''
#include "encounter_catalog.hpp"
#include <cassert>
#include <cstdio>
using namespace pokedex::adventure;
int main() {
  const unsigned roots[] = {ROOTS};
  bool expected[152] = {};
  for (unsigned id : roots) expected[id] = true;
  Save s;
  for (unsigned mode = 0; mode < 3; ++mode) {
    for (unsigned id = 1; id <= 151; ++id)
      if (mode == 2 || (mode == 1 && id % 2)) setOwned(s, id);
    for (unsigned route = 0; route < 3; ++route) {
      unsigned total = 0, pool = 0;
      bool seen[152] = {};
      for (unsigned id = 1; id <= 151; ++id) {
        const auto* e = catalogEntry(id);
        assert(e && e->weight > 0);
        if (expected[id] && (e->routes & (1u << route))) {
          total += e->weight * (owns(s, id) ? 1 : 3); ++pool;
        }
      }
      const unsigned sizes[] = {38, 22, 24};
      assert(pool == sizes[route]);
      for (unsigned seed = 0; seed < 20000; ++seed) {
        unsigned draw = mixRandom(seed) % total, want = 0;
        for (unsigned id = 1; id <= 151; ++id) {
          const auto* e = catalogEntry(id);
          if (!expected[id] || !(e->routes & (1u << route))) continue;
          const unsigned weight = e->weight * (owns(s, id) ? 1 : 3);
          if (draw < weight) { want = id; break; }
          draw -= weight;
        }
        const unsigned got = chooseEncounter(s, Route(route), seed);
        assert(got == want && expected[got]); seen[got] = true;
      }
      for (unsigned id = 1; id <= 151; ++id)
        if (expected[id] && (catalogEntry(id)->routes & (1u << route))) assert(seen[id]);
    }
  }
  assert(!catalogEntry(0) && !catalogEntry(152));
  assert(chooseEncounter(s, Route(3), 42) == 0);
  puts("180000 deterministic weighted draws; 73 roots; pools 38/22/24 passed");
}
'''.replace("ROOTS", ",".join(map(str, ordinary))))
        binary = output / "sampling"
        subprocess.run([os.environ.get("CXX", "c++"), "-std=c++11", "-Wall", "-Wextra", "-Werror",
                        "-Ifirmware/apps/20_pokedex", str(source),
                        "firmware/apps/20_pokedex/encounter_catalog.cpp",
                        "firmware/apps/20_pokedex/adventure_model.cpp",
                        "firmware/apps/20_pokedex/calendar.cpp", "-o", str(binary)],
                       cwd=ROOT, check=True)
        subprocess.run([str(binary)], cwd=ROOT, check=True)

    def generator(self):
        return runpy.run_path(str(ROOT / "tools/build_evolution_rules.py"))

    def test_pinned_derivation_and_generated_header(self):
        gen = self.generator()
        gen["validate"](self.data)
        self.assertEqual(gen["generate"](self.data), gen["OUTPUT"].read_text())

    def test_invalid_product_rules_fail(self):
        validate = self.generator()["validate"]
        for mutate in [lambda d: d["rules"].pop(),
                       lambda d: d["rules"].append(d["rules"][0]),
                       lambda d: d["rules"][0].update(steps=7501),
                       lambda d: d["rules"][0].update(steps=7500.0),
                       lambda d: d["rules"][0].update(sourceLine=1),
                       lambda d: d["parents"].update({"2": 4}),
                       lambda d: d["parents"].update({"2": True}),
                       lambda d: d["ordinaryRoots"].append(147),
                       lambda d: d.update(ruleVersion=1),
                       lambda d: d["provenance"].update(revision="unknown")]:
            damaged = copy.deepcopy(self.data)
            mutate(damaged)
            with self.assertRaises(ValueError):
                validate(damaged)

    def test_csv_constraints_and_ambiguity_fail(self):
        gen = self.generator()
        _, original = gen["load_sources"]()
        def edge(tables):
            return next(r for r in tables["pokemon_evolution.csv"] if r["id"] == "1")
        mutations = [lambda t: t["pokemon_evolution.csv"].append(copy.deepcopy(edge(t))),
                     lambda t: t["pokemon_evolution.csv"].remove(edge(t)),
                     lambda t: edge(t).update(version_group_id="2"),
                     lambda t: edge(t).update(is_default="0"),
                     lambda t: edge(t).update(region_id="1"),
                     lambda t: edge(t).update(required_pokemon_form_id="10001"),
                     lambda t: edge(t).update(evolved_pokemon_form_id="10002"),
                     lambda t: edge(t).update(minimum_level=""),
                     lambda t: edge(t).update(minimum_level="1"),
                     lambda t: edge(t).update(evolution_trigger_id="18"),
                     lambda t: edge(t).update(evolution_trigger_id="2", minimum_level="")]
        for mutate in mutations:
            tables = copy.deepcopy(original)
            mutate(tables)
            with self.assertRaises(ValueError):
                gen["derive"](self.data["parents"], tables)

    def test_corrupt_source_hash_fails(self):
        gen = self.generator()
        output = ROOT / ".pio/catalog-checks"
        output.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=output) as temp:
            source = Path(temp) / "source"
            shutil.copytree(gen["SOURCE"], source)
            path = source / "pokemon_evolution.csv"
            path.write_bytes(path.read_bytes() + b"\n")
            with self.assertRaises(ValueError):
                gen["load_sources"](source)

    def test_cpp_rule_api(self):
        output = ROOT / ".pio/catalog-checks"
        output.mkdir(parents=True, exist_ok=True)
        # Each expected edge is read from the validated public data, not the C++ header.
        cases = "\n".join("  {%d, %d, %d}," % (r["from"], r["to"], r["steps"])
                          for r in self.data["rules"])
        ordinary = ",".join(map(str, self.data["ordinaryRoots"]))
        source = output / "rule-api.cpp"
        source.write_text(r'''
#include "encounter_catalog.hpp"
#include <cassert>
#include <cstdio>
using namespace pokedex::adventure;
int main() {
  const EvolutionRule expected[] = {CASES};
  const unsigned roots[] = {ROOTS};
  bool eligible[152] = {};
  for (unsigned id : roots) eligible[id] = true;
  for (unsigned id = 0; id <= 65535; ++id)
    assert(ordinaryEncounter(id) == (id <= 151 && eligible[id]));
  size_t count = 0, again = 0;
  const auto* all = evolutionRules(count);
  assert(count == 72 && evolutionRules(again) == all && again == count);
  for (unsigned from = 0; from <= 152; ++from) {
    for (unsigned to = 0; to <= 152; ++to) {
      const EvolutionRule* want = nullptr;
      for (const auto& rule : expected)
        if (rule.from == from && rule.to == to) want = &rule;
      const auto* got = evolutionRule(from, to);
      assert(bool(want) == bool(got));
      if (got) {
        assert(got->steps == want->steps && got->from == from && got->to == to);
        bool stable = false;
        for (size_t i = 0; i < count; ++i) if (&all[i] == got) stable = true;
        assert(stable);
      }
    }
  }
  assert(!evolutionRule(65535, 1) && !evolutionRule(1, 65535));
  puts("72 rule costs; all ID pairs; 65536 eligibility inputs; stable pointers passed");
}
'''.replace("CASES", cases).replace("ROOTS", ordinary))
        binary = output / "rule-api"
        subprocess.run([os.environ.get("CXX", "c++"), "-std=c++11", "-Wall", "-Wextra", "-Werror",
                        "-Ifirmware/apps/20_pokedex", str(source),
                        "firmware/apps/20_pokedex/encounter_catalog.cpp",
                        "firmware/apps/20_pokedex/adventure_model.cpp",
                        "firmware/apps/20_pokedex/calendar.cpp", "-o", str(binary)],
                       cwd=ROOT, check=True)
        subprocess.run([str(binary)], cwd=ROOT, check=True)


if __name__ == "__main__":
    unittest.main()
