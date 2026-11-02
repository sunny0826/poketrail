"""合众的分层场景，由 design_preview_scenes 按地区调用。"""
from __future__ import annotations

from random import Random

from PIL import ImageDraw

if __package__:
    from tools.scene_paint import (
        H, W, canopy, draw_path, fill_gradient, outline, rect_gradient, rgb, stamp,
    )
else:
    from scene_paint import (
        H, W, canopy, draw_path, fill_gradient, outline, rect_gradient, rgb, stamp,
    )


def unova(far, mid, near, top):
    d = ImageDraw.Draw(far)
    rect_gradient(far, (0, 0, W - 1, 66), ["#0A0E28", "#10143A", "#171C46", "#202652", "#2C3160", "#3A3C6C", "#4A466E"])
    rng = Random(80)
    for _ in range(30):
        d.point((rng.randrange(W), rng.randrange(0, 30)), fill=rng.choice(["#6A72A6", "#9AA2D0"]))
    fill_gradient(far, lambda m: m.ellipse((188, 8, 212, 32), fill=255), 8, 32, ["#2E3466", "#262C5A"])
    d.ellipse((194, 14, 206, 26), fill="#ECE6CC")
    d.ellipse((196, 15, 204, 20), fill="#F8F4E2")
    for cx, cy in ((202, 22), (198, 21)):
        d.point((cx, cy), fill="#CEC6A8")
    x = 0
    rng = Random(81)
    while x < W:
        w = rng.randint(8, 15)
        h = rng.randint(24, 44)
        d.rectangle((x, 66 - h, x + w, 66), fill="#2A3060")
        for wy in range(66 - h + 3, 64, 4):
            for wx in range(x + 2, x + w - 1, 3):
                if rng.random() < 0.12:
                    d.point((wx, wy), fill="#5E5E78")
        x += w + 2

    def city(dd, _):
        r = Random(82)
        x = 0
        while x < W:
            w = r.randint(9, 17)
            h = r.randint(14, 30)
            if 34 <= x <= 52:
                w, h = 16, 52
            if 140 < x < 218:
                h = r.randint(8, 16)
            dd.rectangle((x, 70 - h, x + w, 70), fill=r.choice(["#1C2042", "#20264A", "#1A1E3C"]))
            dd.line((x, 70 - h, x, 70), fill="#353C6C")
            dd.line((x, 70 - h, x + w, 70 - h), fill="#353C6C")
            if w == 16 and h == 52:
                dd.rectangle((x + 3, 70 - h - 8, x + w - 3, 70 - h), fill="#20264A")
                dd.rectangle((x + 6, 70 - h - 14, x + w - 6, 70 - h - 8), fill="#20264A")
                dd.line((x + 8, 70 - h - 22, x + 8, 70 - h - 14), fill="#8A94B4")
                dd.point((x + 8, 70 - h - 23), fill="#FF5A5A")
            elif r.random() < 0.4:
                dd.line((x + w // 2, 70 - h - 5, x + w // 2, 70 - h), fill="#4A5278")
                dd.point((x + w // 2, 70 - h - 6), fill="#FF6A5A")
            dim = 86 < x < 142
            for wy in range(70 - h + 3, 68, 3):
                for wx in range(x + 2, x + w - 1, 3):
                    if r.random() < (0.16 if dim else 0.42):
                        dd.rectangle((wx, wy, wx + 1, wy), fill=r.choice(["#F2CE6A", "#C89A4A", "#E8E2B0"]))
            x += w + 1
    stamp(far, city, "#0C0E22")
    rect_gradient(far, (0, 70, W - 1, 84), ["#141A38", "#182040", "#1C2446"])
    for _ in range(140):
        xx, yy = rng.randrange(W), rng.randrange(71, 84)
        far.putpixel((xx, yy), rgb(rng.choice(["#8A7A4A", "#C8A858", "#3A4270"])) + (255,))

    def bridge(dd, _):
        for tx in (154, 204):
            dd.rectangle((tx - 3, 30, tx - 2, 66), fill="#C8D0E4")
            dd.rectangle((tx + 2, 30, tx + 3, 66), fill="#8A94B4")
            for yy in (34, 46, 58):
                dd.line((tx - 2, yy, tx + 2, yy + 5), fill="#AAB4CE")
                dd.line((tx + 2, yy, tx - 2, yy + 5), fill="#AAB4CE")
            dd.rectangle((tx - 4, 29, tx + 4, 30), fill="#E0E6F2")
        a, b = 154, 204
        for xx in range(a, b + 1):
            t = (xx - a) / (b - a)
            yy = 31 + int(30 * 4 * t * (1 - t))
            dd.point((xx, yy), fill="#D4DAEA")
            if (xx - a) % 4 == 0 and yy < 62:
                dd.line((xx, yy + 1, xx, 62), fill="#7A86A8")
        for xx in list(range(118, 154)) + list(range(204, W)):
            if xx < 154:
                yy = 31 + int((154 - xx) / 36 * 30)
            else:
                yy = 31 + int((xx - 204) / 24 * 22)
            dd.point((xx, yy), fill="#D4DAEA")
        dd.rectangle((118, 62, W - 1, 64), fill="#8A94B4")
        dd.line((118, 62, W - 1, 62), fill="#C8D0E4")
        for xx in range(120, W, 4):
            dd.point((xx, 63), fill="#F2D890")
    stamp(far, bridge, "#10142C")
    for xx in range(120, W, 4):
        for yy in range(72, 84, 3):
            if (xx + yy) % 7 < 3:
                far.putpixel((xx, yy), rgb("#B89848") + (255,))
    d.rectangle((0, 84, W - 1, 85), fill="#7A82A0")
    for xx in range(0, W, 6):
        d.line((xx, 82, xx, 85), fill="#9AA2C0")
    d.line((0, 82, W - 1, 82), fill="#9AA2C0")
    rect_gradient(far, (0, 86, W - 1, H - 1), ["#4A4E66", "#42465C", "#3A3E52", "#33364A"])
    for k in range(-8, 9):
        if k:
            d.line((118 + k * 6, 86, 118 + k * 34, H - 1), fill="#2E3146")
    inside = draw_path(far, 86, ["#2A2C3C", "#262836", "#222432"], "#6A6E88", 4)
    m = ImageDraw.Draw(mid)
    for y in range(top + 2, H, 9):
        m.rectangle((117, y, 119, y + 4), fill="#E8CC64")
        m.point((117, y), fill="#FFF0A0")

    def lamp(x, y):
        for dx, dy in ((-3, 1), (3, 1), (-2, -1), (2, -1), (0, -2), (-4, 0), (4, 0)):
            m.point((x + dx, y + dy), fill="#7A7058")
        m.rectangle((x - 2, y - 1, x + 2, y + 1), fill="#F6E6A4")
        m.point((x - 1, y - 1), fill="#FFFFFF")
        m.rectangle((x - 1, y + 2, x, y + 15), fill="#5A6080")
        m.line((x - 1, y + 2, x - 1, y + 15), fill="#8A92B2")
        m.rectangle((x - 3, y + 15, x + 2, y + 16), fill="#4A5070")
    lamp(26, top + 10)
    lamp(204, top + 12)
    for x, y in [(56, top + 26), (172, top + 30)]:
        m.rectangle((x - 6, y, x + 6, y + 5), fill="#5A5E78")
        m.line((x - 6, y, x + 6, y), fill="#8A8EA8")
        canopy(m, x, y - 4, 6, ("#1E3E36", "#2E5A4A", "#46785C", "#6A9A70"), x)
    mid.alpha_composite(outline(mid.copy(), "#1A1C2C"))
    n = ImageDraw.Draw(near)
    for y in range(top + 5, H, 8):
        for x0, x1 in [(0, 90), (146, W)]:
            n.line((x0, y, x1, y), fill="#2E3146")
