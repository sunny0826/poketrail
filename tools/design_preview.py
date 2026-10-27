from __future__ import annotations

import re
from functools import lru_cache
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
SPECIES_COUNT = 251
SIZE = 466
CENTER = 233
RADIUS = 228
KEY = 0xF81F
BG = "#081311"
PANEL = "#11231E"
PANEL_2 = "#19352C"
TEXT = "#F4F0E4"
MUTED = "#8DA59A"
MINT = "#7CE3B1"
GOLD = "#F4C95D"
RED = "#FF8278"
BLUE = "#7AC9E8"
PURPLE = "#B8A4E3"
ROUTE_COLORS = {"forest": MINT, "coast": BLUE, "cave": PURPLE}
ROUTE_NAMES = {"forest": "森林", "coast": "海边", "cave": "洞窟"}


def rgb565(value: int) -> tuple[int, int, int]:
    return ((value >> 11) * 255 // 31,
            ((value >> 5) & 63) * 255 // 63,
            (value & 31) * 255 // 31)


@lru_cache(maxsize=64)
def font(size: int, mono: bool = False) -> ImageFont.FreeTypeFont:
    path = ("/System/Library/Fonts/Menlo.ttc" if mono else
            "/System/Library/Fonts/STHeiti Medium.ttc")
    if not Path(path).exists():
        path = "/System/Library/Fonts/Supplemental/Arial Unicode.ttf"
    return ImageFont.truetype(path, size)


class GeneratedAssets:
    def __init__(self) -> None:
        base = ROOT / "firmware/apps/20_pokedex"
        self.icons = (base / "icons_generated.cpp").read_text(encoding="utf-8")
        self.sprites = (base / "sprites_generated.cpp").read_text(encoding="utf-8")
        self._cache: dict[tuple[str, int, int], Image.Image] = {}
        table = re.search(r"const Sprite kSprites\[\] = \{(.*?)\n\};", self.sprites, re.S)
        if not table:
            raise ValueError("missing generated sprite table")
        self.meta: dict[int, dict] = {}
        row = re.compile(
            r'\{"([^"]+)", "([^"]+)", (\d+), (\d+), (\d+), kSprite\d+Palette, '
            r'kSprite\d+Data, sizeof\(kSprite\d+Data\), kSprite\d+Frames, (\d+), '
            r'\d+, "[^"]*", "[^"]*", (true|false)\}')
        for line in table.group(1).splitlines():
            match = row.search(line)
            if match:
                name, types, number, width, height, frames, enlarged = match.groups()
                self.meta[int(number)] = dict(name=name, types=types, width=int(width),
                    height=int(height), frames=int(frames), enlarged=enlarged == "true")
        if set(self.meta) != set(range(1, SPECIES_COUNT + 1)):
            raise ValueError(f"generated sprite table must contain IDs 1-{SPECIES_COUNT}")

    @staticmethod
    def _array(source: str, kind: str, name: str) -> list[int]:
        match = re.search(rf"const {kind} {name}\[\] = \{{(.*?)\}};", source, re.S)
        if not match:
            raise ValueError(f"missing generated array {name}")
        return [int(token, 16) for token in re.findall(r"0x[0-9A-Fa-f]+", match.group(1))]

    @staticmethod
    def _image(width: int, height: int, pixels: list[int], scale: int) -> Image.Image:
        if len(pixels) != width * height:
            raise ValueError("generated pixel count does not match canvas")
        image = Image.new("RGBA", (width, height))
        image.putdata([(*rgb565(pixel), 0 if pixel == KEY else 255) for pixel in pixels])
        return image.resize((width * scale, height * scale), Image.Resampling.NEAREST)

    def icon(self, number: int, scale: int = 1) -> Image.Image:
        key = ("icon", number, scale)
        if key not in self._cache:
            if number == 0:
                match = re.search(r"const Icon kPokeBallIcon = \{(\d+), (\d+),", self.icons)
                if not match:
                    raise ValueError("missing poke-ball icon")
                width, height = map(int, match.groups())
                name = "kPokeBallPixels"
            else:
                width, height = 40, 30
                name = f"kIcon{number:04d}Pixels"
            self._cache[key] = self._image(width, height, self._array(self.icons, "uint16_t", name), scale)
        return self._cache[key].copy()

    def sprite(self, number: int, frame: int = 0, scale: int | None = None) -> Image.Image:
        meta = self.meta[number]
        scale = scale or (3 if meta["enlarged"] and meta["width"] * 3 <= 336
                          and meta["height"] * 3 <= 270 else 2)
        cache_key = ("frame", number, frame % meta["frames"], scale)
        if cache_key not in self._cache:
            stem = f"kSprite{number:04d}"
            palette = self._array(self.sprites, "uint16_t", stem + "Palette")
            data = self._array(self.sprites, "uint8_t", stem + "Data")
            block = re.search(rf"const Frame {stem}Frames\[\] = \{{(.*?)\}};", self.sprites, re.S)
            if not block:
                raise ValueError(f"missing frames for {number}")
            frames = [tuple(map(int, values)) for values in
                      re.findall(r"\{(\d+), (\d+), (\d+)\}", block.group(1))]
            offset, size, _ = frames[frame % len(frames)]
            pixels: list[int] = []
            for run in data[offset:offset + size]:
                pixels.extend([palette[run & 15]] * ((run >> 4) + 1))
            self._cache[cache_key] = self._image(meta["width"], meta["height"], pixels, scale)
        return self._cache[cache_key].copy()

    def sprite_frames(self, number: int, scale: int | None = None) -> list[tuple[Image.Image, int]]:
        stem = f"kSprite{number:04d}"
        block = re.search(rf"const Frame {stem}Frames\[\] = \{{(.*?)\}};", self.sprites, re.S)
        if not block:
            raise ValueError(f"missing frames for {number}")
        frames = [tuple(map(int, values)) for values in
                  re.findall(r"\{(\d+), (\d+), (\d+)\}", block.group(1))]
        return [(self.sprite(number, index, scale), duration)
                for index, (_, _, duration) in enumerate(frames)]

    def name(self, number: int) -> str:
        return self.meta[number]["name"]


class Canvas:
    def __init__(self, background: str = BG) -> None:
        self.image = Image.new("RGB", (SIZE, SIZE), "#000000")
        self.draw = ImageDraw.Draw(self.image)
        self.draw.ellipse((5, 5, 461, 461), fill=background)

    def text(self, value: str, x: int, y: int, size: int = 16, color: str = TEXT,
             anchor: str = "mm", mono: bool = False) -> None:
        self.draw.text((x, y), value, font=font(size, mono), fill=color, anchor=anchor)

    def fit(self, value: str, x: int, y: int, width: int, size: int = 24,
            color: str = TEXT, minimum: int = 14, anchor: str = "mm") -> None:
        while size > minimum and self.draw.textlength(value, font=font(size)) > width:
            size -= 1
        self.text(value, x, y, size, color, anchor)

    def panel(self, box: tuple[int, int, int, int], fill: str = PANEL,
              outline: str | None = None, radius: int = 12, width: int = 1) -> None:
        self.draw.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)

    def paste(self, image: Image.Image, x: int, y: int) -> None:
        self.image.paste(image, (x - image.width // 2, y - image.height // 2), image)

    def finish(self) -> Image.Image:
        self.draw.ellipse((5, 5, 461, 461), outline="#29433B", width=2)
        mask = Image.new("L", (SIZE, SIZE), 0)
        ImageDraw.Draw(mask).ellipse((5, 5, 461, 461), fill=255)
        result = Image.new("RGB", (SIZE, SIZE), "#000000")
        result.paste(self.image, (0, 0), mask)
        return result


if __package__:
    from tools.design_preview_scenes import pixel_scene, pixel_scene_source
else:
    from design_preview_scenes import pixel_scene, pixel_scene_source


def battery(c: Canvas, percent: int = 67) -> None:
    color = RED if 0 <= percent <= 20 else TEXT
    c.draw.rounded_rectangle((198, 11, 220, 23), radius=2, outline=color)
    c.draw.rectangle((221, 15, 223, 19), fill=color)
    if percent >= 0:
        c.draw.rectangle((201, 14, 201 + max(1, percent * 16 // 100), 20), fill=color)
    c.text(f"{percent}%" if percent >= 0 else "--", 231, 17, 13, color, "lm", True)


def title(c: Canvas, value: str, subtitle: str = "", accent: str = MINT) -> None:
    battery(c)
    c.fit(value, 233, 52, 280, 24)
    if subtitle:
        c.fit(subtitle, 233, 80, 300, 14, accent)


def glyph(c: Canvas, name: str, x: int, y: int, color: str = MUTED) -> None:
    d = c.draw
    if name == "route":
        d.line((x - 9, y + 7, x - 2, y - 5, x + 3, y + 2, x + 7, y - 4, x + 10, y + 7), fill=color, width=2)
        d.line((x - 9, y + 7, x + 10, y + 7), fill=color, width=2)
    elif name == "next":
        d.line((x - 7, y, x + 7, y), fill=color, width=2)
        d.line((x + 7, y, x + 1, y - 6), fill=color, width=2)
        d.line((x + 7, y, x + 1, y + 6), fill=color, width=2)
    elif name == "partner":
        d.ellipse((x - 7, y - 6, x + 7, y + 8), outline=color)
        d.rectangle((x - 8, y - 9, x - 3, y - 4), fill=color)
        d.rectangle((x + 3, y - 9, x + 8, y - 4), fill=color)
        d.point((x - 3, y), fill=color); d.point((x + 3, y), fill=color)
    elif name == "book":
        d.rectangle((x - 8, y - 8, x + 7, y + 8), outline=color)
        d.line((x - 3, y - 8, x - 3, y + 8), fill=color)
    elif name == "records":
        for dy, width in [(-6, 12), (0, 15), (6, 9)]:
            d.rectangle((x - 7, y + dy, x - 4, y + dy + 2), fill=color)
            d.line((x, y + dy + 1, x + width - 8, y + dy + 1), fill=color)
    elif name == "clock":
        d.ellipse((x - 8, y - 8, x + 8, y + 8), outline=color)
        d.line((x, y, x, y - 5), fill=color); d.line((x, y, x + 4, y + 3), fill=color)
    elif name == "practice":
        d.ellipse((x - 8, y - 8, x + 8, y + 8), outline=color)
        d.ellipse((x - 3, y - 3, x + 3, y + 3), outline=color)
    elif name == "sleep":
        d.pieslice((x - 8, y - 9, x + 8, y + 8), 65, 295, fill=color)
        d.ellipse((x - 2, y - 10, x + 9, y + 3), fill=PANEL)
    elif name == "home":
        d.polygon([(x - 9, y), (x, y - 8), (x + 9, y)], outline=color)
        d.rectangle((x - 6, y, x + 6, y + 8), outline=color)
    elif name == "back":
        d.line((x - 7, y, x + 7, y), fill=color, width=2)
        d.line((x - 7, y, x - 1, y - 6), fill=color, width=2)
        d.line((x - 7, y, x - 1, y + 6), fill=color, width=2)
    else:
        d.ellipse((x - 7, y - 7, x + 7, y + 7), outline=color)
        d.line((x - 7, y, x + 7, y), fill=color)


def menu(c: Canvas, items: list[dict], selected: int, accent: str = MINT,
         top: int = 228, footer: str = "左 / 右选择 · 双键确认") -> None:
    first = selected // 4 * 4
    for row, index in enumerate(range(first, min(first + 4, len(items)))):
        item = items[index]
        y = top + row * 48
        active = index == selected
        c.panel((83, y - 22, 383, y + 22), PANEL_2 if active else PANEL,
                accent if active else None, 10, 2)
        if active:
            c.draw.rectangle((85, y - 10, 88, y + 10), fill=accent)
        color = accent if active else (MUTED if item.get("disabled") else TEXT)
        if item.get("species"):
            c.paste(c.assets.icon(item["species"]), 112, y)
        else:
            glyph(c, item.get("icon", "dot"), 108, y, color)
        c.fit(item["label"], 137, y, 198 if item.get("note") else 218, 16, color, anchor="lm")
        if item.get("note"):
            c.text(item["note"], 357, y, 12, color, "rm")
        elif active:
            c.text("›", 357, y - 1, 22, accent)
    if len(items) > 4:
        pages = (len(items) + 3) // 4
        c.panel((145, 402, 321, 434), PANEL, None, 9)
        c.text(f"{selected // 4 + 1}/{pages}  下一页", 233, 418, 14, TEXT)
    elif footer:
        c.fit(footer, 233, 420, 240, 13, MUTED)


def poke_count(c: Canvas, assets: GeneratedAssets, count: int, x: int = 327, y: int = 82) -> None:
    c.paste(assets.icon(0), x - 15, y)
    c.text(str(count), x + 8, y, 16, TEXT, "lm", True)


def progress(c: Canvas, value: float, y: int = 366, color: str = MINT) -> None:
    for index in range(12):
        x = 133 + index * 18
        c.draw.rectangle((x, y, x + 12, y + 4), fill=color if index < round(value * 12) else "#29433B")


def alert(c: Canvas, value: str, y: int = 194, color: str = GOLD) -> None:
    width = min(330, int(c.draw.textlength(value, font=font(14))) + 32)
    c.panel((233 - width // 2, y - 15, 233 + width // 2, y + 15), "#253022", None, 8)
    c.fit(value, 233, y, width - 18, 14, color)


def prepare(c: Canvas, assets: GeneratedAssets) -> None:
    c.assets = assets
