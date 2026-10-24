from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

if __package__:
    from tools.design_preview import GeneratedAssets, MUTED, ROUTE_COLORS, TEXT, font
    from tools.design_preview_scenes import (
        MIDDLE_PIXELS_PER_STEP, NEAR_PIXELS_PER_STEP, ground_height,
    )
    from tools.design_preview_screens import render_frame
else:
    from design_preview import GeneratedAssets, MUTED, ROUTE_COLORS, TEXT, font
    from design_preview_scenes import MIDDLE_PIXELS_PER_STEP, NEAR_PIXELS_PER_STEP, ground_height
    from design_preview_screens import render_frame

FRAME_MS = 100
FRAME_COUNT = 40
DURATION_MS = FRAME_MS * FRAME_COUNT
STEPS_PER_WALKING_FRAME = 2
WALKING_RANGES = ((0, 8), (16, 32))
STOPPED_RANGES = ((8, 16), (32, 40))
ANIMATIONS = [
    dict(id="walk-forest", title="森林 · 林径后退", route="forest", partner=4),
    dict(id="walk-coast", title="海边 · 岸景后退", route="coast", partner=7),
    dict(id="walk-cave", title="洞窟 · 晶簇后退", route="cave", partner=1),
]


def visual_steps_at(frame_index: int) -> int:
    walked_frames = sum(max(0, min(frame_index, end) - start)
                        for start, end in WALKING_RANGES if frame_index > start)
    return walked_frames * STEPS_PER_WALKING_FRAME


def sprite_frame_at(assets: GeneratedAssets, partner: int, time_ms: int) -> int:
    frames = assets.sprite_frames(partner)
    position = time_ms % sum(duration for _, duration in frames)
    for index, (_, duration) in enumerate(frames):
        if position < duration:
            return index
        position -= duration
    return len(frames) - 1


def render_walk_frame(spec: dict, assets: GeneratedAssets, frame_index: int) -> Image.Image:
    state = dict(spec, page="Home", species=spec["partner"],
                 visual_steps=visual_steps_at(frame_index),
                 frame=sprite_frame_at(assets, spec["partner"], frame_index * FRAME_MS))
    return render_frame(state, assets)


def walking_storyboard(out: Path) -> None:
    labels = ("停止 · 背景冻结", "行走 · 地景靠近", "接缝 · 完整循环")
    image = Image.new("RGB", (1000, 1080), "#0B1210")
    draw = ImageDraw.Draw(image)
    draw.text((30, 30), "步行纵深 · 三路线关键帧", font=font(26), fill=TEXT, anchor="lm")
    draw.text((30, 63), "示例步事件驱动；伙伴与界面固定，远景稳定，地景向屏幕下方靠近", font=font(14), fill=MUTED, anchor="lm")
    assets = GeneratedAssets()
    for row, spec in enumerate(ANIMATIONS):
        frames = [render_walk_frame(spec, assets, 8), render_walk_frame(spec, assets, 4)]
        seam = dict(spec, page="Home", species=spec["partner"],
                    visual_steps=ground_height(spec["route"]) - 1)
        frames.append(render_frame(seam, assets))
        for column, frame in enumerate(frames):
            x, y = 20 + column * 325, 100 + row * 320
            image.paste(frame.resize((280, 280), Image.Resampling.NEAREST), (x + 8, y))
            draw.text((x + 148, y + 294), f"{spec['title'].split(' · ')[0]} · {labels[column]}",
                      font=font(14), fill=ROUTE_COLORS[spec["route"]], anchor="mm")
    image.save(out / "contacts" / "walking-animation.png", optimize=True)


def build_walking_animations(assets: GeneratedAssets, out: Path) -> list[dict]:
    (out / "animations").mkdir(parents=True, exist_ok=True)
    manifest = []
    for spec in ANIMATIONS:
        frames = [render_walk_frame(spec, assets, index) for index in range(FRAME_COUNT)]
        palette_source = Image.new("RGB", (frames[0].width, frames[0].height * len(frames)))
        for index, frame in enumerate(frames):
            palette_source.paste(frame, (0, index * frame.height))
        palette = palette_source.quantize(colors=255, method=Image.Quantize.MEDIANCUT)
        indexed = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
        relative = f"animations/{spec['id']}.gif"
        indexed[0].save(out / relative, save_all=True, append_images=indexed[1:],
                        duration=[FRAME_MS] * FRAME_COUNT, loop=0, optimize=False, disposal=2)
        for name, frame_index in (("stationary", 8), ("moving", 4)):
            frames[frame_index].save(out / "frames" / f"{spec['id']}-{name}.png", optimize=True)
        seam = dict(spec, page="Home", species=spec["partner"],
                    visual_steps=ground_height(spec["route"]) - 1)
        render_frame(seam, assets).save(out / "frames" / f"{spec['id']}-seam.png", optimize=True)
        manifest.append(dict(spec, group="walking-animation", file=relative,
            durationMs=DURATION_MS, frameMs=FRAME_MS, frameCount=FRAME_COUNT, loop=True,
            exampleStepIncrement=STEPS_PER_WALKING_FRAME, exampleVisualSteps=visual_steps_at(FRAME_COUNT),
            walkingFrameRanges=[list(range) for range in WALKING_RANGES],
            stoppedFrameRanges=[list(range) for range in STOPPED_RANGES],
            middlePixelsPerStep=MIDDLE_PIXELS_PER_STEP, nearPixelsPerStep=NEAR_PIXELS_PER_STEP,
            loopSourceHeight=ground_height(spec["route"]), keyframes=[f"frames/{spec['id']}-{name}.png"
            for name in ("stationary", "moving", "seam")], evidence="host-design-preview"))
    walking_storyboard(out)
    return manifest
