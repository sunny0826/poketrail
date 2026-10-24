from __future__ import annotations

import math

from PIL import ImageEnhance

if __package__:
    from tools.design_preview import (
        BG, BLUE, CENTER, GOLD, MINT, MUTED, PANEL, PANEL_2, PURPLE, RED,
        ROUTE_COLORS, ROUTE_NAMES, TEXT, Canvas, GeneratedAssets, alert, battery,
        glyph, menu, pixel_scene, poke_count, prepare, progress, title,
    )
else:
    from design_preview import (
        BG, BLUE, CENTER, GOLD, MINT, MUTED, PANEL, PANEL_2, PURPLE, RED,
        ROUTE_COLORS, ROUTE_NAMES, TEXT, Canvas, GeneratedAssets, alert, battery,
        glyph, menu, pixel_scene, poke_count, prepare, progress, title,
    )


def base(spec: dict, assets: GeneratedAssets, background: str = BG) -> Canvas:
    c = Canvas(background)
    prepare(c, assets)
    return c


def scene(c: Canvas, route: str, dim: bool = False, dx: int = 0) -> None:
    image = pixel_scene(route)
    if dim:
        image = ImageEnhance.Brightness(image).enhance(0.52)
    c.paste(image.convert("RGBA"), 233 + dx, 227)


def sprite(c: Canvas, assets: GeneratedAssets, number: int, cy: int,
           frame: int = 0, scale: int | None = None, max_height: int | None = None) -> None:
    image = assets.sprite(number, frame, scale)
    if max_height and image.height > max_height:
        image = assets.sprite(number, frame, 2)
    c.draw.ellipse((193, cy + 61, 273, cy + 73), fill="#6C6848")
    c.paste(image, 233, cy)


def route_tag(c: Canvas, route: str) -> None:
    color = ROUTE_COLORS[route]
    c.panel((76, 68, 145, 94), PANEL, None, 7)
    glyph(c, "route", 91, 81, color)
    c.text(ROUTE_NAMES[route], 107, 81, 14, color, "lm")


def home(spec: dict, assets: GeneratedAssets) -> Canvas:
    c = base(spec, assets)
    route = spec.get("route", "forest")
    partner = spec.get("species", 4)
    scene(c, route)
    if spec.get("scene_only"):
        title(c, ROUTE_NAMES[route], "路线场景 · 像素原画", ROUTE_COLORS[route])
        code = {"forest": "FOREST / 01", "coast": "COAST / 02", "cave": "CAVE / 03"}[route]
        c.text(code, 233, 393, 20, ROUTE_COLORS[route], mono=True)
        c.text("228 × 135  ·  2× INTEGER", 233, 425, 12, MUTED, mono=True)
        return c
    title(c, assets.name(partner), "携带中", ROUTE_COLORS[route])
    route_tag(c, route)
    poke_count(c, assets, spec.get("balls", 5), 333, 81)
    sprite(c, assets, partner, 248, spec.get("frame", 0), max_height=190)
    progress(c, spec.get("encounter", 0.68), 361, ROUTE_COLORS[route])
    if spec.get("unknown_date"):
        c.text("今日 --", 233, 393, 28, TEXT)
    else:
        partial = spec.get("variant") == "partial-day"
        c.text("今日已记录" if partial else "今日", 139 if partial else 158, 396, 13 if partial else 14, MUTED)
        c.text(spec.get("steps", "3,842"), 250 if partial else 237, 393, 34, TEXT, mono=True)
        c.text("步", 329 if partial else 316, 397, 14, MUTED)
    variant = spec.get("variant", "")
    messages = {
        "encounter": ("发现宝可梦 · 侧键打开菜单", GOLD),
        "throw-ready": ("待捕捉 · 侧键打开菜单", GOLD),
        "step-warning": ("部分步数缺失", RED),
        "recovery-warning": ("已从备用存档恢复", GOLD),
    }
    message, color = messages.get(variant, ("左 / 右键打开菜单", MUTED))
    c.fit(message, 233, 428, 245, 14, color)
    return c


MENU_ITEMS = [
    {"label": "暂无遭遇", "icon": "practice"}, {"label": "选择路线", "icon": "route"},
    {"label": "我的伙伴", "icon": "partner"}, {"label": "151 图鉴", "icon": "book"},
    {"label": "捕捉与步行记录", "icon": "records"}, {"label": "日期与时间", "icon": "clock"},
    {"label": "免费练习", "icon": "practice"}, {"label": "立即省电", "icon": "sleep"},
    {"label": "返回探索", "icon": "home"},
]


def main_menu(spec: dict, assets: GeneratedAssets) -> Canvas:
    c = base(spec, assets)
    title(c, "步行探险", "今日 3,842 步 · 森林")
    poke_count(c, assets, spec.get("balls", 5), 327, 110)
    items = [dict(item) for item in MENU_ITEMS]
    if spec.get("variant") in ("encounter", "warning"):
        items[0]["label"] = "继续遭遇"
    if spec.get("variant") == "warning":
        alert(c, "已从备用存档恢复", 183, GOLD)
    else:
        alert(c, "下次遭遇还差 1,600 步", 183, ROUTE_COLORS["forest"])
    c.text("小火龙 · 培养 6,750 / 7,500 步", 113, 139, 14, MUTED, "lm")
    menu(c, items, spec.get("selected", 0))
    return c


def date_welcome(spec: dict, assets: GeneratedAssets) -> Canvas:
    c = base(spec, assets)
    subtitle = "上次校时未完成，请重新确认" if spec.get("variant") == "pending" else "设备离线计时，无需手机"
    title(c, "先确认日期", subtitle, GOLD)
    c.draw.ellipse((204, 104, 262, 162), outline=GOLD, width=3)
    c.draw.line((233, 111, 233, 133, 249, 142), fill=GOLD, width=3)
    c.text("暂不设置仍可累计步数", 233, 179, 14, TEXT)
    c.text("捕捉时间会记为未知", 233, 199, 12, MUTED)
    items = [{"label": "设置日期时间", "icon": "clock"},
             {"label": "暂不设置", "icon": "next"},
             {"label": "浏览图鉴", "icon": "book"}]
    menu(c, items, spec.get("selected", 0), GOLD)
    return c


def starter(spec: dict, assets: GeneratedAssets) -> Canvas:
    c = base(spec, assets)
    selected = spec.get("selected", 1)
    ids = [1, 4, 7]
    title(c, "选择最初的伙伴", "只能领取一次", GOLD)
    if selected < 3:
        sprite(c, assets, ids[selected], 151)
    items = [{"label": assets.name(number), "species": number} for number in ids]
    items.append({"label": "返回", "icon": "back"})
    menu(c, items, selected, GOLD)
    return c


def routes(spec: dict, assets: GeneratedAssets) -> Canvas:
    c = base(spec, assets)
    selected = spec.get("selected", 1)
    route = ["forest", "coast", "cave"][min(selected, 2)]
    title(c, "选择路线", "只影响下一场遭遇", ROUTE_COLORS[route])
    preview = pixel_scene(route).crop((0, 36, 456, 216)).resize((228, 90), resample=0)
    c.paste(preview.convert("RGBA"), 233, 151)
    items = [{"label": "森林", "icon": "route"}, {"label": "海边", "icon": "route"},
             {"label": "洞窟", "icon": "route"}, {"label": "返回", "icon": "back"}]
    menu(c, items, selected, ROUTE_COLORS[route])
    return c


def partners(spec: dict, assets: GeneratedAssets) -> Canvas:
    c = base(spec, assets)
    owned = [1, 4, 7, 25, 122, 138, 139]
    selected = spec.get("selected", 1)
    focus = owned[min(selected, len(owned) - 1)]
    title(c, "我的伙伴", f"{len(owned)} 位已拥有", MINT)
    c.draw.ellipse((185, 91, 281, 181), outline="#29433B", width=2)
    c.paste(assets.icon(focus, 2), 233, 137)
    items = [{"label": assets.name(number), "species": number,
              "note": "携带" if number == 122 else ""} for number in owned]
    items.append({"label": "返回", "icon": "back"})
    menu(c, items, selected)
    return c


def encounter(spec: dict, assets: GeneratedAssets) -> Canvas:
    c = base(spec, assets)
    scene(c, spec.get("route", "forest"), True)
    title(c, "发现宝可梦", f"野生的 {assets.name(25)}", GOLD)
    sprite(c, assets, 25, 151)
    items = [{"label": "开始对战", "icon": "practice"}, {"label": "免费练习", "icon": "practice"},
             {"label": "选择伙伴", "icon": "partner"}, {"label": "稍后再战", "icon": "back"}]
    menu(c, items, 0, GOLD)
    return c


def battle(spec: dict, assets: GeneratedAssets) -> Canvas:
    if __package__:
        from tools.design_preview_battle import render_battle_scene
    else:
        from design_preview_battle import render_battle_scene
    return render_battle_scene(spec, assets)


def pause(spec: dict, assets: GeneratedAssets) -> Canvas:
    c = base(spec, assets, "#09110F")
    title(c, "已暂停", "练习不消耗机会" if spec.get("variant") == "practice" else "继续后进入下一拍", GOLD)
    c.draw.rectangle((216, 120, 229, 164), fill=GOLD)
    c.draw.rectangle((239, 120, 252, 164), fill=GOLD)
    menu(c, [{"label": "继续", "icon": "next"},
             {"label": "确认撤退" if spec.get("variant") != "practice" else "退出练习", "icon": "back"}],
         spec.get("selected", 0), GOLD)
    return c


def throw(spec: dict, assets: GeneratedAssets) -> Canvas:
    c = base(spec, assets)
    scene(c, spec.get("route", "forest"), True)
    practice = spec["page"] == "PracticeThrow"
    title(c, assets.name(25), "捕获练习 · 不消耗精灵球" if practice else "等待投球", GOLD)
    if not practice:
        poke_count(c, assets, spec.get("balls", 5), 327, 109)
    if spec.get("variant") == "active":
        sprite(c, assets, 25, 226)
        c.draw.ellipse((125, 118, 341, 334), outline=MINT, width=3)
        c.draw.ellipse((207, 200, 259, 252), outline=BG, width=6)
        c.draw.ellipse((209, 202, 257, 250), outline=GOLD, width=3)
        c.draw.ellipse((200, 311, 266, 377), outline=GOLD, width=3)
        c.paste(assets.icon(0), 233, 344)
        c.text("按住蓄力 · 松手投球", 233, 399, 16, MINT)
        c.text("未投出也会消耗本球", 233, 426, 13, MUTED)
        return c
    sprite(c, assets, 25, 143, scale=2)
    balls = spec.get("balls", 5)
    alert(c, "每 1000 步获得一球" if not balls else "本场剩余 2 次", 199, GOLD)
    if practice:
        items = [{"label": "准备投球", "icon": "practice"},
                 {"label": "换一只练习", "icon": "partner"},
                 {"label": "结束练习", "icon": "back"}]
    else:
        items = [{"label": "准备投球" if balls else "暂无精灵球", "icon": "practice"},
                 {"label": "保留遭遇 · 继续走路", "icon": "home"},
                 {"label": "放弃这次捕捉", "icon": "back"}]
    menu(c, items, spec.get("selected", 0), GOLD, footer="左键切换 · 右键确认")
    if practice:
        c.text("成功不会收入图鉴", 233, 380, 13, MUTED)
    return c


def star(c: Canvas, x: int, y: int, radius: int = 7, color: str = GOLD) -> None:
    c.draw.line((x - radius, y, x + radius, y), fill=color, width=2)
    c.draw.line((x, y - radius, x, y + radius), fill=color, width=2)


def capture(spec: dict, assets: GeneratedAssets) -> Canvas:
    c = base(spec, assets)
    scene(c, "forest", True)
    title(c, assets.name(25), spec.get("stage_name", "捕获"), GOLD)
    stage = spec["stage"]
    if stage == "flight":
        sprite(c, assets, 25, 220)
        for x, y, r in [(250, 322, 3), (258, 307, 4), (266, 292, 5)]:
            c.draw.ellipse((x-r, y-r, x+r, y+r), fill=GOLD)
        c.paste(assets.icon(0), 274, 278)
    elif stage == "flash":
        c.draw.ellipse((148, 148, 318, 318), outline=TEXT, width=4)
        c.draw.ellipse((191, 191, 275, 275), outline=TEXT, width=2)
    elif stage == "drop":
        c.draw.ellipse((180, 326, 286, 340), outline=MUTED)
        c.paste(assets.icon(0), 233, 282)
    elif stage == "wiggle":
        c.draw.ellipse((180, 326, 286, 340), outline=MUTED)
        ball = assets.icon(0).rotate(20, resample=0, expand=True)
        c.paste(ball, 233, 300)
        star(c, 273, 285)
    elif stage == "reveal-success":
        c.draw.ellipse((188, 255, 278, 345), outline=GOLD, width=3)
        c.paste(assets.icon(0), 233, 300)
        for index in range(6):
            angle = index * math.tau / 6
            star(c, int(233 + math.cos(angle) * 95), int(238 + math.sin(angle) * 95),
                 7, GOLD if index % 2 else TEXT)
    else:
        sprite(c, assets, 25, 218)
        c.draw.ellipse((143, 128, 323, 308), outline=TEXT, width=3)
        c.paste(assets.icon(0), 233, 337)
    return c


RESULTS = {
    "captured": ("捕捉成功", "首次捕获", GOLD),
    "lost": ("对战结束", "伙伴仍在，继续步行探索", MUTED),
    "retreated": ("已经撤退", "伙伴仍在，继续步行探索", MUTED),
    "released-owned": ("已捕获 · 遭遇结束", "伙伴与已有收藏保留", GOLD),
    "released-unowned": ("已放弃捕捉", "不退探索步数或已用精灵球", RED),
    "migration": ("旧遭遇已结清", "升级已退回 500 探索步", GOLD),
    "left": ("宝可梦离开了", "伙伴仍在，继续步行探索", MUTED),
}


def result(spec: dict, assets: GeneratedAssets) -> Canvas:
    c = base(spec, assets)
    variant = spec.get("variant", "captured")
    if variant == "practice-captured":
        title(c, "练习成功", assets.name(25), GOLD)
        sprite(c, assets, 25, 168)
        c.text("未收入图鉴", 233, 262, 16, MUTED)
        menu(c, [{"label": "再练一只", "icon": "practice"}, {"label": "返回探索", "icon": "home"}], 0, GOLD)
        return c
    heading, message, color = RESULTS[variant]
    title(c, heading, assets.name(25), color)
    if variant == "captured":
        c.draw.ellipse((160, 105, 306, 251), outline=GOLD, width=2)
        sprite(c, assets, 25, 178)
        c.text("✦ 首次捕获 ✦", 233, 262, 16, GOLD)
        c.text("2026-10-24 09:30", 233, 288, 13, MUTED, mono=True)
        c.text("森林  ·  每日 1千 / 3千步", 233, 315, 14, TEXT)
        menu(c, [{"label": "确认结果 · 继续探索", "icon": "next"}], 0, GOLD, top=372)
    else:
        c.draw.ellipse((181, 112, 285, 216), outline="#29433B", width=2)
        c.paste(assets.icon(25, 2), 233, 164)
        c.fit(message, 233, 249, 290, 16, color)
        if variant == "migration":
            c.text("升级后每 1000 步获得一球", 233, 277, 14, TEXT)
        menu(c, [{"label": "确认结果 · 继续探索", "icon": "next"}], 0, color, top=355)
    return c


def render_frame(spec: dict, assets: GeneratedAssets):
    page = spec["page"]
    if page == "Home": c = home(spec, assets)
    elif page == "Menu": c = main_menu(spec, assets)
    elif page == "DateWelcome": c = date_welcome(spec, assets)
    elif page == "Starter": c = starter(spec, assets)
    elif page == "Routes": c = routes(spec, assets)
    elif page == "Partners": c = partners(spec, assets)
    elif page == "Encounter": c = encounter(spec, assets)
    elif page in ("Battle", "Practice"): c = battle(spec, assets)
    elif page == "Pause": c = pause(spec, assets)
    elif page == "Throw" and spec.get("stage"): c = capture(spec, assets)
    elif page in ("Throw", "PracticeThrow"): c = throw(spec, assets)
    elif page in ("Result", "PracticeResult"): c = result(spec, assets)
    else:
        if __package__:
            from tools.design_preview_details import render_detail
        else:
            from design_preview_details import render_detail
        c = render_detail(spec, assets)
    return c.finish()
