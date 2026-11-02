"""关都的分层场景，由 design_preview_scenes 按地区调用。"""
from __future__ import annotations

import math
from random import Random

from PIL import ImageDraw

if __package__:
    from tools.scene_paint import (
        H, W, canopy, cloud, draw_path, fill_gradient, ground_texture, noise, outline, rect_gradient, stamp, trunk,
    )
else:
    from scene_paint import (
        H, W, canopy, cloud, draw_path, fill_gradient, ground_texture, noise, outline, rect_gradient, stamp, trunk,
    )


def kanto(far, mid, near, top):
    d = ImageDraw.Draw(far)
    rect_gradient(far, (0, 0, W - 1, 64), ["#1E3C58", "#284E6A", "#34627A", "#4E7E8C", "#729C9C", "#94B4A4"])
    for x, y, w in [(20, 16, 34), (138, 9, 46), (196, 28, 24), (84, 30, 20)]:
        cloud(d, x, y, w, "#7A9CA4", "#A8C4C0", "#D2E2D6")
    fill_gradient(far, lambda m: m.polygon([(0, 58), (22, 44), (40, 50), (66, 34), (92, 48), (120, 40), (150, 50),
                                            (176, 36), (204, 46), (W, 40), (W, 66), (0, 66)], fill=255),
                  34, 66, ["#6E96A0", "#5E8692", "#55808A"])
    for a, b in [((22, 44), (16, 48)), ((66, 34), (56, 42)), ((120, 40), (112, 45)), ((176, 36), (166, 44))]:
        d.line((a, b), fill="#8EB0B4")
    fill_gradient(far, lambda m: (m.ellipse((-40, 52, 96, 110), fill=255), m.ellipse((56, 56, 176, 112), fill=255),
                                  m.ellipse((136, 50, 270, 110), fill=255)), 50, 84, ["#4C845A", "#3E744C", "#336642"])
    noise(d, (0, 52, W, 80), ["#5A9464", "#2E5C3C"], 160, 3)
    for x in range(4, W, 9):
        y = 58 + int(4 * math.sin(x / 17)) + (6 if 70 < x < 160 else 0)
        canopy(d, x, y, 4, ("#24503A", "#336A48", "#4C8A58", "#6CA866"), x)

    def house(dd, cx, base, w, roof, wall, glass):
        h = int(w * 0.55)
        rh = int(w * 0.42)
        dd.rectangle((cx - w // 2, base - h, cx + w // 2, base), fill=wall[1])
        dd.rectangle((cx + w // 4, base - h, cx + w // 2, base), fill=wall[0])
        dd.line((cx - w // 2, base - h, cx - w // 2, base), fill=wall[2])
        dd.polygon([(cx - w // 2 - 3, base - h), (cx + w // 2 + 3, base - h),
                    (cx + w // 2 - 1, base - h - rh), (cx - w // 2 + 1, base - h - rh)], fill=roof[1])
        for yy in range(base - h - rh + 2, base - h, 2):
            dd.line((cx - w // 2, yy, cx + w // 2, yy), fill=roof[0])
        dd.line((cx - w // 2 + 1, base - h - rh, cx + w // 2 - 1, base - h - rh), fill=roof[2])
        dd.polygon([(cx - w // 2 - 3, base - h), (cx - w // 2 + 1, base - h - rh), (cx - w // 2 + 3, base - h - rh),
                    (cx - w // 2, base - h)], fill=roof[2])
        dd.rectangle((cx + w // 4, base - h - rh - 3, cx + w // 4 + 2, base - h - rh + 2), fill=wall[0])
        for wx in ([cx - w // 2 + 3] + ([cx + 3] if w > 24 else [])):
            dd.rectangle((wx, base - h + 3, wx + 4, base - h + 6), fill=glass[0])
            dd.point((wx + 1, base - h + 4), fill=glass[1])
        dx = cx + w // 2 - 6 if w <= 24 else cx - 2
        dd.rectangle((dx, base - 6, dx + 3, base), fill="#6A4A36")
        dd.point((dx + 2, base - 3), fill="#E0C070")

    red = ("#8A3A30", "#B04C3C", "#D46E52")
    blue = ("#46566E", "#5E7290", "#8094B0")
    wall = ("#B4A88C", "#D6CCAE", "#ECE4CA")
    stamp(far, lambda dd, _: (house(dd, 24, 76, 22, red, wall, ("#7EBAC8", "#E8F4F0")),
                              house(dd, 58, 78, 18, red, wall, ("#7EBAC8", "#E8F4F0")),
                              house(dd, 172, 77, 32, blue, wall, ("#7EBAC8", "#E8F4F0")),
                              house(dd, 208, 75, 18, red, wall, ("#7EBAC8", "#E8F4F0"))), "#1E2C2C")
    stamp(far, lambda dd, _: (trunk(dd, 4, 60, 86, "#4A3A28", "#6E5A3C"),
                              canopy(dd, 4, 52, 15, ("#1E4A30", "#2E6A42", "#468A50", "#6CAE62"), 7),
                              trunk(dd, 224, 58, 86, "#4A3A28", "#6E5A3C"),
                              canopy(dd, 224, 50, 16, ("#1E4A30", "#2E6A42", "#468A50", "#6CAE62"), 9)), "#14281E")
    rect_gradient(far, (0, 80, W - 1, H - 1), ["#5A9050", "#4C8246", "#40743E", "#36663A"])
    inside = draw_path(far, 79, ["#CDB27A", "#BDA06A", "#A88C5A"], "#7E6844", 1)
    ground_texture(d, 82, H, ["#2E5A32", "#6AA05A", "#467C42"], 11, inside)
    noise(d, (96, 82, 142, H), ["#9A8050", "#D8C08A", "#8E7448"], 120, 12, inside)

    def fence(dd, _):
        for x0, x1 in [(0, 98), (138, W)]:
            dd.line((x0, 79, x1, 79), fill="#9A8050")
            dd.line((x0, 82, x1, 82), fill="#9A8050")
            for x in range(x0 + 2, x1, 7):
                dd.rectangle((x, 76, x + 1, 84), fill="#B89A62")
                dd.point((x, 76), fill="#D8BE86")
    stamp(far, fence, "#3A2E1E")

    m = ImageDraw.Draw(mid)
    for x0, y0, x1, y1 in [(6, 92, 78, 106), (156, 96, 222, 110), (14, 116, 82, 130), (152, 118, 214, 131)]:
        for row, y in enumerate(range(y0, y1, 5)):
            for x in range(x0 + (3 if row % 2 else 0), x1, 7):
                m.polygon([(x, y + 5), (x + 2, y), (x + 3, y + 3), (x + 4, y), (x + 6, y + 5)], fill="#3E7A3A")
                m.line((x + 2, y, x + 2, y + 2), fill="#86C062")
                m.line((x + 4, y, x + 4, y + 1), fill="#86C062")
    mid.alpha_composite(outline(mid.copy(), "#1C3E20"), (0, 0))
    m = ImageDraw.Draw(mid)
    m.rectangle((86, 104, 87, 114), fill="#6A4A30")
    m.rectangle((80, 98, 93, 105), fill="#C8A86A")
    m.line((82, 100, 91, 100), fill="#7A5E3A")
    m.line((82, 102, 89, 102), fill="#7A5E3A")
    m.line((80, 98, 93, 98), fill="#E2C88E")
    n = ImageDraw.Draw(near)
    rng = Random(42)
    for _ in range(22):
        x = rng.choice([rng.randint(4, 88), rng.randint(148, 224)])
        y = rng.randint(top + 4, 131)
        petal = rng.choice(["#E85A4A", "#F2EEDC", "#F4CF5A", "#E88AB0"])
        for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            n.point((x + dx, y + dy), fill=petal)
        n.point((x, y), fill="#F6E27A" if petal != "#F4CF5A" else "#C8862A")
        n.point((x, y + 2), fill="#2C5A2C")
