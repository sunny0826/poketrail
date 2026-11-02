#!/usr/bin/env python3
"""从固定 PokeAPI CSV 离线生成中文资料和 649 种英文名称、属性。"""

from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/pokedex-source"
DATA = ROOT / "assets/pokedex-gen2.json"
RANGES = ((2, 152, 251), (3, 252, 386), (4, 387, 493), (5, 494, 649))
REVISION = "f38eb24b70f8462976154f534e79bf2bdd7af847"
LANGUAGE = 12
ENGLISH = 9
LANGUAGES = [{"id": LANGUAGE, "identifier": "zh-hans"}, {"id": ENGLISH, "identifier": "en"}]
EN_HEADER = ROOT / "firmware/apps/20_pokedex/pokedex_en_generated.hpp"
EN_SOURCE = EN_HEADER.with_suffix(".cpp")
FILES = ("pokemon.csv", "pokemon_species_names.csv", "pokemon_types.csv", "type_names.csv")
FIRST_ID = 152
LAST_ID = 251
URL_BASE = f"https://raw.githubusercontent.com/PokeAPI/pokeapi/{REVISION}/data/v2/csv"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def load_sources(source: Path = SOURCE) -> tuple[dict, dict[str, list[dict]]]:
    manifest = json.loads((source / "manifest.json").read_text())
    require(manifest.get("revision") == REVISION, "unsupported source revision")
    require(manifest.get("languages") == LANGUAGES, "source languages must be zh-hans and en")
    require(set(manifest["sources"]) == set(FILES), "missing or unexpected source")
    tables = {}
    for name in FILES:
        raw = (source / name).read_bytes()
        meta = manifest["sources"][name]
        require(hashlib.sha256(raw).hexdigest() == meta["sha256"] and
                len(raw) == meta["bytes"], f"source hash/size mismatch: {name}")
        require(meta["url"] == f"{URL_BASE}/{name}", f"source URL mismatch: {name}")
        rows = list(csv.DictReader(io.StringIO(raw.decode("utf-8"))))
        require(rows and rows[0].keys(), f"empty CSV: {name}")
        tables[name] = rows
    return manifest, tables


def localized_names(rows: list[dict], key: str, language: int) -> dict[int, str]:
    names = {}
    for row in rows:
        if row["local_language_id"] != str(language):
            continue
        number = int(row[key])
        require(number not in names, f"duplicate language {language} name for {number}")
        require(bool(row["name"]), f"empty language {language} name for {number}")
        names[number] = row["name"]
    return names


def build_pokemons(tables: dict[str, list[dict]], first: int = FIRST_ID,
                   last: int = LAST_ID, language: int = LANGUAGE) -> list[dict]:
    names = localized_names(tables["pokemon_species_names.csv"], "pokemon_species_id", language)
    type_names = localized_names(tables["type_names.csv"], "type_id", language)
    defaults = {}
    for row in tables["pokemon.csv"]:
        if row["is_default"] != "1":
            continue
        species = int(row["species_id"])
        require(species not in defaults, f"duplicate default form for #{species}")
        defaults[species] = row
    types: dict[int, list[tuple[int, int]]] = {}
    for row in tables["pokemon_types.csv"]:
        types.setdefault(int(row["pokemon_id"]), []).append(
            (int(row["slot"]), int(row["type_id"])))
    pokemons = []
    for number in range(first, last + 1):
        require(number in names, f"#{number}: missing language {language} name")
        require(number in defaults, f"#{number}: missing default form")
        entry = defaults[number]
        slots = sorted(types.get(int(entry["id"]), []))
        require(slots and [slot for slot, _ in slots] == list(range(1, len(slots) + 1)),
                f"#{number}: invalid type slots")
        require(all(type_id in type_names for _, type_id in slots),
                f"#{number}: missing language {language} type")
        height, weight = int(entry["height"]), int(entry["weight"])
        require(height > 0 and weight > 0, f"#{number}: invalid height/weight")
        pokemons.append({
            "zukan_id": str(number),
            "zukan_sub_id": 0,
            "pokemon_name": names[number],
            "pokemon_type_name": ",".join(type_names[t] for _, t in slots),
            "height": f"{height / 10:.1f} m",
            "weight": f"{weight / 10:.1f} kg",
        })
    return pokemons


def build_range(generation: int, first: int, last: int, source: Path = SOURCE) -> dict:
    _, tables = load_sources(source)
    return {
        "source": URL_BASE,
        "revision": REVISION,
        "language": "zh-hans",
        "scope": f"National Pokedex {first}-{last} default forms; Chinese names, types, "
                 "height and weight only.",
        "pokemons": build_pokemons(tables, first, last),
    }


def build(source: Path = SOURCE) -> dict:
    return build_range(2, FIRST_ID, LAST_ID, source)


def validate(data: dict, source: Path = SOURCE) -> dict:
    expected = build(source)
    require(json.dumps(data, sort_keys=True) == json.dumps(expected, sort_keys=True),
            "assets/pokedex-gen2.json stale or invalid; regenerate")
    return data


def english_outputs(source: Path = SOURCE) -> dict[Path, str]:
    _, tables = load_sources(source)
    entries = build_pokemons(tables, 1, 649, ENGLISH)
    header = """// Generated by tools/build_pokedex.py; do not edit.
#pragma once
#include <cstddef>
namespace pokedex { namespace data {
struct SpeciesText { const char* name; const char* types; };
constexpr size_t kEnglishSpeciesCount = 649;
extern const SpeciesText kEnglishSpecies[kEnglishSpeciesCount];
} }
"""
    lines = ["// Generated by tools/build_pokedex.py; do not edit.",
             f"// PokeAPI {REVISION}; language 9 (en).",
             '#include "pokedex_en_generated.hpp"',
             "namespace pokedex { namespace data {",
             "const SpeciesText kEnglishSpecies[kEnglishSpeciesCount] = {"]
    for entry in entries:
        name = json.dumps(entry["pokemon_name"], ensure_ascii=False)
        types = json.dumps(entry["pokemon_type_name"].replace(",", " / "), ensure_ascii=False)
        lines.append(f"  {{{name}, {types}}},")
    lines += ["};", "} }", ""]
    return {EN_HEADER: header, EN_SOURCE: "\n".join(lines)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="校验中文资料、英文名称和属性生成物与固定来源一致")
    args = parser.parse_args()
    outputs = []
    for generation, first, last in RANGES:
        path = ROOT / f"assets/pokedex-gen{generation}.json"
        expected = build_range(generation, first, last)
        if args.check:
            require(path.exists(), f"{path.name} missing; regenerate")
            actual = json.loads(path.read_text())
            require(json.dumps(actual, sort_keys=True) == json.dumps(expected, sort_keys=True),
                    f"{path.name} stale or invalid; regenerate")
        else:
            path.write_text(json.dumps(expected, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        outputs.append((path.name, len(expected["pokemons"])))
    for path, content in english_outputs().items():
        if args.check:
            require(path.exists() and path.read_text(encoding="utf-8") == content,
                    f"{path.name} stale or invalid; regenerate")
        else:
            path.write_text(content, encoding="utf-8")
    outputs.append((EN_SOURCE.name, 649))
    action = "pokedex check ok" if args.check else "generated"
    print(action + ": " + ", ".join(f"{name} {count}" for name, count in outputs))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, KeyError) as error:
        print(f"图鉴资料生成失败：{error}", file=sys.stderr)
        sys.exit(1)
