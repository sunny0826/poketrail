"""丰缘的分层场景，由 design_preview_scenes 按地区调用。"""
from __future__ import annotations

import math
from random import Random

from PIL import ImageDraw

if __package__:
    from tools.scene_paint import (
        H, W, cloud, draw_path, fill_gradient, noise, outline, rect_gradient, stamp,
    )
else:
    from scene_paint import (
        H, W, cloud, draw_path, fill_gradient, noise, outline, rect_gradient, stamp,
    )


def hoenn(far, mid, near, top):
    d = ImageDraw.Draw(far)
    rect_gradient(far, (0, 0, W - 1, 58), ["#184868", "#1E5878", "#286C88", "#3A8296", "#5A9AA4", "#82B6B2"])
    cloud(d, 138, 22, 64, "#88AAB0", "#C2D8D2", "#EEF4EA")
    cloud(d, 96, 34, 26, "#88AAB0", "#C2D8D2", "#EEF4EA")
    fill_gradient(far, lambda m: m.polygon([(0, 62), (14, 42), (28, 24), (46, 22), (62, 38), (90, 62)], fill=255),
                  22, 62, ["#8A6A58", "#6E5446", "#5A4438"])
    d.polygon([(40, 23), (46, 22), (62, 38), (90, 62), (62, 62)], fill="#4C3A32")
    for sx in (16, 24, 52, 70):
        d.line((sx, 60, (sx + 37) // 2, 32), fill="#5A4438")
    d.line((28, 24, 46, 22), fill="#2E2220")
    for x, y in [(32, 23), (36, 22), (40, 23), (44, 22)]:
        d.point((x, y), fill="#FFC060")
    d.line((38, 24, 34, 34), fill="#E8743A")
    d.line((34, 34, 36, 40), fill="#C85A30")
    for x, y, r in [(42, 16, 5), (50, 10, 6), (60, 6, 7), (72, 4, 6), (82, 5, 5)]:
        d.ellipse((x - r, y - r + 2, x + r, y + r + 2), fill="#6E6C74")
        d.ellipse((x - r, y - r, x + r, y + r), fill="#9C9AA2")
        d.ellipse((x - r + 1, y - r, x + 1, y - 1), fill="#C4C2C6")
    for rx, ry in [(22, 36), (30, 30), (12, 50)]:
        for k in range(14):
            d.point((rx - k // 2, ry + k), fill="#A08270")
            d.point((rx - k // 2 + 1, ry + k), fill="#5E4A3E")
    for rx, ry in [(50, 28), (60, 40)]:
        for k in range(12):
            d.point((rx + k // 2, ry + k), fill="#3C2E28")
    rect_gradient(far, (0, 58, W - 1, 90), ["#2A7894", "#2E86A0", "#3896AA", "#4CA8B2", "#6AB8B8"])
    rng = Random(61)
    for y in range(60, 88):
        for _ in range(2 + (y - 60) // 6):
            x = rng.randrange(W)
            d.line((x, y, x + 2 + (y - 60) // 7, y), fill="#9AD8D0" if y > 72 else "#6EBCC4")
    fill_gradient(far, lambda m: m.ellipse((160, 54, 206, 64), fill=255), 54, 64, ["#4A7A5A", "#2E5A44"])
    for px in (172, 192):
        d.line((px, 58, px + 1, 50), fill="#3A2E24")
        d.line((px - 3, 50, px + 4, 50), fill="#2E6E3A")
        d.point((px + 1, 49), fill="#2E6E3A")

    def boat(dd, _):
        dd.polygon([(122, 62), (138, 62), (135, 65), (125, 65)], fill="#E8E0D0")
        dd.line((130, 62, 130, 50), fill="#5A4A3A")
        dd.polygon([(131, 51), (131, 61), (139, 61)], fill="#F4F0E4")
        dd.polygon([(129, 52), (129, 61), (123, 61)], fill="#D8D2C2")
    stamp(far, boat, "#1E3A48")
    for x in range(0, W):
        y = 87 + int(1.5 * math.sin(x / 6))
        d.point((x, y), fill="#EEF4EC")
        d.point((x, y + 1), fill="#B8DCD4")
    rect_gradient(far, (0, 89, W - 1, H - 1), ["#AE9E74", "#CDBC8C", "#D2C292", "#C4B282", "#B8A476"])
    inside = draw_path(far, 90, ["#E0D2A4", "#D8C89A", "#CCBC8C"], "#B8A67A", 2)
    noise(d, (0, 92, W, H), ["#E2D6AA", "#AE9C70", "#C0AE80"], 320, 62)

    def palm(dd, base_x, sign):
        pts = [(base_x + sign * int(12 * math.sin(t / 12 * math.pi / 2)), 102 - t * 6) for t in range(12)]
        for i, (px, py) in enumerate(pts[:-1]):
            dd.line((px, py, pts[i + 1][0], pts[i + 1][1]), fill="#7A5A3A", width=4)
        for px, py in pts[1:-1]:
            dd.line((px - 2, py, px + 1, py), fill="#9A7A52")
        tx, ty = pts[-1]
        for ang, length in ((-165, 22), (-130, 20), (-95, 14), (-55, 20), (-15, 22), (20, 16), (160, 16)):
            a = math.radians(ang)
            prev = (tx, ty)
            for s in range(1, length):
                droop = (s / length) ** 2 * 9
                p = (tx + int(s * math.cos(a)), ty + int(s * math.sin(a) * 0.55 + droop))
                dd.line((prev, p), fill="#3E8E48", width=2)
                if s % 3 == 0:
                    dd.line((p[0], p[1], p[0], p[1] + 3), fill="#2E6E3A")
                    dd.point((p[0], p[1] - 1), fill="#74B85E")
                prev = p
        for cx, cy in ((tx - 2, ty + 2), (tx + 1, ty + 3), (tx - 1, ty + 4)):
            dd.ellipse((cx - 1, cy - 1, cx + 2, cy + 2), fill="#6A4A2A")
    stamp(far, lambda dd, _: (palm(dd, 14, 1), palm(dd, 214, -1)), "#16302A")
    m = ImageDraw.Draw(mid)
    for x, y in [(30, 104), (192, 110), (62, 126), (168, 126)]:
        m.ellipse((x, y - 3, x + 6, y + 2), fill="#F2D8C8")
        m.arc((x + 1, y - 2, x + 5, y + 1), 180, 360, fill="#C8907A")
        m.point((x + 3, y - 1), fill="#FFF2E8")
    for x, y in [(16, 118), (206, 98)]:
        for dx, dy in ((-3, 0), (3, 0), (0, -3), (-2, 3), (2, 3)):
            m.line((x, y, x + dx, y + dy), fill="#E07856")
        m.point((x, y), fill="#F4A07A")
    for x, y in [(48, 98), (184, 132)]:
        m.ellipse((x - 5, y - 3, x + 5, y + 3), fill="#7A7A74")
        m.ellipse((x - 4, y - 3, x + 1, y), fill="#A4A49C")
    for x, y in [(8, 132), (88, 100), (150, 120), (222, 124)]:
        for dx, dy in [(-4, -5), (-1, -7), (2, -6), (4, -3)]:
            m.line((x, y, x + dx, y + dy), fill="#6A9A50")
        m.point((x - 1, y - 7), fill="#9AC870")
    m.line((132, 104, 144, 101), fill="#8A6A4A", width=2)
    m.line((132, 103, 144, 100), fill="#B4946A")
    mid.alpha_composite(outline(mid.copy(), "#5A4A36"))
    n = ImageDraw.Draw(near)
    for y in range(top + 6, H, 8):
        for x in range((y * 11) % 17, W, 30):
            if not inside(x + 4, y):
                n.arc((x, y - 2, x + 9, y + 2), 200, 340, fill="#B09C6E")
    noise(n, (0, top + 2, W, H), ["#FFFFF0"], 26, 63)
