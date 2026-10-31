#!/usr/bin/env python3
"""Regenerate the frozen schema3 fixture with its historical encoder, not HEAD."""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

from host_build import ROOT

REVISION = "53ebf94"
SOURCE = "firmware/apps/20_pokedex"
OUTPUT = ROOT / "test/fixtures/simulator"
NAMES = ("adventure_model.hpp", "adventure_model.cpp", "save_codec.hpp", "save_codec.cpp",
         "calendar.hpp", "calendar.cpp", "encounter_catalog.hpp", "encounter_catalog.cpp",
         "adventure_catalog_generated.hpp", "progression_catalog_generated.hpp")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    commit = subprocess.check_output(["git", "rev-parse", REVISION], cwd=ROOT, text=True).strip()
    generator = ROOT / "test/simulator/legacy_fixture.cpp.txt"
    sources = {}
    OUTPUT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="poketrail-legacy-") as temporary:
        work = Path(temporary)
        for name in NAMES:
            content = subprocess.check_output(["git", "show", f"{commit}:{SOURCE}/{name}"], cwd=ROOT)
            (work / name).write_bytes(content)
            sources[name] = sha(content)
        (work / "fixture.cpp").write_bytes(generator.read_bytes())
        executable = work / "fixture"
        subprocess.run(["c++", "-std=c++11", "-O1", "-I", str(work), *map(str, work.glob("*.cpp")),
                        "-o", str(executable)], check=True)
        subprocess.run([str(executable), str(OUTPUT / "schema3-rule2.bin")], check=True)
    manifest = {"sourceCommit": commit, "sources": sources,
                "generator": str(generator.relative_to(ROOT)), "generatorSha256": sha(generator.read_bytes()),
                "fixtures": {"schema3-rule2.bin": sha((OUTPUT / "schema3-rule2.bin").read_bytes())}}
    (OUTPUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(OUTPUT / "manifest.json")


if __name__ == "__main__":
    main()
