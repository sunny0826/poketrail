#!/usr/bin/env python3
"""Locate the exact PlatformIO library version used by host regressions."""

import configparser
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]


def library_source(name: str) -> Path:
    config = configparser.ConfigParser()
    config.read(ROOT / "platformio.ini")
    versions = [line.split("@", 1)[1].strip()
                for line in config["c152"]["lib_deps"].splitlines()
                if line.strip().startswith(f"m5stack/{name}@")]
    if len(versions) != 1:
        raise SystemExit(f"platformio.ini must pin exactly one m5stack/{name} version")
    matches = [path.parent / "src"
               for path in (ROOT / ".pio/libdeps/20_pokedex").glob(f"{name}*/library.json")
               if json.loads(path.read_text())["version"] == versions[0]]
    if len(matches) != 1 or not matches[0].is_dir():
        raise SystemExit(f"Missing unique {name} {versions[0]}; run mise run build first")
    return matches[0]


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: host_dependencies.py M5Unified|M5GFX")
    print(library_source(sys.argv[1]))
