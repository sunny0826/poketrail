from __future__ import annotations

if __package__:
    from tools.design_preview import (
        BG, BLUE, GOLD, MINT, MUTED, PANEL, PANEL_2, PURPLE, RED, SPECIES_COUNT, TEXT,
        Canvas, GeneratedAssets, alert, battery, glyph, menu, poke_count, progress,
        title,
    )
    from tools.design_preview_screens import base, sprite, star
else:
    from design_preview import (
        BG, BLUE, GOLD, MINT, MUTED, PANEL, PANEL_2, PURPLE, RED, SPECIES_COUNT, TEXT,
        Canvas, GeneratedAssets, alert, battery, glyph, menu, poke_count, progress,
        title,
    )
    from design_preview_screens import base, sprite, star

RECORDS_FIRST_CAPTURES_LABEL = f"永久首捕 · {SPECIES_COUNT}种"


def viewer_counter(number: int) -> str:
    return f"#{number:03d} / {SPECIES_COUNT}"


def first_capture_footer(number: int) -> str:
    return f"{number:03d} / {SPECIES_COUNT + 1} · 轻触下方返回"


def record_footer(c: Canvas, value: str) -> None:
    c.text("‹", 137, 413, 24, MUTED)
    c.fit(value, 233, 415, 190, 13, MUTED)
    c.text("›", 329, 413, 24, MUTED)


def viewer(spec: dict, assets: GeneratedAssets) -> Canvas:
    c = base(spec, assets, "#091512")
    number = spec.get("species", 25)
    battery(c)
    if spec.get("variant") == "memory-error":
        c.draw.polygon([(233, 153), (184, 239), (282, 239)], outline=RED, width=4)
        c.text("!", 233, 208, 40, RED, mono=True)
        c.text("动画内存不足", 233, 290, 20, RED)
        return c
    c.text(viewer_counter(number), 233, 42, 14, MUTED, mono=True)
    c.text(assets.name(number), 233, 72, 24, TEXT)
    c.draw.ellipse((113, 103, 353, 343), outline="#203B32", width=2)
    c.draw.ellipse((143, 126, 323, 320), outline="#162C25", width=2)
    for x, y in [(125, 147), (335, 159), (131, 294), (329, 283)]:
        c.draw.rectangle((x, y, x + 3, y + 3), fill="#46675A")
    sprite(c, assets, number, 215 if spec.get("variant") != "reacting" else 201, max_height=220)
    if spec.get("variant") == "reacting":
        x, y = 343, 154
        c.draw.ellipse((x - 12, y - 6, x, y + 6), fill="#F38CA0")
        c.draw.ellipse((x, y - 6, x + 12, y + 6), fill="#F38CA0")
        c.draw.polygon([(x - 12, y), (x + 12, y), (x, y + 17)], fill="#F38CA0")
    c.panel((194, 340, 272, 366), PANEL_2, None, 7)
    c.text(assets.meta[number]["types"], 233, 353, 14, BLUE)
    status = {"unowned": "尚未获得", "history": "曾经获得", "reacting": "当前拥有"}.get(spec.get("variant"), "当前拥有")
    c.text(status, 233, 390, 14, GOLD if status == "尚未获得" else MINT)
    c.text("互动 · 按键切换 · 顶部返回", 233, 423, 13, MUTED)
    return c


def partner_detail(spec: dict, assets: GeneratedAssets) -> Canvas:
    c = base(spec, assets)
    variant = spec.get("variant", "ready")
    if variant in ("owned", "unready"):
        number = 138
    elif variant == "saved":
        number = 5
    elif variant == "final":
        number = 6
    else:
        number = 4
    target = None if variant == "final" else (139 if number == 138 else 5)
    title(c, assets.name(number), "陪伴 6,750 · 可用 6,750 步", MINT)
    c.paste(assets.icon(number, 2), 233, 132)
    target_name = assets.name(target) if target else ""
    messages = {
        "ready": (f"{target_name}需 7,500 步", GOLD),
        "saved": ("进化已保存", MINT),
        "owned": ("已拥有此形态", GOLD),
        "unready": ("培养步数还不够", GOLD),
        "final": ("已是最终形态", MUTED),
    }
    message, color = messages[variant]
    alert(c, message, 188, color)
    items = [{"label": "已携带" if variant != "saved" else "选择携带", "icon": "partner"}]
    if target:
        items.append({"label": assets.name(target), "species": target,
                      "note": "已拥有" if variant == "owned" else ("未达标" if variant == "unready" else "")})
    items.append({"label": "返回伙伴列表", "icon": "back"})
    menu(c, items, spec.get("selected", 1))
    return c


def evolution(spec: dict, assets: GeneratedAssets) -> Canvas:
    c = base(spec, assets)
    title(c, "确认进化", f"{assets.name(4)} → {assets.name(5)}", GOLD)
    c.paste(assets.icon(4, 2), 164, 127)
    c.text("→", 233, 127, 24, GOLD)
    c.paste(assets.icon(5, 2), 302, 127)
    c.text("使用 7,500 培养步", 233, 168, 16, TEXT)
    c.text("原形态不再可携带", 233, 193, 13, GOLD)
    menu(c, [{"label": "确认进化", "icon": "next"}, {"label": "稍后", "icon": "back"}], 0, GOLD)
    c.text("余下培养步留在原形态", 233, 326, 13, MUTED)
    c.text("已捕获的原形态不会再遇到", 233, 351, 13, GOLD)
    return c


def release(spec: dict, assets: GeneratedAssets) -> Canvas:
    c = base(spec, assets)
    title(c, "放弃这次捕捉？", assets.name(25), RED)
    c.draw.polygon([(233, 105), (202, 160), (264, 160)], outline=RED, width=3)
    c.text("!", 233, 142, 26, RED, mono=True)
    c.text("不退探索步数或已用精灵球", 233, 179, 14, TEXT)
    c.text("伙伴与已有收藏保留", 233, 199, 12, MUTED)
    menu(c, [{"label": "确认放弃", "icon": "back"}, {"label": "返回投球", "icon": "next"}], 0, RED)
    return c


def resources(spec: dict, assets: GeneratedAssets) -> Canvas:
    c = base(spec, assets)
    title(c, "资源与数据", "", GOLD)
    c.text("今日 3,842 步 · 森林", 233, 80, 14, MINT)
    poke_count(c, assets, 5, 327, 110)
    c.text("小火龙 · 培养", 233, 132, 14, MUTED)
    c.text("6,750 / 7,500 步", 233, 156, 16, TEXT)
    alert(c, "下次遭遇还差 1,600 步", 183, GOLD)
    menu(c, [{"label": "捕捉与步行记录", "icon": "records"},
             {"label": "日期与时间", "icon": "clock"},
             {"label": "返回", "icon": "back"}], spec.get("selected", 0), GOLD)
    return c


def records(spec: dict, assets: GeneratedAssets) -> Canvas:
    c = base(spec, assets)
    title(c, "捕捉与步行记录", "每日最高 8,432 步", GOLD)
    c.draw.rectangle((207, 124, 259, 168), outline=GOLD, width=2)
    c.draw.line((233, 124, 233, 168), fill=GOLD, width=2)
    for y in (135, 145, 155):
        c.draw.line((214, y, 226, y), fill=GOLD)
        c.draw.line((240, y, 252, y), fill=GOLD)
    items = [{"label": RECORDS_FIRST_CAPTURES_LABEL, "icon": "book"},
             {"label": "最近32次捕捉", "icon": "records"},
             {"label": "今日与14日日汇总", "icon": "records"},
             {"label": "每日与长期里程碑", "icon": "records"},
             {"label": "返回", "icon": "back"}]
    menu(c, items, spec.get("selected", 0), GOLD)
    return c


def capture_card(spec: dict, assets: GeneratedAssets, recent: bool = False) -> Canvas:
    c = base(spec, assets)
    number = spec.get("species", 25)
    title(c, "最近捕捉" if recent else "永久首捕卡", f"#{number:03d} {assets.name(number)}", GOLD)
    empty = spec.get("variant") == "empty"
    if empty:
        c.paste(assets.icon(number, 2), 233, 162)
        alert(c, "初始赠送 · 尚无实际首捕", 230, GOLD)
        c.text("自由浏览不会产生首捕卡", 233, 271, 14, MUTED)
    else:
        sprite(c, assets, number, 157)
        c.text("森林 · 累计捕捉 1 次", 233, 229, 15, TEXT)
        c.text("2026-10-24 09:30", 233, 258, 14, GOLD, mono=True)
        c.text("UTC+08:00 · 完整日", 233, 282, 13, MUTED)
        c.fit("今日 3,842 步 · 累计 48,210 步", 233, 315, 300, 14, TEXT)
        c.text("每日 1千 3千  ·  累计 1万", 233, 349, 14, GOLD)
    record_footer(c, "01 / 32 · 轻触下方返回" if recent else first_capture_footer(number))
    return c


def days(spec: dict, assets: GeneratedAssets) -> Canvas:
    c = base(spec, assets)
    title(c, "每日步数", "2026-10-24 · 计时段 2", BLUE)
    c.text("3,842", 233, 141, 42, TEXT, mono=True)
    c.text("步 · 完整日", 233, 181, 14, MUTED)
    for index, (threshold, reached) in enumerate(((1000, True), (3000, True), (5000, False))):
        y = 235 + index * 48
        color = MINT if reached else "#3A4A43"
        c.draw.ellipse((126, y - 10, 146, y + 10), outline=color, width=2)
        if reached:
            c.draw.line((131, y, 136, y + 5, 142, y - 5), fill=color, width=2)
        c.text(f"{threshold:,}", 161, y, 16, TEXT if reached else MUTED, "lm", True)
        c.text("已达成" if reached else "未达成", 337, y, 14, color, "rm")
    record_footer(c, "01 / 16 · 轻触下方返回")
    return c


def achievements(spec: dict, assets: GeneratedAssets) -> Canvas:
    c = base(spec, assets)
    reached = spec.get("variant") != "empty"
    title(c, "里程碑", "累计 10,000 步" if reached else "每日 5,000 步", GOLD)
    c.draw.ellipse((183, 106, 283, 206), outline=GOLD if reached else "#3A4A43", width=3)
    medal = GOLD if reached else "#3A4A43"
    c.draw.polygon([(233, 126), (240, 149), (263, 156), (240, 163),
                    (233, 186), (226, 163), (203, 156), (226, 149)], fill=medal)
    c.draw.polygon([(202, 191), (216, 187), (211, 224)], fill=GOLD if reached else "#3A4A43")
    c.draw.polygon([(250, 187), (264, 191), (255, 224)], fill=GOLD if reached else "#3A4A43")
    if reached:
        c.text("2026-10-24 09:30", 233, 253, 15, GOLD, mono=True)
        c.text("UTC+08:00 · 完整日", 233, 282, 13, MUTED)
        c.text("当日 8,432 步 · 累计 48,210 步", 233, 319, 14, TEXT)
    else:
        c.text("尚未达成", 233, 254, 18, GOLD)
        progress(c, 0.77, 298, GOLD)
        c.text("3,842 / 5,000 步", 233, 326, 14, TEXT, mono=True)
    record_footer(c, "04 / 07 · 轻触下方返回")
    return c


CLOCK_ITEMS = [
    {"label": "年", "note": "2026", "icon": "clock"},
    {"label": "月", "note": "10", "icon": "clock"},
    {"label": "日", "note": "24", "icon": "clock"},
    {"label": "时", "note": "09", "icon": "clock"},
    {"label": "分", "note": "30", "icon": "clock"},
    {"label": "时区分钟", "note": "480", "icon": "clock"},
    {"label": "校时模式", "note": "普通", "icon": "clock"},
    {"label": "保存并校时", "icon": "next"},
    {"label": "返回", "icon": "back"},
]


def clock(spec: dict, assets: GeneratedAssets) -> Canvas:
    c = base(spec, assets)
    editing = spec.get("variant") == "editing"
    title(c, "左减 · 右加 · 双键完成" if editing else "日期与时间",
          "2026-10-24  09:30", BLUE)
    if spec.get("variant") == "error":
        alert(c, "日期无效或校时失败，请重新确认", 169, RED)
    else:
        c.text("手动输入当前时间", 233, 157, 14, MUTED)
    menu(c, CLOCK_ITEMS, spec.get("selected", 4 if editing else 0), BLUE,
         footer="选择字段 · 双键编辑")
    return c


def error(spec: dict, assets: GeneratedAssets) -> Canvas:
    c = base(spec, assets)
    variant = spec.get("variant", "save")
    if variant == "graphics":
        battery(c)
        c.draw.polygon([(233, 153), (184, 239), (282, 239)], outline=RED, width=4)
        c.text("!", 233, 208, 40, RED, mono=True)
        c.text("画面不可用，请重启", 233, 290, 20, RED)
        return c
    save = variant == "save"
    title(c, "暂时无法保存" if save else "存储不可用", "结果尚未确认" if save else "玩法已暂停", RED)
    c.draw.polygon([(233, 108), (205, 157), (261, 157)], outline=RED, width=3)
    c.text("!", 233, 140, 24, RED, mono=True)
    messages = {
        "save": "重试将保留同一次结果", "storage": "存储未初始化",
        "corrupt": "存档损坏 · 保留原文件", "unsupported": "存档版本较新 · 请升级",
    }
    c.fit(messages[variant], 233, 184, 300, 15, GOLD)
    c.text("图鉴仍可浏览", 233, 209, 13, MUTED)
    items = [{"label": "重试保存 / 读取", "icon": "next"}, {"label": "浏览图鉴", "icon": "book"}]
    menu(c, items, 0, RED)
    return c


def render_detail(spec: dict, assets: GeneratedAssets) -> Canvas:
    page = spec["page"]
    if page == "Viewer": return viewer(spec, assets)
    if page == "PartnerDetail": return partner_detail(spec, assets)
    if page == "EvolutionConfirm": return evolution(spec, assets)
    if page == "ReleaseConfirm": return release(spec, assets)
    if page == "Resources": return resources(spec, assets)
    if page == "Records": return records(spec, assets)
    if page == "FirstCaptures": return capture_card(spec, assets)
    if page == "Recent": return capture_card(spec, assets, True)
    if page == "Days": return days(spec, assets)
    if page == "Achievements": return achievements(spec, assets)
    if page == "Clock": return clock(spec, assets)
    if page == "Error": return error(spec, assets)
    raise ValueError(f"unsupported design page {page}")
