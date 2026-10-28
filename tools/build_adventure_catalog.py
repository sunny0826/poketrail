#!/usr/bin/env python3
"""Generate the offline 1-251 gameplay catalog; no sprite/network dependency.

Route eligibility is derived from each species' official types through the
`routeTypes` table in assets/adventure-251.json, so the hand-written species
files only carry identity, rarity and weight. The schema-1..4 masks are frozen
here in source form and written to route_masks_v4_generated.hpp so old saves keep
their original eligibility checks after revision 5 re-derives every route.
"""
import argparse
import json
import runpy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/adventure-251.json"
OUTPUT = ROOT / "firmware/apps/20_pokedex/adventure_catalog_generated.hpp"
LEGACY_OUTPUT = ROOT / "firmware/apps/20_pokedex/route_masks_v4_generated.hpp"
TYPES_SOURCE = (ROOT / "assets/pokedex-gen1.json", ROOT / "assets/pokedex-gen2.json")
PROGRESSION = runpy.run_path(str(ROOT / "tools/build_evolution_rules.py"))
LAST = 251
ROUTES = {"Forest": 1, "Coast": 2, "Cave": 4}
RUINS = [144, 145, 146, 147, 148, 149, 150, 151, 243, 244, 245, 249, 250, 251]
DRAGON_CAVE = [246]
# Frozen from baseline 8ea8653 (adventure_catalog_generated.hpp). Schema 1-4
# saves are checked against these masks, never against the derived ones.
FROZEN_V4_ROUTES = [
    1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    3, 3, 4, 4, 1, 1, 1, 1, 1, 1, 6, 6, 1, 1, 1, 1, 6, 6, 1, 1, 1, 1, 1, 1,
    1, 4, 4, 1, 1, 3, 3, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 4, 4, 4, 1, 1, 1, 2,
    2, 6, 6, 6, 1, 1, 3, 3, 4, 4, 1, 1, 1, 2, 2, 4, 4, 2, 2, 4, 4, 4, 4, 1,
    1, 2, 2, 4, 4, 1, 1, 4, 4, 4, 4, 1, 4, 4, 4, 4, 1, 1, 1, 2, 2, 2, 2, 2,
    2, 1, 1, 4, 4, 4, 1, 1, 2, 2, 2, 6, 3, 2, 1, 1, 4, 6, 6, 6, 6, 4, 3, 2,
    4, 4, 3, 2, 2, 4, 3, 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1,
    6, 2, 2, 3, 6, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 4, 2, 1, 1, 1, 1, 1, 1,
    1, 2, 2, 1, 4, 4, 2, 4, 4, 4, 1, 1, 1, 4, 4, 4, 1, 1, 2, 1, 4, 1, 4, 1,
    1, 4, 4, 4, 4, 2, 2, 2, 4, 2, 4, 1, 1, 2, 4, 4, 4, 1, 1, 4, 4, 4, 4, 4,
    1, 1, 4, 4, 2, 4, 4, 4, 2, 1, 1,
]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def load_types():
    types = {}
    for path in TYPES_SOURCE:
        data = json.loads(path.read_text())
        for pokemon in data["pokemons"]:
            species = int(pokemon["zukan_id"])
            require(species not in types, f"duplicate dex entry {species}")
            names = [name.strip() for name in pokemon["pokemon_type_name"].split(",")]
            require(names and all(names), f"missing types for {species}")
            types[species] = names
    require(sorted(types) == list(range(1, LAST + 1)), "dex types must cover 1..251")
    return types


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


def route_table(data):
    table = data.get("routeTypes")
    if not isinstance(table, dict) or not table:
        raise ValueError("catalog requires a routeTypes mapping")
    for type_name, route in table.items():
        if not isinstance(type_name, str) or not type_name:
            raise ValueError("routeTypes keys must be type names")
        if route not in ROUTES:
            raise ValueError(f"unknown route for type {type_name}")
    if set(table.values()) != set(ROUTES):
        raise ValueError("every route needs at least one type")
    return table


def route_masks(data, types):
    table = route_table(data)
    known = {name for names in types.values() for name in names}
    if set(table) != known:
        missing = sorted(known - set(table))
        unknown = sorted(set(table) - known)
        raise ValueError(f"routeTypes must cover every type; missing={missing} unknown={unknown}")
    masks = {}
    for species in range(1, LAST + 1):
        mask = 0
        for name in types[species]:
            mask |= ROUTES[table[name]]
        if not mask:
            raise ValueError(f"species {species} has no route")
        masks[species] = mask
    for route, bit in ROUTES.items():
        derived = {species for species in masks if masks[species] & bit}
        expected = {species for species in range(1, LAST + 1)
                    if any(table[name] == route for name in types[species])}
        if derived != expected:
            raise ValueError(f"derived {route} pool disagrees with routeTypes")
    return masks


def ownership(data, progression):
    entries = data["species"]
    if data.get("ruleVersion") != 3:
        raise ValueError("unsupported catalog rule version")
    if (len(entries) != LAST or any(type(e["id"]) is not int for e in entries)
            or [e["id"] for e in entries] != list(range(1, LAST + 1))):
        raise ValueError("IDs must be contiguous 1..251")
    types = load_types()
    masks = route_masks(data, types)
    for entry in entries:
        if "routes" in entry:
            raise ValueError("species entries must not carry handwritten routes")
        if type(entry["rare"]) is not bool:
            raise ValueError("rarity must be boolean")
        if type(entry["weight"]) is not int or not 1 <= entry["weight"] <= 65535:
            raise ValueError("weight must be positive uint16")
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
    if len([i for i in ordinary if i > 151]) != 51:
        raise ValueError("expected 51 Johto ordinary roots")
    parents = progression["parents"]
    if any(parents[str(i)] is not None for i in ordinary if i > 151):
        raise ValueError("Johto ordinary roots must not have a catalog parent")
    if len(FROZEN_V4_ROUTES) != LAST or any(not 1 <= mask <= 7 for mask in FROZEN_V4_ROUTES):
        raise ValueError("frozen schema-4 masks must cover 251 species")
    return masks, set(ordinary)


def generate(data, progression=None):
    if progression is None:
        progression = json.loads((ROOT / "assets/evolution-251.json").read_text())
    PROGRESSION["validate"](progression)
    masks, ordinary = ownership(data, progression)
    entries = data["species"]
    lines = ["// Generated by tools/build_adventure_catalog.py; edit assets/adventure-251.json.",
             "#pragma once", "#include \"encounter_catalog.hpp\"",
             "namespace pokedex { namespace adventure {",
             "static const CatalogEntry kCatalog[251] = {"]
    for entry in entries:
        lines.append(f"  {{{masks[entry['id']]}, {str(entry['rare']).lower()}, {entry['weight']}}},"
                     f" // {entry['id']:03d}")
    lines.extend(["};", "static const bool kOrdinaryEncounter[251] = {"])
    for i in range(1, LAST + 1):
        lines.append(f"  {str(i in ordinary).lower()}, // {i:03d}")
    lines.extend(["};", "} }", ""])
    return "\n".join(lines)


def generate_legacy_masks():
    lines = ["// Frozen from baseline 8ea8653; schema 1-4 route eligibility.",
             "// Generated by tools/build_adventure_catalog.py; do not edit by hand.",
             "#pragma once", "#include <cstdint>",
             "namespace pokedex { namespace adventure {",
             "static const uint8_t kRouteMaskV4[251] = {"]
    for start in range(0, LAST, 12):
        row = FROZEN_V4_ROUTES[start:start + 12]
        lines.append("  " + " ".join(f"{mask}," for mask in row))
    lines.extend(["};", "} }", ""])
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    catalogue = load_catalog()
    progression = json.loads((ROOT / "assets/evolution-251.json").read_text())
    text = generate(catalogue, progression)
    legacy_text = generate_legacy_masks()
    progression_text = PROGRESSION["generate"](progression)
    if args.check:
        if not OUTPUT.exists() or OUTPUT.read_text() != text:
            raise SystemExit("adventure catalog stale; regenerate")
        if not LEGACY_OUTPUT.exists() or LEGACY_OUTPUT.read_text() != legacy_text:
            raise SystemExit("frozen schema-4 route masks stale; regenerate")
        if not PROGRESSION["OUTPUT"].exists() or PROGRESSION["OUTPUT"].read_text() != progression_text:
            raise SystemExit("progression catalog stale; regenerate tools/build_evolution_rules.py")
        print("catalog check: 251 typed route masks, 124 ordinary roots "
              "(73 Kanto + 51 Johto), reserved species excluded, both headers match")
    else:
        OUTPUT.write_text(text)
        LEGACY_OUTPUT.write_text(legacy_text)
        print(f"generated {OUTPUT.relative_to(ROOT)} and {LEGACY_OUTPUT.relative_to(ROOT)}")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError) as error:
        raise SystemExit(f"遭遇目录生成失败：{error}")
