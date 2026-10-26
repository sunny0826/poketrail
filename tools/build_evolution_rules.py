#!/usr/bin/env python3
"""Rebuild the 1-251 progression rules from pinned, local PokeAPI CSVs (no network).

Kanto edges keep their frozen version-group-1 records, costs and order in
assets/evolution-gen1.json; Johto edges are selected from the Gold/Silver
(version_group_id=3) default records. All non-level adaptations cost 10000 steps.
"""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/evolution-source"
DATA = ROOT / "assets/evolution-251.json"
GEN1_DATA = ROOT / "assets/evolution-gen1.json"
OUTPUT = ROOT / "firmware/apps/20_pokedex/progression_catalog_generated.hpp"
REVISION = "f38eb24b70f8462976154f534e79bf2bdd7af847"
FILES = ("pokemon_species.csv", "pokemon_evolution.csv", "evolution_triggers.csv")
LAST = 251
SPECIAL_STEPS = 10000
STEP_PER_LEVEL = 500
# Any non-empty value changes the adaptation, including "0" relative stats.
CONDITION_COLUMNS = ("trigger_item_id", "held_item_id", "time_of_day", "minimum_happiness",
                     "minimum_beauty", "minimum_affection", "relative_physical_stats",
                     "party_species_id", "party_type_id", "trade_species_id", "known_move_id",
                     "known_move_type_id", "used_move_id", "minimum_move_count", "minimum_steps",
                     "minimum_damage_taken", "nature_bitmask", "condition_expression")
FLAG_COLUMNS = ("needs_overworld_rain", "turn_upside_down", "needs_multiplayer", "near_special_rock")


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


def source_parents(tables, last):
    species = {int(r["id"]): r for r in tables["pokemon_species.csv"] if 1 <= int(r["id"]) <= last}
    require(set(species) == set(range(1, last + 1)), "missing species")
    parents = {}
    for i, row in species.items():
        parent = int(row["evolves_from_species_id"] or 0)
        parents[str(i)] = parent if 1 <= parent <= last else None
    require(all(p is None or type(p) is int for p in parents.values()), "invalid parent")
    return species, parents


def edge_row(tables, parent, target, version_group):
    matches = [r for r in tables["pokemon_evolution.csv"]
               if r["evolved_species_id"] == str(target) and r["version_group_id"] == str(version_group)
               and r["is_default"] == "1" and not r["region_id"]
               and r["required_pokemon_form_id"] in ("", str(parent))
               and r["evolved_pokemon_form_id"] in ("", str(target))]
    require(len(matches) == 1,
            f"expected unique version-group {version_group} default edge {parent}->{target}, "
            f"got {len(matches)}")
    return matches[0]


def conditions(row):
    found = [name for name in CONDITION_COLUMNS if row[name] not in ("",)]
    found += [name for name in FLAG_COLUMNS if row[name] == "1"]
    return found


def derive(parents, tables):
    """Frozen Kanto derivation: 72 edges, 52 level and 20 special."""
    species, source = source_parents(tables, 151)
    require(parents == source, "parents disagree with pinned source")
    triggers = {r["id"]: r["identifier"] for r in tables["evolution_triggers.csv"]}
    selected = {}
    for target in range(1, 152):
        parent = parents[str(target)]
        if parent is None:
            continue
        selected[target] = edge_row(tables, parent, target, 1)
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
            steps = (level - entry) * STEP_PER_LEVEL
        else:
            require(level is None, f"unexpected level for special edge {target}")
            steps = SPECIAL_STEPS
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


def kanto():
    """Frozen Gen1 artifact: identical derivation, used only for provenance checks."""
    frozen = json.loads(GEN1_DATA.read_text())
    manifest, tables = load_sources()
    rules, ordinary = derive(frozen["parents"], tables)
    return {**frozen, "ruleVersion": 2, "provenance": manifest, "rules": rules,
            "ordinaryRoots": ordinary}


def derive_251(tables):
    """Kanto edges verbatim, then the 50 Johto edges targeting National Dex 1-251."""
    species, parents = source_parents(tables, LAST)
    triggers = {r["id"]: r["identifier"] for r in tables["evolution_triggers.csv"]}
    kanto_rules = [dict(rule, versionGroup=1) for rule in kanto()["rules"]]
    incoming = {}
    for rule in kanto_rules:
        incoming[rule["to"]] = rule
    new = []
    for target in range(1, LAST + 1):
        parent = parents[str(target)]
        if parent is None or (parent <= 151 and target <= 151):
            continue
        row = edge_row(tables, parent, target, 3)
        trigger = triggers.get(row["evolution_trigger_id"])
        require(trigger in ("level-up", "trade", "use-item"), f"unsupported trigger for {target}")
        level = int(row["minimum_level"]) if row["minimum_level"] else None
        special = conditions(row)
        source = incoming.get(parent)
        require(not (source and source["trigger"] != "level-up" and trigger == "level-up" and not special),
                f"explicit source entry level required for {parent}")
        entry = source["originalLevel"] if source and source["trigger"] == "level-up" else 1
        if trigger == "level-up" and level is not None and not special:
            require(1 <= entry < level <= 100, f"invalid level difference for {target}")
            steps = (level - entry) * STEP_PER_LEVEL
        else:
            require(special and (level is None or trigger != "level-up" or special),
                    f"unsupported special adaptation for {target}")
            steps = SPECIAL_STEPS
        rule = {"from": parent, "to": target, "steps": steps, "trigger": trigger,
                "originalLevel": level, "sourceEntryLevel": entry,
                "sourceRowId": int(row["id"]), "sourceLine": row["csv_line"],
                "speciesSourceLine": species[target]["csv_line"], "versionGroup": 3}
        incoming[target] = rule
        new.append(rule)
    require(len(new) == 50, "expected 50 Johto edges")
    require(sum(r["to"] > 151 for r in new) == 42 and sum(r["to"] <= 151 for r in new) == 8,
            "expected 42 Johto targets and 8 Johto babies")
    rules = kanto_rules + new
    require(len(rules) == 122, "expected 122 edges")
    require(len({r["to"] for r in rules}) == 122, "duplicate evolution target")
    return rules, parents


def build(source=SOURCE):
    manifest, tables = load_sources(source)
    rules, parents = derive_251(tables)
    return {
        "source": "https://raw.githubusercontent.com/PokeAPI/pokeapi/"
                  f"{REVISION}/data/v2/csv/pokemon_species.csv",
        "scope": "Evolution relationships restricted to National Pokedex 1-251. Kanto edges keep "
                 "version_group_id=1; Johto edges use version_group_id=3 default records. "
                 "Every non-level adaptation costs 10000 steps.",
        "parents": parents,
        "ruleVersion": 2,
        "provenance": manifest,
        "rules": rules,
    }


def validate(data, source=SOURCE):
    expected = build(source)
    # JSON comparison rejects bool/float pretending to be an integer.
    require(json.dumps(data, sort_keys=True) == json.dumps(expected, sort_keys=True),
            "progression rules/provenance/parents stale or invalid; regenerate")
    return data


def generate(data):
    lines = ["// Generated by tools/build_evolution_rules.py; do not edit.",
             "#pragma once", '#include "encounter_catalog.hpp"',
             "namespace pokedex { namespace adventure {",
             "static const EvolutionRule kEvolutionRules[] = {"]
    for rule in data["rules"]:
        lines.append("  {%d, %d, %d}," % (rule["from"], rule["to"], rule["steps"]))
    lines.extend(["};", "} }", ""])
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    if args.check:
        require(DATA.exists(), "assets/evolution-251.json missing; regenerate")
        validate(json.loads(DATA.read_text()))
        require(json.loads(GEN1_DATA.read_text()) == kanto(),
                "assets/evolution-gen1.json frozen Kanto artifact drifted")
        require(OUTPUT.exists() and OUTPUT.read_text() == generate(json.loads(DATA.read_text())),
                "progression header stale; regenerate")
        print("progression check: 122 edges (72 Kanto + 50 Johto), 251 parents, "
              "frozen Gen1 artifact and header match")
    else:
        data = build()
        DATA.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        OUTPUT.write_text(generate(data))
        print("generated assets/evolution-251.json and progression_catalog_generated.hpp")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError) as error:
        raise SystemExit(f"进化规则生成失败：{error}")
