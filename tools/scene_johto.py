"""城都的分层场景，由 design_preview_scenes 按地区调用。"""
from __future__ import annotations

from random import Random

from PIL import ImageDraw

if __package__:
    from tools.scene_paint import (
        H, W, canopy, fill_gradient, ground_texture, outline, path_mask, rect_gradient, stamp, trunk,
    )
else:
    from scene_paint import (
        H, W, canopy, fill_gradient, ground_texture, outline, path_mask, rect_gradient, stamp, trunk,
    )


def johto(far, mid, near, top):
    d = ImageDraw.Draw(far)
    rect_gradient(far, (0, 0, W - 1, 72), ["#241C3C", "#34244A", "#4E2C52", "#743A56", "#A04E58", "#C8684E", "#E08C58"])
    for r, color in ((18, "#C86A52"), (14, "#DE8A56")):
        fill_gradient(far, lambda m, r=r: m.ellipse((66 - r, 52 - r, 66 + r, 52 + r), fill=255), 30, 70,
                      [color, color])
    d.ellipse((57, 43, 75, 61), fill="#F4C474")
    d.ellipse((59, 45, 69, 53), fill="#FAE0A0")
    for x, y, w in [(10, 20, 50), (110, 14, 60), (150, 30, 50), (20, 34, 30)]:
        d.line((x, y, x + w, y), fill="#7A4460")
        d.line((x + 6, y + 1, x + w - 4, y + 1), fill="#E89A78")
    fill_gradient(far, lambda m: m.polygon([(0, 62), (26, 46), (48, 54), (84, 36), (118, 52), (146, 42), (180, 54),
                                            (W, 44), (W, 80), (0, 80)], fill=255), 36, 80, ["#7A4660", "#6A3C56"])
    fill_gradient(far, lambda m: m.polygon([(0, 74), (30, 60), (64, 68), (100, 58), (140, 70), (190, 60), (W, 66),
                                            (W, 84), (0, 84)], fill=255), 58, 84, ["#563048", "#46283E"])
    for a, b in [((26, 46), (18, 52)), ((84, 36), (72, 44)), ((146, 42), (136, 48)), ((100, 58), (90, 63))]:
        d.line((a, b), fill="#B86A62")

    def roofs(dd, _):
        for x, base, w in [(4, 80, 20), (26, 78, 16), (46, 80, 22), (70, 79, 14)]:
            dd.rectangle((x, base - 6, x + w, base), fill="#4A3034")
            dd.polygon([(x - 2, base - 6), (x + w + 2, base - 6), (x + w - 2, base - 10), (x + 2, base - 10)],
                       fill="#3A3A48")
            dd.line((x + 2, base - 10, x + w - 2, base - 10), fill="#6A6A7A")
            dd.point((x - 3, base - 7), fill="#3A3A48")
            dd.point((x + w + 3, base - 7), fill="#3A3A48")
            for wx in range(x + 3, x + w - 2, 6):
                dd.rectangle((wx, base - 4, wx + 2, base - 2), fill="#F0B460")
    stamp(far, roofs, "#1E1424")

    def torii(dd, _):
        x = 36
        dd.rectangle((x - 8, 66, x - 6, 82), fill="#C0443A")
        dd.rectangle((x + 6, 66, x + 8, 82), fill="#C0443A")
        dd.line((x - 7, 66, x - 7, 82), fill="#E06A50")
        dd.rectangle((x - 9, 68, x + 9, 69), fill="#C0443A")
        dd.polygon([(x - 13, 62), (x + 13, 62), (x + 11, 65), (x - 11, 65)], fill="#2A1E22")
        dd.point((x - 14, 61), fill="#2A1E22")
        dd.point((x + 14, 61), fill="#2A1E22")
        dd.rectangle((x - 11, 65, x + 11, 66), fill="#C0443A")
    stamp(far, torii, "#1A1018")

    def pagoda(dd, _):
        cx = 184
        dd.rectangle((cx - 16, 80, cx + 16, 84), fill="#7A6A64")
        dd.line((cx - 16, 80, cx + 16, 80), fill="#9A8A82")
        for i in range(5):
            bw = 12 - i * 2
            base = 80 - i * 11
            dd.rectangle((cx - bw, base - 7, cx + bw, base), fill="#5A3A3E")
            for px in range(cx - bw + 2, cx + bw, 3):
                dd.line((px, base - 6, px, base - 1), fill="#3E2830")
            dd.line((cx - bw, base - 1, cx + bw, base - 1), fill="#8A5A4A")
            dd.rectangle((cx - 1, base - 5, cx + 1, base - 3), fill="#F4BC62")
            rw = bw + 6
            dd.polygon([(cx - rw, base - 7), (cx + rw, base - 7), (cx + rw - 5, base - 11), (cx - rw + 5, base - 11)],
                       fill="#34444A")
            dd.line((cx - rw + 5, base - 11, cx + rw - 5, base - 11), fill="#5A7070")
            dd.line((cx - rw, base - 7, cx - rw + 5, base - 11), fill="#D08452")
            dd.point((cx - rw - 1, base - 8), fill="#34444A")
            dd.point((cx - rw - 2, base - 9), fill="#34444A")
            dd.point((cx + rw + 1, base - 8), fill="#34444A")
            dd.point((cx + rw + 2, base - 9), fill="#34444A")
        for k in range(6):
            dd.line((cx - 1, 25 - k * 2, cx + 1, 25 - k * 2), fill="#E0B050" if k % 2 == 0 else "#9A7434")
        dd.line((cx, 14, cx, 12), fill="#F2D06A")
    stamp(far, pagoda, "#1A1220")
    maple = ("#6E2626", "#B0402C", "#DC703A", "#F2B04E")
    stamp(far, lambda dd, _: (trunk(dd, 8, 62, 86, "#3A2422", "#5E3A30"), canopy(dd, 8, 56, 17, maple, 21),
                              trunk(dd, 60, 70, 84, "#3A2422", "#5E3A30"), canopy(dd, 60, 68, 9, maple, 22),
                              trunk(dd, 148, 72, 84, "#3A2422", "#5E3A30"), canopy(dd, 148, 70, 8, maple, 23),
                              trunk(dd, 222, 60, 86, "#3A2422", "#5E3A30"), canopy(dd, 222, 54, 16, maple, 24)),
          "#24121A")
    rect_gradient(far, (0, 84, W - 1, H - 1), ["#6E5440", "#624A38", "#563F30", "#4C3628"])
    inside = path_mask(84)
    widths = [2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7]
    y = 84
    row = 0
    while y < H:
        hgt = widths[min(row, len(widths) - 1)]
        t = (y - 84) / (H - 84)
        half = int(4 + t * 22)
        cuts = sorted([118 - half, 118 - half // 3 + (row % 2) * 3, 118 + half // 3 + (row % 2) * 3, 118 + half])
        for a, b in zip(cuts, cuts[1:]):
            if b - a < 3 or y + 1 > H - 1:
                continue
            tone = "#A0968A" if (row + a) % 3 else "#92887A"
            d.rectangle((a + 1, y + 1, b - 1, min(H - 1, y + hgt - 1)), fill=tone)
            d.line((a + 1, y + 1, b - 1, y + 1), fill="#BAB0A0")
        y += hgt + 1
        row += 1
    ground_texture(d, 86, H, ["#7E6048", "#4A3426", "#6A7A44"], 31, inside)
    m = ImageDraw.Draw(mid)

    def lantern(x, y):
        m.rectangle((x - 5, y + 15, x + 5, y + 17), fill="#7A7268")
        m.rectangle((x - 2, y + 9, x + 2, y + 15), fill="#8E867A")
        m.line((x - 2, y + 9, x - 2, y + 15), fill="#B0A898")
        m.rectangle((x - 4, y + 7, x + 4, y + 9), fill="#7A7268")
        m.rectangle((x - 3, y + 1, x + 3, y + 7), fill="#5E564C")
        m.rectangle((x - 1, y + 2, x + 1, y + 6), fill="#F6C866")
        m.point((x, y + 3), fill="#FFF0B0")
        m.polygon([(x - 7, y + 1), (x, y - 3), (x + 7, y + 1)], fill="#6A6258")
        m.line((x - 7, y + 1, x, y - 3), fill="#9A9284")
        m.point((x - 8, y), fill="#6A6258")
        m.point((x + 8, y), fill="#6A6258")
        m.rectangle((x - 1, y - 5, x + 1, y - 3), fill="#8E867A")
    lantern(28, top + 20)
    lantern(204, top + 24)
    for x, y in [(70, 120), (170, 110), (12, 104), (214, 128)]:
        for dx, dy in [(-3, -3), (-1, -4), (1, -4), (3, -2)]:
            m.line((x, y, x + dx, y + dy), fill="#56603A")
        m.point((x - 1, y - 4), fill="#7E8A4C")
    mid.alpha_composite(outline(mid.copy(), "#2A1C18"))
    n = ImageDraw.Draw(near)
    rng = Random(52)
    for _ in range(46):
        x, y = rng.randrange(2, W - 2), rng.randrange(top + 2, H - 1)
        c = rng.choice(["#C8402C", "#E07A3A", "#F0B04A", "#A8322A"])
        n.point((x, y), fill=c)
        n.point((x + 1, y), fill=c)
        n.point((x, y - 1), fill=c)
