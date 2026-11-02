"""神奥的分层场景，由 design_preview_scenes 按地区调用。"""
from __future__ import annotations

from random import Random

from PIL import ImageDraw

if __package__:
    from tools.scene_paint import (
        H, W, draw_path, fill_gradient, noise, outline, rect_gradient, stamp,
    )
else:
    from scene_paint import (
        H, W, draw_path, fill_gradient, noise, outline, rect_gradient, stamp,
    )


def sinnoh(far, mid, near, top):
    d = ImageDraw.Draw(far)
    rect_gradient(far, (0, 0, W - 1, 84), ["#0C1430", "#121C3A", "#1A2848", "#24365A", "#32486C", "#465E80", "#5E7896"])
    rng = Random(70)
    for _ in range(55):
        x, y = rng.randrange(W), rng.randrange(0, 44)
        d.point((x, y), fill=rng.choice(["#8EA2C8", "#C8D6F0", "#6A7EA6"]))
    for x, y in [(30, 10), (150, 6), (206, 18)]:
        for dx, dy in ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1)):
            d.point((x + dx, y + dy), fill="#E8F0FF" if dx == dy == 0 else "#8EA2C8")
    fill_gradient(far, lambda m: m.polygon([(110, 70), (146, 40), (166, 30), (186, 38), (206, 32), (W, 42), (W, 80),
                                            (110, 80)], fill=255), 30, 80, ["#5E7496", "#4E6488"])
    d.polygon([(146, 40), (166, 30), (186, 38), (176, 40), (166, 36), (156, 42)], fill="#C8D4E6")
    d.polygon([(186, 38), (206, 32), (220, 38), (206, 38), (198, 42)], fill="#B4C2D8")
    fill_gradient(far, lambda m: m.polygon([(0, 76), (16, 60), (32, 44), (50, 26), (62, 14), (70, 10), (80, 16),
                                            (96, 30), (108, 36), (122, 52), (142, 76)], fill=255),
                  10, 76, ["#4A5E82", "#3A4C6E", "#2E3E5C"])
    d.polygon([(70, 10), (80, 16), (96, 30), (108, 36), (122, 52), (142, 76), (98, 76), (84, 40)], fill="#26344E")
    d.polygon([(50, 26), (62, 14), (70, 10), (80, 16), (90, 26), (82, 24), (76, 30), (70, 22), (62, 30), (56, 28)],
              fill="#E8EEF6")
    d.polygon([(70, 10), (80, 16), (90, 26), (82, 24), (76, 30), (74, 18)], fill="#AEBED6")
    rng = Random(75)
    for sx, sy, length in [(42, 36, 9), (50, 30, 12), (58, 30, 7), (86, 28, 10), (96, 34, 8), (104, 38, 6)]:
        x = sx
        for k in range(length):
            y = sy + k
            if rng.random() < 0.7:
                d.point((x, y), fill="#C4D0E2" if k < length * 0.6 else "#8EA0BE")
            x += rng.choice((-1, 0, 0, 1)) if sx < 70 else rng.choice((0, 0, 1))
    for px in (68, 70, 72):
        d.line((px, 8, px, 10), fill="#9AA8C2")
    d.line((67, 8, 73, 8), fill="#B4C0D6")

    def pine(dd, x, base, h):
        tiers = 4
        for i in range(tiers):
            w = int(h * 0.38) - i * (h // 12)
            yb = base - i * h // 5
            yt = yb - h // 3
            dd.polygon([(x - w, yb), (x, yt), (x + w, yb)], fill="#1E4844")
            dd.polygon([(x, yt), (x + w, yb), (x, yb)], fill="#163634")
            dd.line((x - w, yb, x, yt), fill="#E4ECF4")
            dd.line((x - w + 1, yb, x - w + 4, yb - 2), fill="#E4ECF4")
            dd.line((x + 1, yt + 1, x + w, yb), fill="#A8BAD0")
        dd.rectangle((x - 1, base, x + 1, base + 3), fill="#3A2E28")
    stamp(far, lambda dd, _: [pine(dd, x, 80, h) for x, h in
                              [(4, 30), (20, 22), (34, 16), (48, 12), (160, 12), (174, 16), (210, 24), (224, 30)]],
          "#0E1E24")

    def cabin(dd, _):
        x, base = 190, 80
        dd.rectangle((x - 10, base - 9, x + 10, base), fill="#6A4A36")
        for yy in range(base - 8, base, 2):
            dd.line((x - 10, yy, x + 10, yy), fill="#563A2A")
        dd.rectangle((x - 4, base - 6, x - 1, base - 3), fill="#F2C870")
        dd.point((x - 3, base - 5), fill="#FFF0C0")
        dd.rectangle((x + 3, base - 6, x + 6, base), fill="#3E2A20")
        dd.polygon([(x - 13, base - 9), (x, base - 17), (x + 13, base - 9)], fill="#E8EEF6")
        dd.polygon([(x, base - 17), (x + 13, base - 9), (x, base - 9)], fill="#B8C6DA")
        dd.rectangle((x + 6, base - 18, x + 8, base - 13), fill="#5A4A44")
        dd.rectangle((x + 5, base - 19, x + 9, base - 18), fill="#E8EEF6")
    stamp(far, cabin, "#141C2A")
    for i, (x, y) in enumerate([(197, 59), (199, 55), (196, 51), (198, 47)]):
        d.point((x, y), fill="#9AAAC4" if i % 2 else "#C8D2E2")
        d.point((x + 1, y), fill="#9AAAC4")
    for gx, gy in [(194, 74), (192, 75), (189, 74)]:
        d.point((gx - 6, gy + 7), fill="#8A7A5A")
    rect_gradient(far, (0, 80, W - 1, H - 1), ["#B4C4D8", "#C6D4E4", "#CCD8E6", "#BCCADC", "#AEBED4"])
    for x0, x1, y in [(0, 60, 82), (150, W, 82)]:
        fill_gradient(far, lambda m, a=x0, b=x1, c=y: m.ellipse((a, c - 2, b, c + 6), fill=255), y - 2, y + 6,
                      ["#94A8C4", "#A8BAD0"])
    inside = draw_path(far, 81, ["#A6B6CC", "#9CAEC6", "#94A6BE"], "#8496B2", 3)
    noise(d, (0, 84, W, H), ["#E8F0F8", "#9EB2CA"], 140, 73, lambda x, y: not inside(x, y))
    m = ImageDraw.Draw(mid)
    for i, y in enumerate(range(top + 4, H - 3, 7)):
        x = 114 if i % 2 else 120
        m.rectangle((x, y, x + 1, y + 2), fill="#7E92AE")
        m.point((x, y + 3), fill="#8EA2BC")
    for box in [(4, 98, 42, 108), (184, 110, 224, 120), (18, 122, 62, 132), (154, 96, 186, 104)]:
        fill_gradient(mid, lambda mm, b=box: mm.ellipse(b, fill=255), box[1], box[3], ["#F4F8FC", "#D2DEEA", "#A6B8CE"])
    for x, y in [(68, 112), (166, 126)]:
        m.ellipse((x - 5, y - 3, x + 5, y + 4), fill="#5E6E86")
        m.ellipse((x - 5, y - 4, x + 4, y), fill="#F0F4FA")
        m.point((x - 2, y - 3), fill="#FFFFFF")
    for x, y in [(90, 96), (148, 104)]:
        for dx, dy in ((-3, -5), (0, -7), (3, -4), (-1, -3)):
            m.line((x, y, x + dx, y + dy), fill="#5A5048")
    mid.alpha_composite(outline(mid.copy(), "#6A7C98"))
    n = ImageDraw.Draw(near)
    rng = Random(74)
    for _ in range(48):
        x, y = rng.randrange(2, W - 2), rng.randrange(top + 2, H - 2)
        n.point((x, y), fill="#FFFFFF")
        if rng.random() < 0.25:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                n.point((x + dx, y + dy), fill="#DCE6F2")
