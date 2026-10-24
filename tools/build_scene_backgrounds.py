#!/usr/bin/env python3
"""从定稿的像素场景生成固件 RGB565 小图；无网络或外部素材依赖。"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

from PIL import ImageEnhance

if __package__:
    from tools.design_preview import pixel_scene_source
else:
    from design_preview import pixel_scene_source

ROOT = Path(__file__).resolve().parent.parent
OUT_HEADER = ROOT / "firmware/apps/20_pokedex/scene_backgrounds_generated.hpp"
MANIFEST = ROOT / "assets/scene-backgrounds.json"
ROUTES = ("forest", "coast", "cave")
WIDTH = 228
HEIGHT = 135
DIM_FACTOR = 0.52


def to_rgb565(r: int, g: int, b: int) -> int:
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def image_pixels(route: str, dim: bool = False) -> list[int]:
    image = pixel_scene_source(route)
    if dim:
        image = ImageEnhance.Brightness(image).enhance(DIM_FACTOR)
    if image.size != (WIDTH, HEIGHT):
        raise ValueError(f"unexpected scene canvas for {route}: {image.size}")
    return [to_rgb565(r, g, b) for r, g, b in image.get_flattened_data()]


def pixel_digest(pixels: list[int]) -> str:
    return hashlib.sha256(b"".join(pixel.to_bytes(2, "little") for pixel in pixels)).hexdigest()


def generate_header() -> str:
    return f"""#pragma once

// 由 tools/build_scene_backgrounds.py 自动生成，请勿手工编辑。
#include <stdint.h>

namespace pokedex {{
namespace data {{

constexpr uint16_t kSceneWidth = {WIDTH};
constexpr uint16_t kSceneHeight = {HEIGHT};
constexpr uint16_t kSceneScale = 2;
constexpr uint16_t kSceneThumbTop = 18;
constexpr uint16_t kSceneThumbHeight = 90;
struct SceneBackground {{
  const uint16_t* pixels;
}};
extern const SceneBackground kRouteScenes[3];
extern const SceneBackground kRouteScenesDim[3];

}}  // namespace data
}}  // namespace pokedex
"""


def emit_array(lines: list[str], name: str, pixels: list[int]) -> None:
    lines.append(f"const uint16_t {name}[] = {{")
    for start in range(0, len(pixels), 12):
        lines.append("    " + ", ".join(f"0x{value:04X}" for value in pixels[start:start + 12]) + ",")
    lines.append("};")


def generate_source() -> str:
    lines = ["// 由 tools/build_scene_backgrounds.py 自动生成，请勿手工编辑。",
             '#include "scene_backgrounds_generated.hpp"', "",
             "namespace pokedex {", "namespace data {", "namespace {"]
    for route in ROUTES:
        for dim in (False, True):
            suffix = "Dim" if dim else ""
            lines.append(f"// {route}{suffix}, {WIDTH}x{HEIGHT}")
            emit_array(lines, f"kScene{route.title()}{suffix}", image_pixels(route, dim))
    lines.extend(["}  // namespace", "",
                  "const SceneBackground kRouteScenes[3] = {"])
    for route in ROUTES:
        lines.append(f"    {{kScene{route.title()}}},")
    lines.extend(["};", "", "const SceneBackground kRouteScenesDim[3] = {"])
    for route in ROUTES:
        lines.append(f"    {{kScene{route.title()}Dim}},")
    lines.extend(["};", "}  // namespace data", "}  // namespace pokedex", ""])
    return "\n".join(lines)


def generate_manifest() -> str:
    routes = []
    for route in ROUTES:
        normal = image_pixels(route)
        dim = image_pixels(route, True)
        routes.append(dict(route=route, normalSha256=pixel_digest(normal), dimSha256=pixel_digest(dim)))
    return json.dumps(dict(source="tools/design_preview_scenes.py#pixel_scene_source", width=WIDTH,
        height=HEIGHT, scale=2, dimFactor=DIM_FACTOR, thumbTop=18, thumbHeight=90,
        routes=routes), ensure_ascii=False, indent=2) + "\n"


def generate_all() -> tuple[str, str, str]:
    return generate_header(), generate_source(), generate_manifest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="只校验生成物，不写文件")
    args = parser.parse_args()
    header, source, manifest = generate_all()
    outputs = ((OUT_HEADER, header), (OUT_HEADER.with_suffix(".cpp"), source), (MANIFEST, manifest))
    if args.check:
        for path, expected in outputs:
            if not path.exists() or path.read_text(encoding="utf-8") != expected:
                raise ValueError(f"生成物缺失或不是最新内容: {path}")
        print(f"scene check ok: {len(ROUTES)} routes, normal + dim, {WIDTH}x{HEIGHT}")
        return 0
    for path, content in outputs:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
    print(f"generated {len(ROUTES)} route scenes, normal + dim, {WIDTH}x{HEIGHT}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, KeyError) as error:
        print(f"场景生成失败：{error}", file=sys.stderr)
        sys.exit(1)
