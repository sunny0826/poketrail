#!/usr/bin/env python3
"""把黑／白 GIF 完整合成、无损去重，生成带原始时序的 RGB565 索引动画。"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import sys
import urllib.request
import zlib
from pathlib import Path

from PIL import Image

REPO_ROOT = Path(__file__).resolve().parent.parent
OUT_HEADER = REPO_ROOT / "firmware/apps/20_pokedex/sprites_generated.hpp"
CACHE_DIR = REPO_ROOT / "assets/sprites/black-white"
METADATA_CACHE = tuple(REPO_ROOT / f"assets/pokedex-gen{generation}.json"
                      for generation in range(1, 6))
SCALE_CACHE = REPO_ROOT / "assets/sprite-scales.json"
MANIFESTS = {generation: REPO_ROOT / f"assets/animations-gen{generation}.json"
             for generation in range(1, 6)}
RANGES = {1: (1, 151), 2: (152, 251), 3: (252, 386), 4: (387, 493), 5: (494, 649)}
GEN1_COUNT = 151
POKEMON_COUNT = 649
SPRITE_REVISION = "97a7832d307311c0e1c6ea9035308bf80cc94f81"
SPRITE_URL = (
    f"https://raw.githubusercontent.com/PokeAPI/sprites/{SPRITE_REVISION}/"
    "sprites/pokemon/versions/generation-v/black-white/animated/{id}.gif"
)
TRANSPARENT_KEY = 0xF81F
MAX_CANVAS = 160


def generation_of(number: int) -> int:
    for generation, (first, last) in RANGES.items():
        if first <= number <= last:
            return generation
    raise ValueError(f"#{number} is outside National Dex 1-649")


def requires_separate_output(wanted: list[int], has_ids: bool) -> bool:
    """非完整 1-649 输出（或显式 --ids）不得写入正式动画表。"""
    return has_ids or sorted(wanted) != list(range(1, POKEMON_COUNT + 1))


def fetch(number: int, expected_hash: str | None = None) -> bytes:
    path = CACHE_DIR / f"{number:04d}.gif"
    if path.exists():
        data = path.read_bytes()
    else:
        request = urllib.request.Request(SPRITE_URL.format(id=number),
                                         headers={"User-Agent": "m5-pokedex/1.0"})
        with urllib.request.urlopen(request, timeout=30) as response:
            data = response.read()
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    if expected_hash and hashlib.sha256(data).hexdigest() != expected_hash:
        raise ValueError(f"#{number} GIF differs from its animations manifest")
    return data


def to_rgb565(r: int, g: int, b: int) -> int:
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def frame_colors(image: Image.Image) -> tuple[int, ...]:
    result = []
    for r, g, b, a in image.convert("RGBA").get_flattened_data():
        if a < 128:
            result.append(TRANSPARENT_KEY)
        else:
            color = to_rgb565(r, g, b)
            if color == TRANSPARENT_KEY:
                raise ValueError("opaque pixel collides with transparent key")
            result.append(color)
    return tuple(result)


def build_animation(data: bytes) -> dict:
    image = Image.open(io.BytesIO(data))
    width, height = image.size
    if image.format != "GIF" or not (0 < width <= MAX_CANVAS and 0 < height <= MAX_CANVAS):
        raise ValueError(f"unsupported GIF canvas {width}x{height}")
    palette = [TRANSPARENT_KEY]
    lookup = {TRANSPARENT_KEY: 0}
    unique = {}
    blob = bytearray()
    frames = []
    for i in range(image.n_frames):
        image.seek(i)  # Pillow 合成局部帧，并处理透明与 disposal。
        duration = image.info.get("duration", 0)
        if not 0 < duration <= 65535:
            raise ValueError(f"frame {i}: invalid duration {duration}")
        colors = frame_colors(image)
        for color in colors:
            if color not in lookup:
                if len(palette) == 256:
                    raise ValueError("animation needs more than 256 colors")
                lookup[color] = len(palette)
                palette.append(color)
        indices = bytes(lookup[color] for color in colors)
        if indices not in unique:
            unique[indices] = len(blob)
            blob.extend(indices)
        frames.append((unique[indices], duration))
    if len(unique) < 2:
        raise ValueError("animation must contain at least two distinct frames")
    raw = bytes(blob)
    compressed = zlib.compress(raw, 9)
    if zlib.decompress(compressed) != raw:
        raise ValueError("animation compression did not round-trip")
    return dict(width=width, height=height, palette=palette, data=compressed,
                raw_size=len(raw), frames=frames, unique=len(unique),
                duration=sum(frame[1] for frame in frames),
                sha256=hashlib.sha256(data).hexdigest())


def _unique_ids(entry: dict, key: str, field: str, first: int, last: int) -> set[int]:
    ids = entry.get(field)
    if not isinstance(ids, list) or not all(type(i) is int for i in ids):
        raise ValueError(f"generation {key}: {field} must be an integer list")
    if len(set(ids)) != len(ids) or any(not first <= i <= last for i in ids):
        raise ValueError(f"generation {key}: invalid {field} IDs")
    return set(ids)


def load_scales(path: Path = SCALE_CACHE) -> set[int]:
    """返回优先 3 倍的编号：中间／最终形态与无进化链物种。"""
    data = json.loads(path.read_text())["generations"]
    if set(data) != {str(generation) for generation in RANGES}:
        raise ValueError("sprite scales must describe generations 1 through 5")
    enlarged = set()
    for key, (first, last) in ((str(generation), bounds) for generation, bounds in RANGES.items()):
        entry = data[key]
        if (entry.get("first"), entry.get("last")) != (first, last):
            raise ValueError(f"generation {key}: unexpected ID range")
        evolved = _unique_ids(entry, key, "evolved", first, last)
        standalone = _unique_ids(entry, key, "standalone", first, last)
        if evolved & standalone:
            raise ValueError(f"generation {key}: standalone and evolved must not overlap")
        base = _unique_ids(entry, key, "base", first, last) if "base" in entry else None
        if base is not None and base & (evolved | standalone):
            raise ValueError(f"generation {key}: base must not overlap evolved or standalone")
        if key != "1":
            expected = set(range(first, last + 1)) - evolved - standalone
            if base != expected:
                raise ValueError(
                    f"generation {key}: base, evolved and standalone must partition its IDs")
        enlarged |= evolved | standalone
    return enlarged


def load_metadata() -> dict[int, dict]:
    by_id = {}
    for path in METADATA_CACHE:
        for entry in json.loads(path.read_text())["pokemons"]:
            if entry["zukan_sub_id"] != 0:
                continue
            number = int(entry["zukan_id"])
            if number in by_id:
                raise ValueError(f"#{number}: duplicate metadata")
            by_id[number] = entry
    if set(by_id) != set(range(1, POKEMON_COUNT + 1)):
        raise ValueError("metadata must contain exactly IDs 1-649")
    return by_id


def load_manifests() -> tuple[dict[int, str], set[int]]:
    hashes = {}
    present = set()
    for generation, path in MANIFESTS.items():
        if not path.exists():
            continue
        present.add(generation)
        manifest = json.loads(path.read_text())
        if manifest["revision"] != SPRITE_REVISION:
            raise ValueError(f"generation {generation} animation manifest revision differs")
        first, last = RANGES[generation]
        ids = [entry["id"] for entry in manifest["sprites"]]
        if sorted(ids) != list(range(first, last + 1)):
            raise ValueError(f"generation {generation}: incomplete animation manifest")
        for entry in manifest["sprites"]:
            hashes[entry["id"]] = entry["sha256"]
    return hashes, present


def load_sprites(wanted: list[int]) -> list[dict]:
    enlarged_species = load_scales()
    by_id = load_metadata()
    hashes, present = load_manifests()
    sprites = []
    for number in wanted:
        if number not in by_id:
            raise ValueError(f"#{number} metadata missing")
        if generation_of(number) in present and number not in hashes:
            raise ValueError(f"#{number}: missing animation manifest entry")
        entry = by_id[number]
        animation = build_animation(fetch(number, hashes.get(number)))
        sprites.append(dict(animation, id=number, name=entry["pokemon_name"],
                            types=entry["pokemon_type_name"],
                            height_text=entry.get("height") or "-",
                            weight_text=entry.get("weight") or "-",
                            enlarged=number in enlarged_species))
    return sprites


def generate_header(sprites: list[dict]) -> str:
    return '''#pragma once

// 由 tools/build_sprites.py 自动生成，请勿手工编辑。
#include <stddef.h>
#include "sprite.hpp"

namespace pokedex {
namespace data {
extern const Sprite kSprites[];
extern const size_t kSpriteCount;
}  // namespace data
}  // namespace pokedex
'''


def generate_source(sprites: list[dict]) -> str:
    lines = ['// 由 tools/build_sprites.py 自动生成，请勿手工编辑。',
             '#include "sprites_generated.hpp"', '',
             'namespace pokedex {', 'namespace data {', 'namespace {']
    for s in sprites:
        ident = f'kSprite{s["id"]:04d}'
        lines.append(f'// #{s["id"]} {s["name"]}, {s["width"]}x{s["height"]}, '
                     f'{len(s["frames"])} frames')
        lines.append(f'const uint16_t {ident}Palette[] = {{' +
                     ', '.join(f'0x{v:04X}' for v in s["palette"]) + '};')
        lines.append(f'const uint8_t {ident}Data[] = {{')
        for start in range(0, len(s["data"]), 24):
            lines.append('    ' + ', '.join(f'0x{v:02X}' for v in s["data"][start:start+24]) + ',')
        lines.append('};')
        lines.append(f'const Frame {ident}Frames[] = {{')
        for offset, duration in s["frames"]:
            lines.append(f'    {{{offset}, {duration}}},')
        lines.append('};')
    lines.extend(['}  // namespace', '', 'const Sprite kSprites[] = {'])
    for s in sprites:
        ident = f'kSprite{s["id"]:04d}'
        text = lambda key: json.dumps(s[key], ensure_ascii=False)
        lines.append(f'    {{{text("name")}, {text("types")}, {s["id"]}, '
                     f'{s["width"]}, {s["height"]}, {ident}Palette, '
                     f'{len(s["palette"])}, {ident}Data, sizeof({ident}Data), '
                     f'{s["raw_size"]}, {ident}Frames, {len(s["frames"])}, '
                     f'{s["duration"]}, {text("height_text")}, {text("weight_text")}, '
                     f'{text("enlarged")}}},')
    lines.extend(['};', f'const size_t kSpriteCount = {len(sprites)};',
                  '}  // namespace data', '}  // namespace pokedex', ''])
    return '\n'.join(lines)


def generate_manifest(sprites: list[dict]) -> str:
    entries = [dict(id=s["id"], sha256=s["sha256"], width=s["width"], height=s["height"],
                    frames=len(s["frames"]), unique_frames=s["unique"],
                    duration_ms=s["duration"], data_bytes=len(s["data"])) for s in sprites]
    return json.dumps(dict(revision=SPRITE_REVISION, sprites=entries),
                      ensure_ascii=False, indent=2) + '\n'


def write_manifests(sprites: list[dict], generation: int) -> None:
    for key, path in MANIFESTS.items():
        if key > generation:
            continue
        first, last = (1, GEN1_COUNT) if key == 1 else (GEN1_COUNT + 1, POKEMON_COUNT)
        selected = [s for s in sprites if first <= s["id"] <= last]
        if [s["id"] for s in selected] != list(range(first, last + 1)):
            raise ValueError(f"generation {key}: incomplete manifest output")
        path.write_text(generate_manifest(selected), encoding='utf-8')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--generation', type=int, choices=list(RANGES), default=5,
                        help='1 至 5 分别生成到该世代末尾；默认 5 生成 1-649')
    parser.add_argument('--ids', default='', help='指定编号，逗号分隔')
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    count = RANGES[args.generation][1]
    wanted = [int(n) for n in args.ids.split(',')] if args.ids else list(range(1, count + 1))
    if len(set(wanted)) != len(wanted) or any(n < 1 or n > POKEMON_COUNT for n in wanted):
        parser.error('编号必须是1至649的不重复整数')
    output = (args.out or OUT_HEADER).resolve()
    official = {OUT_HEADER.resolve(), OUT_HEADER.with_suffix('.cpp').resolve()}
    if requires_separate_output(wanted, bool(args.ids)):
        if args.out is None:
            parser.error('非完整 1-649 输出必须通过 --out 指定独立路径')
        if output in official or output.with_suffix('.cpp').resolve() in official:
            parser.error('非完整 1-649 输出不得写入正式动画表，请指定独立的 --out 路径')
    sprites = load_sprites(wanted)
    output.write_text(generate_header(sprites), encoding='utf-8')
    output.with_suffix('.cpp').write_text(generate_source(sprites), encoding='utf-8')
    if not args.ids and output == OUT_HEADER:
        write_manifests(sprites, args.generation)
    print(f'{len(sprites)} animations; {sum(len(s["frames"]) for s in sprites)} frames; '
          f'{sum(len(s["data"]) for s in sprites):,} compressed bytes')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, OSError, KeyError) as error:
        print(f'素材生成失败：{error}', file=sys.stderr)
        sys.exit(1)
