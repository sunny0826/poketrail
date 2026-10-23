#!/usr/bin/env python3
"""把黑／白 GIF 完整合成、无损去重，生成带原始时序的 RGB565 索引动画。"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import sys
import urllib.request
from pathlib import Path

from PIL import Image

REPO_ROOT = Path(__file__).resolve().parent.parent
OUT_HEADER = REPO_ROOT / "firmware/apps/20_pokedex/sprites_generated.hpp"
CACHE_DIR = REPO_ROOT / "assets/sprites/black-white"
METADATA_CACHE = REPO_ROOT / "assets/pokedex-gen1.json"
EVOLUTION_CACHE = REPO_ROOT / "assets/evolution-gen1.json"
MANIFEST = REPO_ROOT / "assets/animations-gen1.json"
SPRITE_REVISION = "97a7832d307311c0e1c6ea9035308bf80cc94f81"
SPRITE_URL = (
    f"https://raw.githubusercontent.com/PokeAPI/sprites/{SPRITE_REVISION}/"
    "sprites/pokemon/versions/generation-v/black-white/animated/{id}.gif"
)
TRANSPARENT_KEY = 0xF81F
MAX_CANVAS = 128


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
        raise ValueError(f"#{number} GIF differs from assets/animations-gen1.json")
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


def encode_runs(indices: bytes) -> bytes:
    encoded = bytearray()
    i = 0
    while i < len(indices):
        color = indices[i]
        if color > 15:
            raise ValueError("palette index exceeds four bits")
        count = 1
        while count < 16 and i + count < len(indices) and indices[i + count] == color:
            count += 1
        encoded.append(((count - 1) << 4) | color)
        i += count
    return bytes(encoded)


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
                lookup[color] = len(palette)
                palette.append(color)
        if len(palette) > 16:
            raise ValueError("animation needs more than 16 colors; no quantization allowed")
        indices = bytes(lookup[color] for color in colors)
        if indices not in unique:
            runs = encode_runs(indices)
            unique[indices] = (len(blob), len(runs))
            blob.extend(runs)
        offset, size = unique[indices]
        frames.append((offset, size, duration))
    if len(unique) < 2:
        raise ValueError("animation must contain at least two distinct frames")
    return dict(width=width, height=height, palette=palette, data=bytes(blob),
                frames=frames, unique=len(unique), duration=sum(f[2] for f in frames),
                sha256=hashlib.sha256(data).hexdigest())


def load_evolved_species(path: Path = EVOLUTION_CACHE) -> set[int]:
    raw = json.loads(path.read_text())["parents"]
    if set(raw) != {str(i) for i in range(1, 152)}:
        raise ValueError("evolution metadata must contain exactly IDs 1-151")
    parents = {int(number): parent for number, parent in raw.items()}
    for number, parent in parents.items():
        if parent is not None and (type(parent) is not int or
                                   parent not in parents or parent == number):
            raise ValueError(f"#{number}: invalid evolution parent {parent}")
    for number in parents:
        seen = set()
        current = number
        while current is not None:
            if current in seen:
                raise ValueError(f"#{number}: evolution cycle")
            seen.add(current)
            current = parents[current]
    return {number for number, parent in parents.items() if parent is not None}


def load_sprites(wanted: list[int]) -> list[dict]:
    evolved_species = load_evolved_species()
    metadata = json.loads(METADATA_CACHE.read_text())["pokemons"]
    by_id = {int(e["zukan_id"]): e for e in metadata if e["zukan_sub_id"] == 0}
    manifest = json.loads(MANIFEST.read_text()) if MANIFEST.exists() else {}
    if manifest and manifest["revision"] != SPRITE_REVISION:
        raise ValueError("animation manifest revision differs from generator")
    hashes = {entry["id"]: entry["sha256"] for entry in manifest.get("sprites", [])}
    sprites = []
    for number in wanted:
        if number not in by_id:
            raise ValueError(f"#{number} metadata missing")
        entry = by_id[number]
        animation = build_animation(fetch(number, hashes.get(number)))
        sprites.append(dict(animation, id=number, name=entry["pokemon_name"],
                            types=entry["pokemon_type_name"],
                            height_text=entry.get("height") or "-",
                            weight_text=entry.get("weight") or "-",
                            has_evolved=number in evolved_species))
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
        palette = s["palette"] + [TRANSPARENT_KEY] * (16 - len(s["palette"]))
        lines.append(f'// #{s["id"]} {s["name"]}, {s["width"]}x{s["height"]}, '
                     f'{len(s["frames"])} frames')
        lines.append(f'const uint16_t {ident}Palette[] = {{' +
                     ', '.join(f'0x{v:04X}' for v in palette) + '};')
        lines.append(f'const uint8_t {ident}Data[] = {{')
        for start in range(0, len(s["data"]), 24):
            lines.append('    ' + ', '.join(f'0x{v:02X}' for v in s["data"][start:start+24]) + ',')
        lines.append('};')
        lines.append(f'const Frame {ident}Frames[] = {{')
        for offset, size, duration in s["frames"]:
            lines.append(f'    {{{offset}, {size}, {duration}}},')
        lines.append('};')
    lines.extend(['}  // namespace', '', 'const Sprite kSprites[] = {'])
    for s in sprites:
        ident = f'kSprite{s["id"]:04d}'
        text = lambda key: json.dumps(s[key], ensure_ascii=False)
        lines.append(f'    {{{text("name")}, {text("types")}, {s["id"]}, '
                     f'{s["width"]}, {s["height"]}, {ident}Palette, {ident}Data, '
                     f'sizeof({ident}Data), {ident}Frames, {len(s["frames"])}, '
                     f'{s["duration"]}, {text("height_text")}, {text("weight_text")}, '
                     f'{text("has_evolved")}}},')
    lines.extend(['};', f'const size_t kSpriteCount = {len(sprites)};',
                  '}  // namespace data', '}  // namespace pokedex', ''])
    return '\n'.join(lines)


def generate_manifest(sprites: list[dict]) -> str:
    entries = [dict(id=s["id"], sha256=s["sha256"], width=s["width"], height=s["height"],
                    frames=len(s["frames"]), unique_frames=s["unique"],
                    duration_ms=s["duration"], data_bytes=len(s["data"])) for s in sprites]
    return json.dumps(dict(revision=SPRITE_REVISION, sprites=entries),
                      ensure_ascii=False, indent=2) + '\n'


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--generation', type=int, choices=[1], default=1)
    parser.add_argument('--ids', default='', help='指定编号，逗号分隔')
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    wanted = [int(n) for n in args.ids.split(',')] if args.ids else list(range(1, 152))
    if len(set(wanted)) != len(wanted) or any(n < 1 or n > 151 for n in wanted):
        parser.error('编号必须是1至151的不重复整数')
    output = (args.out or OUT_HEADER).resolve()
    if args.ids and (args.out is None or
                     output.with_suffix('.cpp').resolve() == OUT_HEADER.with_suffix('.cpp').resolve()):
        parser.error('使用 --ids 时必须通过 --out 指定完整图鉴之外的输出路径')
    sprites = load_sprites(wanted)
    output.write_text(generate_header(sprites), encoding='utf-8')
    output.with_suffix('.cpp').write_text(generate_source(sprites), encoding='utf-8')
    if wanted == list(range(1, 152)) and output == OUT_HEADER:
        MANIFEST.write_text(generate_manifest(sprites), encoding='utf-8')
    print(f'{len(sprites)} animations; {sum(len(s["frames"]) for s in sprites)} frames; '
          f'{sum(len(s["data"]) for s in sprites):,} compressed bytes')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, OSError, KeyError) as error:
        print(f'素材生成失败：{error}', file=sys.stderr)
        sys.exit(1)
