#!/usr/bin/env python3
"""探测 M5Stack StopWatch 的串口。

ESP32-S3 的 USB CDC 在下载模式下会枚举出 /dev/cu.usbmodem* 或
/dev/cu.usbserial-*。这里挑最可能的那个：优先 usbmodem，其次 usbserial。

用法：
  mise exec -- python3 scripts/detect_port.py
"""

from __future__ import annotations

import glob
import sys


def candidates() -> list[str]:
    modem = sorted(glob.glob("/dev/cu.usbmodem*"))
    serial = sorted(glob.glob("/dev/cu.usbserial-*"))
    usb = sorted(glob.glob("/dev/cu.usb*"))
    # 去重并保持优先级
    seen: set[str] = set()
    ordered: list[str] = []
    for path in modem + serial + usb:
        if path not in seen:
            seen.add(path)
            ordered.append(path)
    return ordered


def main() -> int:
    ports = candidates()
    if not ports:
        print("", file=sys.stderr)
        return 1
    # 排除明显不是开发板的（蓝牙、内部调制解调器）
    filtered = [p for p in ports if "Bluetooth" not in p and "modem" not in p.lower() or "usbmodem" in p.lower()]
    chosen = (filtered or ports)[0]
    print(chosen)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
