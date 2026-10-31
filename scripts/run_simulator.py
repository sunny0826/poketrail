#!/usr/bin/env python3
"""Build and launch the native desktop simulator using the pinned firmware libraries."""

import argparse
import fcntl
import hashlib
from datetime import datetime, timezone
from pathlib import Path
import os
import subprocess
import sys

from host_build import APP, ROOT, HostBuild, RUNTIME_SOURCES


def boundary_checks(executable, save, shots, output):
    command = [str(executable), str(save.resolve()), str(shots.resolve()), "--reload"]
    before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in save.glob("save-*.bin")}
    with (save / "session.lock").open("r+") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=15)
        if result.returncode == 0 or "无法锁定" not in result.stderr:
            raise RuntimeError("同一存档目录未拒绝第二个进程")
    after = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in save.glob("save-*.bin")}
    if before != after:
        raise RuntimeError("拒绝并发启动时改变了存档")
    print("PASS concurrent launch refused without changing saves")
    corrupt = output / "corrupt-save"
    corrupt.mkdir()
    for name in ("save-a.bin", "save-b.bin"):
        (corrupt / name).write_bytes(b"intentionally invalid save")
    subprocess.run([str(executable), str(corrupt.resolve()), str(shots.resolve()), "--corrupt"],
                   cwd=ROOT, check=True, timeout=30)
    if any(p.read_bytes() != b"intentionally invalid save" for p in corrupt.glob("save-*.bin")):
        raise RuntimeError("损坏存档被覆盖")
    print("PASS corrupt saves remain unchanged")


def main():
    parser = argparse.ArgumentParser(description="PokeTrail SDL 桌面模拟器")
    parser.add_argument("--save-dir", type=Path, default=ROOT / ".simulator/save",
                        help="独立双槽存档目录；换目录即可新开游戏")
    parser.add_argument("--build-only", action="store_true", help="只构建模拟器")
    parser.add_argument("--test", action="store_true", help="用隔离存档验证 SDL 输入、截图与重启")
    args = parser.parse_args()
    sources = [ROOT / "simulator" / name for name in ("main.cpp", "app.cpp", "platform.cpp", "smoke.cpp")]
    executable = HostBuild().executable("poketrail-simulator", [*sources, *RUNTIME_SOURCES, APP / "file_store.cpp"])
    if args.build_only:
        print(executable)
        return
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    if args.test:
        output = Path(os.environ.get("MYM5_VERIFICATION_DIR", str(ROOT / ".pio/verification" / f"simulator-{stamp}")))
        output.mkdir(parents=True, exist_ok=False)
        save = output / "save"
        shots = output / "screenshots"
    else:
        save = args.save_dir.expanduser().resolve()
        shots = ROOT / ".simulator/screenshots" / stamp
    save.mkdir(parents=True, exist_ok=True)
    shots.mkdir(parents=True, exist_ok=True)
    command = [str(executable), str(save.resolve()), str(shots.resolve())]
    print("←/A 左键 · →/D 右键 · Enter 双键 · 鼠标触摸/滑动 · W 走路 · Tab 16倍 · P 电源 · F12 截图", flush=True)
    if args.test:
        subprocess.run([*command, "--smoke"], cwd=ROOT, check=True, timeout=120)
        subprocess.run([*command, "--reload"], cwd=ROOT, check=True, timeout=30)
        boundary_checks(executable, save, shots, output)
        from PIL import Image
        for image in shots.glob("*.bmp"):
            with Image.open(image) as source:
                source.save(image.with_suffix(".png"))
        print(f"Simulator verification: {output.resolve()}")
    else:
        os.execv(str(executable), command)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"模拟器失败：{error}", file=sys.stderr)
        sys.exit(1)
