#!/usr/bin/env python3
"""Regenerate frozen old-schema fixtures with their historical encoders, not HEAD."""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

from host_build import ROOT

SOURCE = "firmware/apps/20_pokedex"
OUTPUT = ROOT / "test/fixtures/simulator"
CODEC = ("adventure_model.hpp", "adventure_model.cpp", "save_codec.hpp", "save_codec.cpp",
         "calendar.hpp", "calendar.cpp", "encounter_catalog.hpp", "encounter_catalog.cpp",
         "adventure_catalog_generated.hpp", "progression_catalog_generated.hpp")
FIXTURES = {
    "schema3-rule2.bin": ("53ebf94", "test/simulator/legacy_fixture.cpp.txt", CODEC),
    "schema6-coast.bin": ("05432f6", "test/simulator/schema6_fixture.cpp.txt",
                          CODEC + ("adventure_migration.hpp", "adventure_migration.cpp",
                                   "large_object.hpp", "route_masks_v4_generated.hpp")),
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def build(name, revision, generator, names):
    commit = subprocess.check_output(["git", "rev-parse", revision], cwd=ROOT, text=True).strip()
    generator = ROOT / generator
    sources = {}
    with tempfile.TemporaryDirectory(prefix="poketrail-legacy-") as temporary:
        work = Path(temporary)
        for source in names:
            content = subprocess.check_output(["git", "show", f"{commit}:{SOURCE}/{source}"], cwd=ROOT)
            (work / source).write_bytes(content)
            sources[source] = sha(content)
        (work / "fixture.cpp").write_bytes(generator.read_bytes())
        executable = work / "fixture"
        subprocess.run(["c++", "-std=c++11", "-O1", "-I", str(work), *map(str, work.glob("*.cpp")),
                        "-o", str(executable)], check=True)
        subprocess.run([str(executable), str(OUTPUT / name)], check=True)
    return {"sourceCommit": commit, "sources": sources,
            "generator": str(generator.relative_to(ROOT)), "generatorSha256": sha(generator.read_bytes())}


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    provenance = {name: build(name, *spec) for name, spec in FIXTURES.items()}
    manifest = {"provenance": provenance,
                "fixtures": {name: sha((OUTPUT / name).read_bytes()) for name in FIXTURES}}
    (OUTPUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(OUTPUT / "manifest.json")


if __name__ == "__main__":
    main()
