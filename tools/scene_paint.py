"""地区场景共用的像素绘制工具：有序抖动渐变、描边、云、树冠与地面纹理。"""
from __future__ import annotations

from random import Random

from PIL import Image, ImageChops, ImageDraw

W = 228
H = 135
BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


def rgb(value: str) -> tuple[int, int, int]:
    value = value.lstrip("#")
    return tuple(int(value[i:i + 2], 16) for i in (0, 2, 4))


def layer() -> Image.Image:
    return Image.new("RGBA", (W, H), (0, 0, 0, 0))


def gradient_in(img, mask, y0, y1, stops):
    cols = [rgb(c) + (255,) for c in stops]
    n = len(cols) - 1
    px, mk = img.load(), mask.load()
    box = mask.getbbox()
    if not box:
        return
    for y in range(box[1], box[3]):
        t = min(max((y - y0) / max(1, y1 - y0), 0), 1) * n
        i = min(int(t), n - 1)
        f = t - i
        for x in range(box[0], box[2]):
            if mk[x, y]:
                px[x, y] = cols[i + 1] if f * 16 > BAYER[y % 4][x % 4] + 0.5 else cols[i]


def fill_gradient(img, shape, y0, y1, stops):
    mask = Image.new("L", (W, H), 0)
    shape(ImageDraw.Draw(mask))
    gradient_in(img, mask, y0, y1, stops)


def rect_gradient(img, box, stops):
    fill_gradient(img, lambda m: m.rectangle(box, fill=255), box[1], box[3], stops)


def outline(src, color):
    alpha = src.getchannel("A").point(lambda v: 255 if v else 0)
    grown = alpha.copy()
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        shifted = Image.new("L", (W, H), 0)
        shifted.paste(alpha, (dx, dy))
        grown = ImageChops.lighter(grown, shifted)
    ring = ImageChops.subtract(grown, alpha)
    out = layer()
    out.paste(Image.new("RGBA", (W, H), rgb(color) + (255,)), (0, 0), ring)
    out.alpha_composite(src)
    return out


def stamp(base, draw_fn, edge):
    obj = layer()
    draw_fn(ImageDraw.Draw(obj), obj)
    base.alpha_composite(outline(obj, edge) if edge else obj)


def noise(draw, box, colors, count, seed, mask=None):
    rng = Random(seed)
    for _ in range(count):
        x = rng.randint(box[0], box[2] - 1)
        y = rng.randint(box[1], box[3] - 1)
        if mask is None or mask(x, y):
            draw.point((x, y), fill=colors[rng.randrange(len(colors))])


def cloud(d, x, y, w, shadow, base, light):
    blobs = [(x + w * i // 5, y - (3 if i in (1, 2) else 1) - (2 if i == 2 else 0), w // 4 + (2 if i in (1, 2) else 0))
             for i in range(5)]
    for bx, by, r in blobs:
        d.ellipse((bx - r, by - r // 2 + 2, bx + r, by + r // 2 + 2), fill=shadow)
    for bx, by, r in blobs:
        d.ellipse((bx - r, by - r // 2, bx + r, by + r // 2), fill=base)
    for bx, by, r in blobs:
        d.ellipse((bx - r + 2, by - r // 2, bx + r // 3, by), fill=light)
    d.rectangle((x - w // 4, y + 2, x + w + w // 6, y + 3), fill=shadow)


def canopy(d, cx, cy, r, tones, seed):
    dark, base, light, top = tones
    rng = Random(seed)
    blobs = [(cx + rng.randint(-r // 2, r // 2), cy + rng.randint(-r // 3, r // 3), rng.randint(r // 2, r * 3 // 4))
             for _ in range(7)]
    for bx, by, br in blobs:
        d.ellipse((bx - br + 1, by - br + 2, bx + br + 1, by + br + 2), fill=dark)
    for bx, by, br in blobs:
        d.ellipse((bx - br, by - br, bx + br, by + br - 1), fill=base)
    for bx, by, br in blobs:
        d.ellipse((bx - br + 1, by - br, bx + br // 3, by - br // 3), fill=light)
    for _ in range(r * 2):
        x = cx + rng.randint(-r, r // 2)
        y = cy + rng.randint(-r, 0)
        if (x - cx) ** 2 + (y - cy) ** 2 < (r - 2) ** 2:
            d.point((x, y), fill=top)


def trunk(d, x, y0, y1, dark, light):
    d.rectangle((x - 2, y0, x + 2, y1), fill=dark)
    d.line((x - 1, y0, x - 1, y1), fill=light)


def ground_texture(d, top, bottom, colors, seed, skip=None):
    rng = Random(seed)
    for y in range(top, bottom):
        depth = (y - top) / max(1, bottom - top)
        for _ in range(int(3 + depth * 9)):
            x = rng.randrange(W)
            if skip and skip(x, y):
                continue
            length = 1 + int(depth * 2.5)
            d.line((x, y, x, y - length + 1), fill=colors[rng.randrange(len(colors))])


def path_mask(top):
    def inside(x, y):
        if y < top:
            return False
        t = (y - top) / (H - top)
        half = 4 + t * 22
        return abs(x - 118 + t * 0) <= half
    return inside


def draw_path(img, top, stops, edge, seed):
    inside = path_mask(top)
    mask = Image.new("L", (W, H), 0)
    mk = mask.load()
    for y in range(top, H):
        for x in range(W):
            if inside(x, y):
                mk[x, y] = 255
    gradient_in(img, mask, top, H, stops)
    d = ImageDraw.Draw(img)
    for y in range(top, H):
        t = (y - top) / (H - top)
        half = int(4 + t * 22)
        d.point((118 - half, y), fill=edge)
        d.point((118 + half, y), fill=edge)
    return inside
