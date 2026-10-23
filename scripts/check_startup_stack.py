#!/usr/bin/env python3
"""Check Engine::begin's local Xtensa frame; this is not a call-chain stack bound."""

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys


SYMBOL = "_ZN7pokedex9adventure6Engine5beginEv"
ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, default=ROOT / ".pio/build/20_pokedex/firmware.elf")
    parser.add_argument("--max-frame-bytes", type=int, default=512)
    args = parser.parse_args()
    if args.max_frame_bytes <= 0:
        parser.error("--max-frame-bytes must be positive")
    core = os.environ.get("PLATFORMIO_CORE_DIR")
    if not core:
        parser.error("PLATFORMIO_CORE_DIR is missing; run through the project mise environment")
    objdump = Path(core) / "packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-objdump"
    if not objdump.is_file() or not args.elf.is_file():
        parser.error("target toolchain or ELF is missing; run mise run build first")
    try:
        result = subprocess.run(
            [str(objdump), "-d", f"--disassemble={SYMBOL}", str(args.elf)],
            check=True, capture_output=True, text=True,
        )
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"cannot inspect target ELF: {error}", file=sys.stderr)
        return 2
    match = re.search(
        rf"<{SYMBOL}>:\s*\n\s*[0-9a-f]+:\s+[0-9a-f]+\s+entry\s+a1,\s*(0x[0-9a-f]+|[0-9]+)\b",
        result.stdout,
    )
    if not match:
        print("Engine::begin Xtensa entry instruction was not found", file=sys.stderr)
        return 2
    frame = int(match.group(1), 0)
    passed = frame <= args.max_frame_bytes
    print(f"{'PASS' if passed else 'FAIL'} Engine::begin local frame: {frame} bytes "
          f"(limit {args.max_frame_bytes})")
    print("This guard detects large local allocations; full task stack usage still needs device measurement.")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
