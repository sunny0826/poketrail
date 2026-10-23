#!/usr/bin/env python3
"""离线生成宝可梦与精灵球静态缩略图：固定来源 PNG 转 RGB565 常量数组。

平时完全离线：从 assets/sprites/icons 缓存读取并按 assets/icons-gen1.json
核对 SHA256，哈希不符立即失败。首次或补图时使用 --download 显式联网下载，
--write-manifest 根据缓存重建清单。
"""

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
OUT_HEADER = REPO_ROOT / "firmware/apps/20_pokedex/icons_generated.hpp"
CACHE_DIR = REPO_ROOT / "assets/sprites/icons"
MANIFEST = REPO_ROOT / "assets/icons-gen1.json"
SPRITE_REVISION = "97a7832d307311c0e1c6ea9035308bf80cc94f81"
SOURCE_REPO = f"https://github.com/PokeAPI/sprites/tree/{SPRITE_REVISION}"
ICON_URL = (
    f"https://raw.githubusercontent.com/PokeAPI/sprites/{SPRITE_REVISION}/"
    "sprites/pokemon/versions/generation-vii/icons/{id}.png"
)
POKE_BALL_URL = (
    f"https://raw.githubusercontent.com/PokeAPI/sprites/{SPRITE_REVISION}/"
    "sprites/items/poke-ball.png"
)
POKE_BALL_CACHE = "poke-ball.png"
TRANSPARENT_KEY = 0xF81F  # 与 firmware/apps/20_pokedex/sprite.hpp kTransparentKey 一致
POKEMON_COUNT = 151


def cache_path(name: str) -> Path:
    return CACHE_DIR / name


def fetch(name: str, url: str, download: bool) -> bytes:
    path = cache_path(name)
    if not path.exists():
        if not download:
            raise ValueError(f"缓存缺少 {name}；使用 --download 显式下载")
        request = urllib.request.Request(url, headers={"User-Agent": "m5-pokedex/1.0"})
        with urllib.request.urlopen(request, timeout=30) as response:
            data = response.read()
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    return path.read_bytes()


def to_rgb565(r: int, g: int, b: int) -> int:
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def build_icon(data: bytes, name: str) -> dict:
    image = Image.open(io.BytesIO(data))
    if image.format != "PNG":
        raise ValueError(f"{name}: not a PNG")
    width, height = image.size
    if not (0 < width <= 64 and 0 < height <= 64):
        raise ValueError(f"{name}: unsupported canvas {width}x{height}")
    pixels = []
    transparent = 0
    for r, g, b, a in image.convert("RGBA").get_flattened_data():
        if a < 128:
            pixels.append(TRANSPARENT_KEY)
            transparent += 1
        else:
            color = to_rgb565(r, g, b)
            if color == TRANSPARENT_KEY:
                raise ValueError(f"{name}: opaque pixel collides with transparent key")
            pixels.append(color)
    if transparent == 0:
        raise ValueError(f"{name}: icon has no transparent pixels")
    return dict(width=width, height=height, pixels=tuple(pixels),
                sha256=hashlib.sha256(data).hexdigest())


def load_manifest() -> dict:
    manifest = json.loads(MANIFEST.read_text())
    if manifest["revision"] != SPRITE_REVISION:
        raise ValueError("icon manifest revision differs from generator")
    entries = {entry["id"]: entry for entry in manifest["icons"]}
    if set(entries) != set(range(1, POKEMON_COUNT + 1)):
        raise ValueError("icon manifest must contain exactly IDs 1-151")
    for entry in manifest["icons"]:
        if len(entry["sha256"]) != 64 or "width" not in entry or "height" not in entry:
            raise ValueError(f"#{entry['id']}: incomplete manifest entry")
    if "poke_ball" not in manifest:
        raise ValueError("icon manifest missing poke_ball entry")
    return manifest


def verify_hash(name: str, data: bytes, expected: str) -> None:
    actual = hashlib.sha256(data).hexdigest()
    if actual != expected:
        raise ValueError(f"{name}: SHA256 differs from assets/icons-gen1.json")


def load_icons(download: bool, write_manifest: bool) -> tuple[list[dict], dict]:
    """返回 (151只宝可梦图标, 精灵球图标)，按清单核对或重建清单。"""
    manifest = None if write_manifest else load_manifest()
    entries = ({entry["id"]: entry for entry in manifest["icons"]}
               if manifest else {})
    pokemon = []
    for number in range(1, POKEMON_COUNT + 1):
        name = f"{number:04d}.png"
        data = fetch(name, ICON_URL.format(id=number), download)
        if manifest:
            verify_hash(name, data, entries[number]["sha256"])
        icon = build_icon(data, name)
        if manifest:
            entry = entries[number]
            if (icon["width"], icon["height"]) != (entry["width"], entry["height"]):
                raise ValueError(f"{name}: size differs from manifest")
        pokemon.append(dict(icon, id=number))
    ball_data = fetch(POKE_BALL_CACHE, POKE_BALL_URL, download)
    if manifest:
        verify_hash(POKE_BALL_CACHE, ball_data, manifest["poke_ball"]["sha256"])
    poke_ball = build_icon(ball_data, POKE_BALL_CACHE)
    if manifest:
        entry = manifest["poke_ball"]
        if (poke_ball["width"], poke_ball["height"]) != (entry["width"], entry["height"]):
            raise ValueError(f"{POKE_BALL_CACHE}: size differs from manifest")
    return pokemon, poke_ball


def generate_header() -> str:
    return '''#pragma once

// 由 tools/build_icons.py 自动生成，请勿手工编辑。
// 透明像素使用 sprite.hpp 的 pokedex::kTransparentKey。
#include <stdint.h>

namespace pokedex {
namespace data {

struct Icon {
  uint16_t width;
  uint16_t height;
  const uint16_t* pixels;
};

extern const Icon kPokemonIcons[151];
extern const Icon kPokeBallIcon;

}  // namespace data
}  // namespace pokedex
'''


def emit_pixels(lines: list[str], ident: str, pixels: tuple[int, ...]) -> None:
    lines.append(f'const uint16_t {ident}[] = {{')
    for start in range(0, len(pixels), 12):
        lines.append('    ' + ', '.join(f'0x{v:04X}' for v in pixels[start:start + 12]) + ',')
    lines.append('};')


def generate_source(pokemon: list[dict], poke_ball: dict) -> str:
    lines = ['// 由 tools/build_icons.py 自动生成，请勿手工编辑。',
             '#include "icons_generated.hpp"', '',
             'namespace pokedex {', 'namespace data {', 'namespace {']
    for icon in pokemon:
        lines.append(f'// #{icon["id"]}, {icon["width"]}x{icon["height"]}')
        emit_pixels(lines, f'kIcon{icon["id"]:04d}Pixels', icon["pixels"])
    lines.append(f'// poke-ball, {poke_ball["width"]}x{poke_ball["height"]}')
    emit_pixels(lines, 'kPokeBallPixels', poke_ball["pixels"])
    lines.extend(['}  // namespace', '', 'const Icon kPokemonIcons[151] = {'])
    for icon in pokemon:
        ident = f'kIcon{icon["id"]:04d}Pixels'
        lines.append(f'    {{{icon["width"]}, {icon["height"]}, {ident}}},')
    lines.extend(['};',
                  f'const Icon kPokeBallIcon = '
                  f'{{{poke_ball["width"]}, {poke_ball["height"]}, kPokeBallPixels}};',
                  '}  // namespace data', '}  // namespace pokedex', ''])
    return '\n'.join(lines)


def generate_manifest(pokemon: list[dict], poke_ball: dict) -> str:
    entries = [dict(id=icon["id"], sha256=icon["sha256"],
                    width=icon["width"], height=icon["height"]) for icon in pokemon]
    return json.dumps(dict(
        source=SOURCE_REPO,
        pokemon_path="sprites/pokemon/versions/generation-vii/icons/{id}.png",
        poke_ball_path="sprites/items/poke-ball.png",
        revision=SPRITE_REVISION,
        icons=entries,
        poke_ball=dict(sha256=poke_ball["sha256"],
                       width=poke_ball["width"], height=poke_ball["height"]),
    ), ensure_ascii=False, indent=2) + '\n'


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--download', action='store_true',
                        help='允许联网下载缓存中缺失的 PNG')
    parser.add_argument('--write-manifest', action='store_true',
                        help='根据缓存重建 assets/icons-gen1.json（隐含 --download）')
    parser.add_argument('--check', action='store_true',
                        help='只校验生成物与清单一致，不写任何文件')
    args = parser.parse_args()
    download = args.download or args.write_manifest
    if args.check and (download or args.write_manifest):
        parser.error('--check 不能与 --download/--write-manifest 同时使用')
    pokemon, poke_ball = load_icons(download, args.write_manifest)
    header = generate_header()
    source = generate_source(pokemon, poke_ball)
    if args.check:
        out_cpp = OUT_HEADER.with_suffix('.cpp')
        if not OUT_HEADER.exists() or not out_cpp.exists():
            raise ValueError('生成物缺失，先运行 tools/build_icons.py')
        if OUT_HEADER.read_text(encoding='utf-8') != header:
            raise ValueError('icons_generated.hpp 与生成器输出不一致')
        if out_cpp.read_text(encoding='utf-8') != source:
            raise ValueError('icons_generated.cpp 与生成器输出不一致')
        print(f'check ok: {len(pokemon)} pokemon icons + poke-ball match manifest')
        return 0
    if args.write_manifest:
        MANIFEST.write_text(generate_manifest(pokemon, poke_ball), encoding='utf-8')
    OUT_HEADER.write_text(header, encoding='utf-8')
    OUT_HEADER.with_suffix('.cpp').write_text(source, encoding='utf-8')
    total = sum(len(icon["pixels"]) for icon in pokemon) + len(poke_ball["pixels"])
    print(f'{len(pokemon)} pokemon icons + poke-ball; {total * 2:,} pixel bytes')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, OSError, KeyError) as error:
        print(f'图标生成失败：{error}', file=sys.stderr)
        sys.exit(1)
