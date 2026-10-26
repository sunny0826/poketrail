#!/usr/bin/env python3
"""用固定提交的本地 PokeAPI CSV 生成第二世代中文资料（完全离线）。"""

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
REVISION = "f38eb24b70f8462976154f534e79bf2bdd7af847"
LANGUAGE = 12
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
    require(manifest.get("language") == {"id": LANGUAGE, "identifier": "zh-hans"},
            "source language must be zh-hans")
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


def chinese_names(rows: list[dict], key: str) -> dict[int, str]:
    names = {}
    for row in rows:
        if row["local_language_id"] != str(LANGUAGE):
            continue
        number = int(row[key])
        require(number not in names, f"duplicate Chinese name for {number}")
        names[number] = row["name"]
    return names


def build_pokemons(tables: dict[str, list[dict]]) -> list[dict]:
    names = chinese_names(tables["pokemon_species_names.csv"], "pokemon_species_id")
    type_names = chinese_names(tables["type_names.csv"], "type_id")
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
    for number in range(FIRST_ID, LAST_ID + 1):
        require(number in names, f"#{number}: missing zh-hans name")
        require(number in defaults, f"#{number}: missing default form")
        slots = sorted(types.get(number, []))
        require(slots and [slot for slot, _ in slots] == list(range(1, len(slots) + 1)),
                f"#{number}: invalid type slots")
        require(all(type_id in type_names for _, type_id in slots),
                f"#{number}: missing zh-hans type")
        entry = defaults[number]
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


def build(source: Path = SOURCE) -> dict:
    _, tables = load_sources(source)
    return {
        "source": URL_BASE,
        "revision": REVISION,
        "language": "zh-hans",
        "scope": "National Pokedex 152-251 default forms; Chinese names, types, "
                 "height and weight only.",
        "pokemons": build_pokemons(tables),
    }


def validate(data: dict, source: Path = SOURCE) -> dict:
    expected = build(source)
    require(json.dumps(data, sort_keys=True) == json.dumps(expected, sort_keys=True),
            "assets/pokedex-gen2.json stale or invalid; regenerate")
    return data


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="只校验 assets/pokedex-gen2.json 与固定来源一致")
    args = parser.parse_args()
    if args.check:
        data = validate(json.loads(DATA.read_text()))
        print(f"pokedex check ok: {len(data['pokemons'])} species, revision {REVISION}")
        return 0
    data = build()
    DATA.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"generated {DATA.relative_to(ROOT)}: {len(data['pokemons'])} species")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, KeyError) as error:
        print(f"图鉴资料生成失败：{error}", file=sys.stderr)
        sys.exit(1)
