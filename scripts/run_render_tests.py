#!/usr/bin/env python3
"""用项目锁定的 M5GFX 在离屏画布上运行渲染回归，无需连接设备。"""

from pathlib import Path
from datetime import datetime, timezone
import os
import shlex
import shutil
import subprocess
import sys

from host_dependencies import library_source

ROOT = Path(__file__).resolve().parent.parent


def run(args):
    subprocess.run(args, cwd=ROOT, check=True)


def main():
    lib = library_source("M5GFX")
    unified = library_source("M5Unified")
    build = ROOT / ".pio/render-test" / lib.parent.name
    print(f"Render tests: {lib}", flush=True)
    if not shutil.which("sdl2-config"):
        raise SystemExit("离屏测试需要 SDL2 原生库及 sdl2-config；固件构建不需要它")
    build.mkdir(parents=True, exist_ok=True)
    flags = shlex.split(subprocess.check_output(
        ["sdl2-config", "--cflags"], text=True))
    libs = shlex.split(subprocess.check_output(
        ["sdl2-config", "--libs"], text=True))
    common = ["-O1", "-DLGFX_SDL", "-isystem", str(lib), *flags]
    linker_flags = ["-Wl,-dead_strip"] if sys.platform == "darwin" else ["-Wl,--gc-sections"]
    sources = [
        *sorted((lib / "lgfx/utility").glob("*.c")),
        *sorted((lib / "lgfx/Fonts/efont").glob("*.c")),
        lib / "lgfx/Fonts/IPA/lgfx_font_japan.c",
        lib / "lgfx/v1/LGFXBase.cpp",
        lib / "lgfx/v1/lgfx_fonts.cpp",
        lib / "lgfx/v1/LGFX_Sprite.cpp",
        *sorted((lib / "lgfx/v1/misc").glob("*.cpp")),
        lib / "lgfx/v1/panel/Panel_Device.cpp",
        lib / "lgfx/v1/platforms/sdl/common.cpp",
    ]
    header_mtime = max(path.stat().st_mtime for path in lib.rglob("*.hpp"))
    objects = []
    for source in sources:
        obj = build / ("_".join(source.relative_to(lib).parts) + ".o")
        objects.append(str(obj))
        if obj.exists() and obj.stat().st_mtime >= max(
                source.stat().st_mtime, header_mtime, Path(__file__).stat().st_mtime):
            continue
        is_c = source.suffix == ".c"
        compiler = os.environ.get("CC" if is_c else "CXX", "cc" if is_c else "c++")
        standard = [] if is_c else ["-std=gnu++11"]
        run([compiler, *common, *standard, "-c", str(source), "-o", str(obj)])
    app = ROOT / "firmware/apps/20_pokedex"
    shared = ["firmware/shared/board.cpp", str(unified / "utility/Button_Class.cpp"),
              str(app / "game.cpp"), str(app / "sprite.cpp"),
              str(app / "sprites_generated.cpp"), str(app / "battery_screen.cpp")]
    adventure_sources = [
        "runtime.cpp", "runtime_clock.cpp", "runtime_input.cpp", "runtime_progression.cpp", "runtime_power.cpp", "screens.cpp",
        "screens_home.cpp", "screens_exploration.cpp", "screens_battle.cpp", "screens_capture.cpp", "screens_practice.cpp", "screens_records.cpp", "screens_clock.cpp", "screens_progression.cpp", "screens_evolution.cpp", "screens_components.cpp", "scene_backgrounds_generated.cpp",
        "viewer_screen.cpp", "adventure.cpp", "adventure_steps.cpp", "adventure_clock.cpp",
        "adventure_capture.cpp", "adventure_model.cpp", "adventure_progression.cpp", "adventure_migration.cpp",
        "battle.cpp", "practice_capture.cpp", "capture_anim.cpp", "encounter_catalog.cpp", "icons_generated.cpp",
        "save_codec.cpp", "motion.cpp", "calendar.cpp",
    ]
    output = Path(os.environ.get("MYM5_VERIFICATION_DIR", str(
        ROOT / ".pio/verification" / datetime.now(timezone.utc).strftime("render-%Y%m%dT%H%M%SZ"))))
    suites = [("test_render", []),
              ("test_button_menu_render", [str(app / name) for name in adventure_sources]),
              ("test_touch_menu_render", [str(app / name) for name in adventure_sources]),
              ("test_adventure_render", [str(app / name) for name in adventure_sources]),
              ("test_capture_render", [str(app / name) for name in adventure_sources]),
              ("test_battle_render", [str(app / name) for name in adventure_sources]),
              ("test_progression_render", [str(app / name) for name in adventure_sources]),
              ("test_evolution_render", [str(app / name) for name in adventure_sources]),
              ("test_evolution_preview_render", [str(app / name) for name in adventure_sources]),
              ("test_thumbnail_render", [str(app / name) for name in adventure_sources]),
              ("test_power_render", [str(app / name) for name in adventure_sources]),
              ("test_power_pages_render", [str(app / name) for name in adventure_sources]),
              ("test_home_render", [str(app / name) for name in adventure_sources])]
    if len(sys.argv) > 1:
        selected = set(sys.argv[1:])
        unknown = selected - {name for name, _ in suites}
        if unknown:
            raise SystemExit(f"未知渲染套件: {', '.join(sorted(unknown))}")
        suites = [(name, sources) for name, sources in suites if name in selected]
    for name, extra_sources in suites:
        screenshots = output / "ui" / name
        screenshots.mkdir(parents=True, exist_ok=True)
        os.environ["MYM5_UI_OUTPUT_DIR"] = str(screenshots.resolve())
        executable = build / name
        run([os.environ.get("CXX", "c++"), *common, "-std=gnu++11",
             "-Wall", "-Wextra", "-Itest/support", "-Ifirmware/shared", "-I" + str(app), "-isystem", str(unified),
             f"test/{name}.cpp", *shared, *extra_sources, *objects,
             *libs, *linker_flags, "-o", str(executable)])
        run([str(executable)])


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
