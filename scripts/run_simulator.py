#!/usr/bin/env python3
"""Build and launch the native desktop simulator using the pinned firmware libraries."""

import argparse
from datetime import datetime, timezone
from pathlib import Path
import os
import subprocess
import sys

from host_build import APP, ROOT, HostBuild, RUNTIME_SOURCES


def main():
    parser = argparse.ArgumentParser(description="PokeTrail SDL 桌面模拟器")
    parser.add_argument("--save-dir", type=Path, default=ROOT / ".simulator/save",
                        help="独立双槽存档目录；换目录即可新开游戏")
    parser.add_argument("--build-only", action="store_true", help="只构建模拟器")
    args = parser.parse_args()
    sources = [ROOT / "simulator" / name for name in ("main.cpp", "app.cpp", "platform.cpp")]
    executable = HostBuild().executable("poketrail-simulator", [*sources, *RUNTIME_SOURCES, APP / "file_store.cpp"])
    if args.build_only:
        print(executable)
        return
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    save = args.save_dir.expanduser().resolve()
    shots = ROOT / ".simulator/screenshots" / stamp
    save.mkdir(parents=True, exist_ok=True)
    shots.mkdir(parents=True, exist_ok=True)
    command = [str(executable), str(save.resolve()), str(shots.resolve())]
    print("←/A 左键 · →/D 右键 · Enter 双键 · 鼠标触摸/滑动 · W 走路 · Tab 16倍 · P 电源 · F12 截图", flush=True)
    os.execv(str(executable), command)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"模拟器失败：{error}", file=sys.stderr)
        sys.exit(1)
