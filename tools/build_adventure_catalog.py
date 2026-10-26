#!/usr/bin/env python3
"""Generate the offline 1-251 gameplay catalog; no sprite/network dependency.

Ordinary encounter eligibility is explicit catalog data. It is no longer derived
from the evolution parent graph, so later-generation pre-evolutions cannot change
which species spawn on the walking routes.
"""
import argparse
import json
import runpy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/adventure-251.json"
OUTPUT = ROOT / "firmware/apps/20_pokedex/adventure_catalog_generated.hpp"
PROGRESSION = runpy.run_path(str(ROOT / "tools/build_evolution_rules.py"))
LAST = 251
FIRST_ROUTE_COUNTS = [38, 22, 24]
RUINS = [144, 145, 146, 147, 148, 149, 150, 151, 243, 244, 245, 249, 250, 251]
DRAGON_CAVE = [246]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def load_catalog(path=SOURCE):
    path = Path(path)
    data = json.loads(path.read_text())
    if not isinstance(data, dict):
        raise ValueError("catalog must be a JSON object")
    files = data.pop("speciesFiles", None)
    if (not isinstance(files, list) or not files
            or any(not isinstance(name, str) or not name for name in files)
            or len(set(files)) != len(files) or "species" in data):
        raise ValueError("catalog requires unique speciesFiles and no inline species")
    species = []
    base = path.parent.resolve()
    for name in files:
        relative = Path(name)
        source = (base / relative).resolve()
        if relative.is_absolute() or not source.is_relative_to(base):
            raise ValueError("species file must stay inside the catalog directory")
        part = json.loads(source.read_text())
        if not isinstance(part, dict) or not isinstance(part.get("species"), list):
            raise ValueError(f"species file must contain a species list: {name}")
        for entry in part["species"]:
            if not isinstance(entry, dict) or type(entry.get("id")) is not int:
                raise ValueError(f"species entry requires an integer ID: {name}")
        species.extend(part["species"])
    data["species"] = sorted(species, key=lambda entry: entry["id"])
    return data


def ownership(data, progression):
    entries = data["species"]
    if data.get("ruleVersion") != 2:
        raise ValueError("unsupported catalog rule version")
    if (len(entries) != LAST or any(type(e["id"]) is not int for e in entries)
            or [e["id"] for e in entries] != list(range(1, LAST + 1))):
        raise ValueError("IDs must be contiguous 1..251")
    masks = {"Forest": 1, "Coast": 2, "Cave": 4}
    pools = {route: set() for route in masks}
    for entry in entries:
        routes = entry["routes"]
        if not routes or len(set(routes)) != len(routes) or any(r not in masks for r in routes):
            raise ValueError(f"invalid routes for {entry['id']}")
        if type(entry["rare"]) is not bool:
            raise ValueError("rarity must be boolean")
        if type(entry["weight"]) is not int or not 1 <= entry["weight"] <= 65535:
            raise ValueError("weight must be positive uint16")
        for route in routes:
            pools[route].add(entry["id"])
    if not all(pools.values()) or set.union(*pools.values()) != set(range(1, LAST + 1)):
        raise ValueError("route pools must be nonempty and cover all 251")
    ordinary = data.get("ordinaryRoots")
    if (not isinstance(ordinary, list) or any(type(i) is not int for i in ordinary)
            or ordinary != sorted(set(ordinary)) or not all(1 <= i <= LAST for i in ordinary)):
        raise ValueError("ordinaryRoots must be sorted unique IDs in 1..251")
    reserved = data.get("reserved")
    if not isinstance(reserved, dict) or set(reserved) != {"ruins", "dragonCave"}:
        raise ValueError("reserved must declare ruins and dragonCave")
    if reserved["ruins"] != RUINS or reserved["dragonCave"] != DRAGON_CAVE:
        raise ValueError("reserved species must match the independent-activity plan")
    held = set(reserved["ruins"]) | set(reserved["dragonCave"])
    if held & set(ordinary):
        raise ValueError("reserved species must stay out of the ordinary pool")
    targets = {rule["to"] for rule in progression["rules"]}
    uncovered = [i for i in range(1, LAST + 1)
                 if i not in ordinary and i not in held and i not in targets]
    if uncovered:
        raise ValueError(f"species without acquisition owner: {uncovered}")
    first = [i for i in ordinary if i <= 151]
    frozen = json.loads((ROOT / "assets/evolution-gen1.json").read_text())["ordinaryRoots"]
    if first != frozen:
        raise ValueError("Kanto ordinary roots must keep the frozen 73-species set")
    ordinary_pools = {route: pools[route] & set(first) for route in masks}
    if [len(ordinary_pools[route]) for route in ("Forest", "Coast", "Cave")] != FIRST_ROUTE_COUNTS:
        raise ValueError("Kanto ordinary route pools must stay 38/22/24")
    if len([i for i in ordinary if i > 151]) != 51:
        raise ValueError("expected 51 Johto ordinary roots")
    parents = progression["parents"]
    if any(parents[str(i)] is not None for i in ordinary if i > 151):
        raise ValueError("Johto ordinary roots must not have a catalog parent")
    return masks, pools, set(ordinary)


def generate(data, progression=None):
    if progression is None:
        progression = json.loads((ROOT / "assets/evolution-251.json").read_text())
    PROGRESSION["validate"](progression)
    masks, _, ordinary = ownership(data, progression)
    entries = data["species"]
    lines = ["// Generated by tools/build_adventure_catalog.py; edit assets/adventure-251.json.",
             "#pragma once", "#include \"encounter_catalog.hpp\"",
             "namespace pokedex { namespace adventure {",
             "static const CatalogEntry kCatalog[251] = {"]
    for entry in entries:
        mask = sum(masks[r] for r in entry["routes"])
        lines.append(f"  {{{mask}, {str(entry['rare']).lower()}, {entry['weight']}}}, // {entry['id']:03d}")
    lines.extend(["};", "static const bool kOrdinaryEncounter[251] = {"])
    for i in range(1, LAST + 1):
        lines.append(f"  {str(i in ordinary).lower()}, // {i:03d}")
    lines.extend(["};", "} }", ""])
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    catalogue = load_catalog()
    progression = json.loads((ROOT / "assets/evolution-251.json").read_text())
    text = generate(catalogue, progression)
    progression_text = PROGRESSION["generate"](progression)
    if args.check:
        if not OUTPUT.exists() or OUTPUT.read_text() != text:
            raise SystemExit("adventure catalog stale; regenerate")
        if not PROGRESSION["OUTPUT"].exists() or PROGRESSION["OUTPUT"].read_text() != progression_text:
            raise SystemExit("progression catalog stale; regenerate tools/build_evolution_rules.py")
        print("catalog check: 251 IDs, 124 ordinary roots (73 Kanto + 51 Johto), "
              "Kanto pools 38/22/24, reserved species excluded, both headers match")
    else:
        OUTPUT.write_text(text)
        print(f"generated {OUTPUT.relative_to(ROOT)}")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError) as error:
        raise SystemExit(f"遭遇目录生成失败：{error}")
