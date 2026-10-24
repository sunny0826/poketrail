from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw

if __package__:
    from tools.design_preview import (
        BG, BLUE, GOLD, MINT, MUTED, ROUTE_COLORS, TEXT, Canvas, GeneratedAssets,
        font, title,
    )
    from tools.design_preview_screens import base, scene
else:
    from design_preview import (
        BG, BLUE, GOLD, MINT, MUTED, ROUTE_COLORS, TEXT, Canvas, GeneratedAssets,
        font, title,
    )
    from design_preview_screens import base, scene

FRAME_MS = 50
DURATION_MS = 1500
TARGET_MS = 750
HIT_MS = 850
ANIMATIONS = [
    dict(id="battle-attack", title="普通攻击 · 前冲与受击", page="Battle",
         action="attack", variant="normal", route="forest", partner=4, enemy=25),
    dict(id="battle-dodge", title="闪避 · 侧移与残影", page="Battle",
         action="dodge", variant="normal", route="coast", partner=7, enemy=54),
    dict(id="battle-perfect", title="完美攻击 · 金色爆发", page="Battle",
         action="attack", variant="perfect", route="cave", partner=4, enemy=41),
]


def pulse(time_ms: int, start: int, peak: int, end: int) -> float:
    if time_ms <= start or time_ms >= end:
        return 0.0
    if time_ms < peak:
        return (time_ms - start) / (peak - start)
    return (end - time_ms) / (end - peak)


def battle_state(spec: dict, time_ms: int) -> dict:
    action = spec.get("action", "attack")
    variant = spec.get("variant", "normal")
    attack = action == "attack"
    perfect = variant == "perfect"
    hit = time_ms >= HIT_MS and variant != "miss"
    state = {
        "playerHp": 3,
        "enemyHp": 3,
        "playerOffset": 0,
        "enemyOffset": 0,
        "markerX": max(103, min(363, 233 + (time_ms - TARGET_MS) * 120 // DURATION_MS)),
        "perfectWindow": abs(time_ms - TARGET_MS) <= 70,
        "judgement": time_ms < TARGET_MS,
        "shake": 0,
    }
    if time_ms < TARGET_MS:
        state["message"] = "右键 · 攻击" if attack else "左键 · 躲避"
    else:
        state["message"] = "未命中" if variant == "miss" else ("完美" if perfect else "命中")
    if attack:
        state["playerOffset"] = round(34 * pulse(time_ms, 750, 850, 1150))
        state["enemyOffset"] = round(14 * pulse(time_ms, 850, 925, 1080))
        if hit:
            state["enemyHp"] = 1 if perfect else 2
        if perfect and 850 <= time_ms < 950:
            state["shake"] = (-2, 2, -1, 1)[(time_ms - 850) // 25 % 4]
    else:
        state["enemyOffset"] = -round(30 * pulse(time_ms, 720, 830, 1080))
        state["playerOffset"] = -round(26 * pulse(time_ms, 780, 870, 1120))
        if time_ms >= HIT_MS and variant == "miss":
            state["playerHp"] = 2
    return state


def source_frame(animations: dict, number: int, time_ms: int) -> Image.Image:
    frames = animations[number]
    position = time_ms % sum(duration for _, duration in frames)
    for image, duration in frames:
        if position < duration:
            return image
        position -= duration
    return frames[-1][0]


def actor(c: Canvas, image: Image.Image, x: int, y: int, flash: bool = False,
          opacity: int = 255, shadow: bool = True) -> None:
    if shadow:
        c.draw.ellipse((x - 40, y + 61, x + 40, y + 73), fill="#6C6848")
    if opacity < 255:
        image = image.copy()
        image.putalpha(image.getchannel("A").point(lambda alpha: alpha * opacity // 255))
    if flash:
        silhouette = Image.new("RGBA", image.size, (255, 255, 255, 0))
        silhouette.putalpha(image.getchannel("A"))
        image = silhouette
    c.image.paste(image, (x - image.width // 2, y - image.height // 2), image)


def hp(c: Canvas, name: str, value: int, x: int, color: str) -> None:
    c.text(name, x, 92, 14, TEXT, "lm")
    for index in range(3):
        c.draw.rectangle((x + index * 18, 108, x + 12 + index * 18, 114),
                         fill=color if index < value else "#30443C")


def impact(c: Canvas, x: int, y: int, time_ms: int, color: str, strong: bool) -> None:
    if not 850 <= time_ms < 1150:
        return
    progress = (time_ms - 850) / 300
    count = 8 if strong else 5
    for index in range(count):
        angle = index * math.tau / count + 0.2
        radius = 12 + progress * (55 if strong else 40)
        px = round(x + math.cos(angle) * radius)
        py = round(y + math.sin(angle) * radius * 0.65)
        size = 4 if strong and index % 2 == 0 else 3
        c.draw.rectangle((px - size // 2, py - size // 2,
                          px + size // 2, py + size // 2), fill=color)
    if time_ms < 950:
        radius = 22 if strong else 14
        c.draw.polygon([(x, y - radius), (x + 5, y - 5), (x + radius, y),
                        (x + 5, y + 5), (x, y + radius), (x - 5, y + 5),
                        (x - radius, y), (x - 5, y - 5)], fill=color)


def target_brackets(c: Canvas, x: int, y: int, color: str) -> None:
    for sx, sy in [(-1, -1), (1, -1), (-1, 1), (1, 1)]:
        px, py = x + sx * 48, y + sy * 48
        c.draw.line((px, py, px - sx * 10, py), fill=color, width=2)
        c.draw.line((px, py, px, py - sy * 10), fill=color, width=2)


def render_battle_scene(spec: dict, assets: GeneratedAssets, time_ms: int | None = None,
                        animations: dict | None = None) -> Canvas:
    if time_ms is None:
        time_ms = 900 if spec.get("variant") == "perfect" else 650
    time_ms %= DURATION_MS
    state = battle_state(spec, time_ms)
    route = spec.get("route", "forest")
    partner = spec.get("partner", 4)
    enemy = spec.get("enemy", 25)
    action = spec.get("action", "attack")
    variant = spec.get("variant", "normal")
    animations = animations or {number: assets.sprite_frames(number, 2) for number in {partner, enemy}}
    c = base(spec, assets)
    scene(c, route, True, state["shake"])
    title(c, "免费练习" if spec.get("page") == "Practice" else "节奏对战", "轻触上方暂停", MINT)
    hp(c, assets.name(partner), state["playerHp"], 91, MINT)
    hp(c, assets.name(enemy), state["enemyHp"], 282, GOLD)
    player_x = 155 + state["playerOffset"] + state["shake"]
    enemy_x = 311 + state["enemyOffset"] + state["shake"]
    if action == "dodge":
        if 760 <= time_ms < 1000:
            actor(c, source_frame(animations, partner, time_ms), 155 - 10, 232, opacity=55, shadow=False)
            actor(c, source_frame(animations, partner, time_ms), 155 - 22, 232, opacity=30, shadow=False)
        if 760 <= time_ms < HIT_MS:
            projectile_x = 290 - round((time_ms - 760) * 0.65)
            c.draw.rectangle((projectile_x - 4, 225, projectile_x + 4, 229), fill=BLUE)
    if action == "attack" and time_ms < HIT_MS:
        target_brackets(c, enemy_x, 226, GOLD if state["perfectWindow"] else ROUTE_COLORS[route])
    actor(c, source_frame(animations, partner, time_ms), player_x, 232)
    flash = action == "attack" and variant != "miss" and 850 <= time_ms < 1000 and (time_ms // 50) % 2 == 0
    actor(c, source_frame(animations, enemy, time_ms), enemy_x, 226, flash=flash)
    if action == "attack" and variant != "miss":
        impact(c, enemy_x, 222, time_ms, GOLD if variant == "perfect" else TEXT, variant == "perfect")
    c.draw.line((103, 357, 363, 357), fill="#5B7168", width=3)
    for delta in (-180, -70, 70, 180):
        x = 233 + delta * 120 // DURATION_MS
        c.draw.line((x, 352, x, 362), fill=GOLD if abs(delta) == 70 else MUTED)
    target_color = GOLD if state["perfectWindow"] else MINT
    c.panel((217, 346, 249, 368), "#29453A", target_color, 4, 2)
    marker_gold = state["perfectWindow"] or (variant == "perfect" and time_ms >= TARGET_MS)
    c.draw.ellipse((state["markerX"] - 6, 351, state["markerX"] + 6, 363),
                   fill=GOLD if marker_gold else MINT)
    message_color = GOLD if state["message"] == "完美" else (MUTED if state["message"] == "未命中" else TEXT)
    c.fit(state["message"], 233, 401, 250, 22, message_color)
    c.text("每拍只接受第一次动作", 233, 430, 12, MUTED)
    return c


def storyboard(assets: GeneratedAssets, output: Path) -> None:
    times = [650, 750, 850, 950, 1100, 1450]
    image = Image.new("RGB", (1540, 970), "#0B1210")
    draw = ImageDraw.Draw(image)
    draw.text((30, 30), "战斗动画分镜 · 1500ms / 拍", font=font(26), fill=TEXT, anchor="lm")
    draw.text((30, 63), "判定立即生效；角色动作随后表现，不阻塞下一拍", font=font(14), fill=MUTED, anchor="lm")
    for row, spec in enumerate(ANIMATIONS):
        animations = {number: assets.sprite_frames(number, 2) for number in {spec["partner"], spec["enemy"]}}
        for column, time_ms in enumerate(times):
            frame = render_battle_scene(spec, assets, time_ms, animations).finish()
            thumb = frame.resize((233, 233), Image.Resampling.NEAREST)
            x, y = 20 + column * 250, 100 + row * 285
            image.paste(thumb, (x + 8, y))
            draw.text((x + 125, y + 252), f"{spec['title'].split(' · ')[0]} · {time_ms}ms",
                      font=font(14), fill=TEXT, anchor="mm")
    image.save(output, optimize=True)


def build_battle_animations(assets: GeneratedAssets, out: Path) -> list[dict]:
    (out / "animations").mkdir(parents=True, exist_ok=True)
    manifest = []
    for spec in ANIMATIONS:
        animations = {number: assets.sprite_frames(number, 2) for number in {spec["partner"], spec["enemy"]}}
        frames = [render_battle_scene(spec, assets, index * FRAME_MS, animations).finish()
                  for index in range(DURATION_MS // FRAME_MS)]
        relative = f"animations/{spec['id']}.gif"
        frames[0].save(out / relative, save_all=True, append_images=frames[1:],
                       duration=[FRAME_MS] * len(frames), loop=0, optimize=False, disposal=2)
        state = battle_state(spec, 900)
        manifest.append(dict(id=spec["id"], title=spec["title"], group="battle-animation",
            file=relative, action=spec["action"], variant=spec["variant"], route=spec["route"],
            partner=spec["partner"], enemy=spec["enemy"], durationMs=DURATION_MS,
            frameMs=FRAME_MS, frameCount=len(frames), loop=True, judgement=state["message"],
            playerHp=state["playerHp"], enemyHp=state["enemyHp"]))
    storyboard(assets, out / "contacts" / "battle-animation.png")
    return manifest
