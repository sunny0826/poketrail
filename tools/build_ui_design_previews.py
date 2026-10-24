#!/usr/bin/env python3
from __future__ import annotations

import html
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw

if __package__:
    from tools.design_preview import GeneratedAssets, GOLD, MINT, MUTED, TEXT, font
    from tools.design_preview_screens import render_frame
    from tools.design_preview_battle import build_battle_animations
else:
    from design_preview import GeneratedAssets, GOLD, MINT, MUTED, TEXT, font
    from design_preview_screens import render_frame
    from design_preview_battle import build_battle_animations

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs/design/ui-scene-redesign"
GROUPS = {
    "exploration": ("探索与菜单", "路线场景、主页、菜单与伙伴列表"),
    "capture": ("遭遇与捕捉", "节奏对战、投球、结果与不可逆确认"),
    "partners": ("伙伴与图鉴", "初始选择、培养、进化与收藏状态"),
    "records": ("步行记录", "首捕卡、日汇总与里程碑"),
    "system": ("日期与异常", "校时、保存重试和存储故障"),
}


def frame(ident: str, page: str, title: str, group: str, **state) -> dict:
    return dict(id=ident, page=page, title=title, group=group, **state)


FRAMES = [
    frame("scene-forest", "Home", "森林 · 场景原画", "exploration", route="forest", scene_only=True),
    frame("scene-coast", "Home", "海边 · 场景原画", "exploration", route="coast", scene_only=True),
    frame("scene-cave", "Home", "洞窟 · 场景原画", "exploration", route="cave", scene_only=True),
    frame("home-forest", "Home", "森林 · 携带小火龙", "exploration", route="forest", species=4),
    frame("home-coast", "Home", "海边 · 携带杰尼龟", "exploration", route="coast", species=7),
    frame("home-cave", "Home", "洞窟 · 携带妙蛙种子", "exploration", route="cave", species=1),
    frame("home-encounter", "Home", "发现宝可梦", "exploration", variant="encounter", encounter=1),
    frame("home-throw-ready", "Home", "等待继续捕捉", "exploration", variant="throw-ready", encounter=1),
    frame("home-unknown-date", "Home", "日期未知", "exploration", unknown_date=True),
    frame("home-step-warning", "Home", "部分步数缺失", "exploration", variant="step-warning"),
    frame("home-recovery", "Home", "备用存档恢复", "exploration", variant="recovery-warning"),
    frame("menu-main", "Menu", "主菜单 · 第一页", "exploration", selected=0, variant="encounter"),
    frame("menu-second", "Menu", "主菜单 · 第二页", "exploration", selected=4),
    frame("menu-last", "Menu", "主菜单 · 末页", "exploration", selected=8),
    frame("menu-recovery", "Menu", "菜单 · 恢复警告", "exploration", variant="warning"),
    frame("routes", "Routes", "路线选择", "exploration", selected=1),
    frame("partners", "Partners", "伙伴列表", "exploration", selected=1),
    frame("partners-long", "Partners", "伙伴列表 · 长名称", "exploration", selected=6),
    frame("partners-last", "Partners", "伙伴列表 · 末页返回", "exploration", selected=7),

    frame("encounter", "Encounter", "遭遇预告", "capture"),
    frame("battle-attack", "Battle", "对战 · 攻击拍", "capture", action="attack"),
    frame("battle-dodge", "Battle", "对战 · 躲避拍", "capture", action="dodge"),
    frame("battle-perfect", "Battle", "对战 · 完美判定", "capture", variant="perfect"),
    frame("practice", "Practice", "捕获练习", "capture", action="attack"),
    frame("practice-throw", "PracticeThrow", "练习投球 · 不消耗精灵球", "capture"),
    frame("practice-result", "PracticeResult", "练习成功 · 未收入图鉴", "capture", variant="practice-captured"),
    frame("pause", "Pause", "实战暂停", "capture"),
    frame("pause-practice", "Pause", "练习暂停", "capture", variant="practice"),
    frame("throw-menu", "Throw", "投球准备", "capture"),
    frame("throw-zero", "Throw", "零球 · 保留遭遇", "capture", balls=0),
    frame("throw-active", "Throw", "蓄力瞄准", "capture", variant="active"),
    frame("capture-flight", "Throw", "捕获 01 · 飞行", "capture", stage="flight", stage_name="飞行"),
    frame("capture-flash", "Throw", "捕获 02 · 白闪", "capture", stage="flash", stage_name="白闪"),
    frame("capture-drop", "Throw", "捕获 03 · 落地", "capture", stage="drop", stage_name="落地"),
    frame("capture-wiggle", "Throw", "捕获 04 · 摇摆", "capture", stage="wiggle", stage_name="摇摆"),
    frame("capture-success", "Throw", "捕获 05 · 成功揭示", "capture", stage="reveal-success", stage_name="成功"),
    frame("capture-escape", "Throw", "捕获 06 · 挣脱揭示", "capture", stage="reveal-failure", stage_name="挣脱"),
    frame("release-confirm", "ReleaseConfirm", "放弃捕捉确认", "capture"),
    frame("result-captured", "Result", "结果 · 首次捕获", "capture", variant="captured"),
    frame("result-lost", "Result", "结果 · 对战失败", "capture", variant="lost"),
    frame("result-retreated", "Result", "结果 · 撤退", "capture", variant="retreated"),
    frame("result-released-owned", "Result", "结果 · 已捕获后放弃", "capture", variant="released-owned"),
    frame("result-released-new", "Result", "结果 · 放弃新捕捉", "capture", variant="released-unowned"),
    frame("result-migration", "Result", "结果 · 旧遭遇结清", "capture", variant="migration"),
    frame("result-left", "Result", "结果 · 宝可梦离开", "capture", variant="left"),

    frame("starter", "Starter", "初始伙伴选择", "partners", selected=1),
    frame("viewer-owned", "Viewer", "图鉴 · 当前拥有", "partners"),
    frame("viewer-unowned", "Viewer", "图鉴 · 尚未获得", "partners", variant="unowned"),
    frame("viewer-reacting", "Viewer", "图鉴 · 互动", "partners", variant="reacting"),
    frame("viewer-memory-error", "Viewer", "图鉴 · 动画内存不足", "partners", variant="memory-error"),
    frame("partner-ready", "PartnerDetail", "伙伴详情 · 进化就绪", "partners", variant="ready"),
    frame("partner-saved", "PartnerDetail", "伙伴详情 · 进化已保存", "partners", variant="saved"),
    frame("partner-owned", "PartnerDetail", "伙伴详情 · 目标已拥有", "partners", variant="owned"),
    frame("partner-unready", "PartnerDetail", "伙伴详情 · 步数不足", "partners", variant="unready"),
    frame("evolution-confirm", "EvolutionConfirm", "进化确认", "partners"),

    frame("records", "Records", "记录目录", "records"),
    frame("records-last", "Records", "记录目录 · 末页", "records", selected=4),
    frame("first-capture", "FirstCaptures", "永久首捕卡", "records"),
    frame("first-capture-empty", "FirstCaptures", "首捕卡 · 初始赠送空态", "records", variant="empty", species=1),
    frame("recent", "Recent", "最近捕捉", "records"),
    frame("days", "Days", "每日步数", "records"),
    frame("achievements", "Achievements", "里程碑 · 已达成", "records"),
    frame("achievements-empty", "Achievements", "里程碑 · 未达成", "records", variant="empty"),

    frame("date-welcome", "DateWelcome", "首次日期确认", "system"),
    frame("clock", "Clock", "日期与时间", "system"),
    frame("clock-editing", "Clock", "校时 · 编辑分钟", "system", variant="editing", selected=4),
    frame("clock-error", "Clock", "校时 · 错误恢复", "system", variant="error", selected=7),
    frame("error-save", "Error", "保存失败 · 重试同次结果", "system", variant="save"),
    frame("error-storage", "Error", "存储未初始化", "system", variant="storage"),
    frame("error-corrupt", "Error", "存档损坏 · 保留原文件", "system", variant="corrupt"),
    frame("error-unsupported", "Error", "存档版本较新", "system", variant="unsupported"),
    frame("error-graphics", "Error", "画面不可用", "system", variant="graphics"),
    frame("home-partial-day", "Home", "今日已记录 · 部分日", "exploration", variant="partial-day"),
    frame("date-pending", "DateWelcome", "上次校时未完成", "system", variant="pending"),
    frame("viewer-history", "Viewer", "图鉴 · 曾经获得", "partners", variant="history"),
    frame("partner-final", "PartnerDetail", "伙伴详情 · 最终形态", "partners", variant="final"),
]

OVERVIEW = ["home-forest", "home-coast", "home-cave", "menu-main", "battle-attack", "viewer-owned"]


def contact_sheet(ident: str, heading: str, specs: list[dict]) -> Path:
    columns, cell_w, cell_h = 3, 310, 342
    rows = math.ceil(len(specs) / columns)
    image = Image.new("RGB", (columns * cell_w + 40, rows * cell_h + 100), "#0B1210")
    draw = ImageDraw.Draw(image)
    draw.text((30, 28), heading, font=font(26), fill=TEXT, anchor="lm")
    draw.text((30, 62), f"{len(specs)} 帧 · 原生单帧 466×466", font=font(14), fill=MUTED, anchor="lm")
    for index, spec in enumerate(specs):
        x = 20 + (index % columns) * cell_w
        y = 86 + (index // columns) * cell_h
        frame_image = Image.open(OUT / "frames" / f"{spec['id']}.png")
        thumb = frame_image.resize((280, 280), Image.Resampling.NEAREST)
        image.paste(thumb, (x + 15, y))
        draw.text((x + 155, y + 302), spec["title"], font=font(16), fill=TEXT, anchor="mm")
        draw.text((x + 155, y + 324), spec["page"], font=font(11, True), fill=MUTED, anchor="mm")
    path = OUT / "contacts" / f"{ident}.png"
    image.save(path, optimize=True)
    return path


def gallery(specs: list[dict], animations: list[dict]) -> str:
    groups = dict(GROUPS)
    groups["battle-animation"] = ("战斗动画", "前冲、受击、闪避与完美判定")
    for group, (heading, description) in groups.items():
        figures = []
        source = animations if group == "battle-animation" else specs
        for spec in source:
            if group != "battle-animation" and spec["group"] != group:
                continue
            path = spec.get("file", f"frames/{spec['id']}.png")
            figures.append(f'''<figure id="{spec['id']}">
  <a href="{path}" aria-label="打开{html.escape(spec['title'])}原生图">
    <img src="{path}" width="466" height="466" loading="lazy" alt="{html.escape(spec['title'])}">
  </a>
  <figcaption><span>{spec.get('page', 'GIF')}</span><h3>{html.escape(spec['title'])}</h3></figcaption>
</figure>''')
        (OUT / f"{group}.html").write_text(f'''<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><link rel="stylesheet" href="style.css"></head><body>
<section id="{group}"><div class="section-head"><p>{description}</p><h2>{heading}</h2></div>
<div class="grid">{''.join(figures)}</div></section></body></html>''', encoding="utf-8")
    sections = [f'<object data="{group}.html" type="text/html" aria-label="{value[0]}"></object>'
                for group, value in groups.items()]
    nav = "".join(f'<a href="{key}.html">{value[0]}</a>' for key, value in groups.items())
    return f'''<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>PokeTrail · 旅途观测仪</title><link rel="stylesheet" href="style.css"></head><body>
<header><p class="kicker">POKETRAIL / FIELD INSTRUMENT 01</p><h1>旅途观测仪</h1><p class="lead">全场景与 UI 视觉重设计 · 复古像素 / 圆屏 / 离线步行</p>
<span class="badge">独立设计提案 · 非 Runtime / 实机截图 · 示例数据</span>
<div class="stats"><div><b>25 / 25</b><span>正式页面</span></div><div><b>3</b><span>路线场景</span></div><div><b>{len(specs)}</b><span>静态设计帧</span></div><div><b>{len(animations)}</b><span>战斗动画</span></div><div><b>466 px</b><span>原生圆屏</span></div></div></header>
<nav aria-label="设计分组">{nav}</nav>
<div class="overview"><img src="contacts/overview.png" alt="六张核心设计总览"></div>
{''.join(sections)}
<footer>安全半径 228 · 整数倍像素缩放 · 保留现有输入与玩法<br><a href="../ui-scene-redesign.md">阅读设计文档</a> · <a href="manifest.json">覆盖清单 JSON</a></footer>
</body></html>'''


def main() -> None:
    (OUT / "frames").mkdir(parents=True, exist_ok=True)
    (OUT / "contacts").mkdir(parents=True, exist_ok=True)
    assets = GeneratedAssets()
    for spec in FRAMES:
        render_frame(spec, assets).save(OUT / "frames" / f"{spec['id']}.png", optimize=True)
    by_id = {spec["id"]: spec for spec in FRAMES}
    contact_sheet("overview", "核心视觉总览", [by_id[ident] for ident in OVERVIEW])
    for group, (heading, _) in GROUPS.items():
        contact_sheet(group, heading, [spec for spec in FRAMES if spec["group"] == group])
    animations = build_battle_animations(assets, OUT)
    manifest = dict(schema=2, kind="independent-design-proposal", nativeSize=[466, 466],
        safeRadius=228, source=["firmware/apps/20_pokedex/icons_generated.cpp",
        "firmware/apps/20_pokedex/sprites_generated.cpp"], frames=FRAMES, animations=animations)
    (OUT / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (OUT / "index.html").write_text(gallery(FRAMES, animations), encoding="utf-8")
    print(f"generated {len(FRAMES)} design frames, {len(animations)} battle GIFs, 7 contact sheets: {OUT}")


if __name__ == "__main__":
    main()
