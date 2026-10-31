#!/usr/bin/env python3
"""用项目锁定的 M5GFX 在离屏画布上运行渲染回归，无需连接设备。"""

from pathlib import Path
from datetime import datetime, timezone
import os
import subprocess
import sys

from host_build import HostBuild, ROOT, RUNTIME_SOURCES, run


def main():
    build = HostBuild()
    print(f"Render tests: {build.lib}", flush=True)
    output = Path(os.environ.get("MYM5_VERIFICATION_DIR", str(
        ROOT / ".pio/verification" / datetime.now(timezone.utc).strftime("render-%Y%m%dT%H%M%SZ"))))
    names = ["test_render", "test_button_menu_render", "test_touch_menu_render",
             "test_adventure_render", "test_capture_render", "test_battle_render",
             "test_progression_render", "test_evolution_render", "test_evolution_preview_render",
             "test_thumbnail_render", "test_power_render", "test_power_pages_render",
             "test_home_render", "test_test_mode_render", "test_clock_sync_render"]
    if len(sys.argv) > 1:
        selected = set(sys.argv[1:])
        unknown = selected - set(names)
        if unknown:
            raise SystemExit(f"未知渲染套件: {', '.join(sorted(unknown))}")
        names = [name for name in names if name in selected]
    for name in names:
        print(f"SUITE {name}", flush=True)
        screenshots = output / "ui" / name
        screenshots.mkdir(parents=True, exist_ok=True)
        os.environ["MYM5_UI_OUTPUT_DIR"] = str(screenshots.resolve())
        sources = [] if name == "test_render" else RUNTIME_SOURCES
        executable = build.executable(name, [ROOT / "test" / f"{name}.cpp", *sources])
        run([executable])


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
