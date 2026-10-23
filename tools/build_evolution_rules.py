#!/usr/bin/env python3
"""Rebuild Gen 1 progression from pinned, local PokeAPI CSVs (no network)."""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/evolution-source"
DATA = ROOT / "assets/evolution-gen1.json"
OUTPUT = ROOT / "firmware/apps/20_pokedex/progression_catalog_generated.hpp"
REVISION = "f38eb24b70f8462976154f534e79bf2bdd7af847"
FILES = ("pokemon_species.csv", "pokemon_evolution.csv", "evolution_triggers.csv")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def load_sources(source=SOURCE):
    manifest = json.loads((source / "manifest.json").read_text())
    require(manifest.get("revision") == REVISION, "unsupported source revision")
    require(set(manifest["sources"]) == set(FILES), "missing or unexpected source")
    tables = {}
    for name in FILES:
        raw = (source / name).read_bytes()
        meta = manifest["sources"][name]
        require(hashlib.sha256(raw).hexdigest() == meta["sha256"] and
                len(raw) == meta["bytes"], f"source hash/size mismatch: {name}")
        require(meta["url"] == f"https://raw.githubusercontent.com/PokeAPI/pokeapi/{REVISION}/data/v2/csv/{name}",
                f"source URL mismatch: {name}")
        reader = csv.DictReader(io.StringIO(raw.decode("utf-8")))
        rows = []
        ids = set()
        for row in reader:
            require(row["id"] not in ids, f"duplicate CSV ID: {name}/{row['id']}")
            ids.add(row["id"])
            row["csv_line"] = reader.line_num
            rows.append(row)
        tables[name] = rows
    return manifest, tables


def derive(parents, tables):
    species = {int(r["id"]): r for r in tables["pokemon_species.csv"] if 1 <= int(r["id"]) <= 151}
    require(set(species) == set(range(1, 152)), "missing species")
    source_parents = {}
    for i, row in species.items():
        parent = int(row["evolves_from_species_id"] or 0)
        source_parents[str(i)] = parent if 1 <= parent <= 151 else None
    require(parents == source_parents and all(p is None or type(p) is int for p in parents.values()),
            "parents disagree with pinned source")
    triggers = {r["id"]: r["identifier"] for r in tables["evolution_triggers.csv"]}
    selected = {}
    for target in range(1, 152):
        parent = parents[str(target)]
        if parent is None:
            continue
        matches = [r for r in tables["pokemon_evolution.csv"]
                   if r["evolved_species_id"] == str(target) and r["version_group_id"] == "1"
                   and r["is_default"] == "1" and not r["region_id"]
                   and r["required_pokemon_form_id"] in ("", str(parent))
                   and r["evolved_pokemon_form_id"] in ("", str(target))]
        require(len(matches) == 1, f"expected unique Kanto default edge to {target}, got {len(matches)}")
        selected[target] = matches[0]
    rules = []
    for target, row in selected.items():
        parent = parents[str(target)]
        trigger = triggers.get(row["evolution_trigger_id"])
        require(trigger in ("level-up", "trade", "use-item"), f"unsupported trigger for {target}")
        level = int(row["minimum_level"]) if row["minimum_level"] else None
        entry = 1
        if parent in selected:
            incoming = selected[parent]
            require(triggers[incoming["evolution_trigger_id"]] == "level-up" and incoming["minimum_level"],
                    f"explicit source entry level required for {parent}")
            entry = int(incoming["minimum_level"])
        if trigger == "level-up":
            require(level is not None and 1 <= entry < level <= 100, f"invalid level difference for {target}")
            steps = (level - entry) * 500
        else:
            require(level is None, f"unexpected level for special edge {target}")
            steps = 10000
        rules.append({"from": parent, "to": target, "steps": steps, "trigger": trigger,
                      "originalLevel": level, "sourceEntryLevel": entry,
                      "sourceRowId": int(row["id"]), "sourceLine": row["csv_line"],
                      "speciesSourceLine": species[target]["csv_line"]})
    require(len(rules) == 72 and sum(r["trigger"] == "level-up" for r in rules) == 52,
            "expected 72 edges: 52 level and 20 special")
    ordinary = [i for i in range(1, 144) if parents[str(i)] is None]
    require(len(ordinary) == 73, "expected 73 ordinary roots")
    reachable = set(ordinary)
    while True:
        expanded = reachable | {r["to"] for r in rules if r["from"] in reachable}
        if expanded == reachable:
            break
        reachable = expanded
    require(reachable == set(range(1, 144)), "ordinary evolution must cover exactly 143 species")
    return rules, ordinary


def build(data, source=SOURCE):
    manifest, tables = load_sources(source)
    rules, ordinary = derive(data["parents"], tables)
    return {**data, "ruleVersion": 2, "provenance": manifest, "rules": rules,
            "ordinaryRoots": ordinary}


def validate(data, source=SOURCE):
    expected = build(data, source)
    # JSON comparison rejects bool/float pretending to be an integer.
    require(json.dumps(data, sort_keys=True) == json.dumps(expected, sort_keys=True),
            "progression rules/provenance/ordinary roots stale or invalid; regenerate")
    return data


def generate(data):
    lines = ["// Generated by tools/build_evolution_rules.py; do not edit.",
             "#pragma once", '#include "encounter_catalog.hpp"',
             "namespace pokedex { namespace adventure {",
             "static const EvolutionRule kEvolutionRules[] = {"]
    for rule in data["rules"]:
        lines.append("  {%d, %d, %d}," % (rule["from"], rule["to"], rule["steps"]))
    lines.extend(["};", "static const bool kOrdinaryEncounter[151] = {"])
    ordinary = set(data["ordinaryRoots"])
    for i in range(1, 152):
        lines.append(f"  {str(i in ordinary).lower()}, // {i:03d}")
    lines.extend(["};", "} }", ""])
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    data = json.loads(DATA.read_text())
    if args.check:
        validate(data)
        require(OUTPUT.exists() and OUTPUT.read_text() == generate(data), "progression header stale; regenerate")
        print("progression check: 72 edges (52 level/20 special), 73 roots, 143 reachable, source hashes and header match")
    else:
        data = build(data)
        DATA.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n")
        OUTPUT.write_text(generate(data))
        print("generated assets/evolution-gen1.json and progression_catalog_generated.hpp")


if __name__ == "__main__":
    main()
