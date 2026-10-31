#!/usr/bin/env python3
"""Serial SDL/Runtime journeys with isolated disk state and process recovery."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import selectors
import signal
import shutil
import subprocess
import sys
import time

from host_build import APP, ROOT, HostBuild, RUNTIME_SOURCES

SEED = 20261031
UTC = 1793376000
CASES = {
    "journey": ("P0-01", "blank", [("journey", "run"), ("journey", "reload")]),
    "capture": ("P0-02", "blank", [("capture", "run"), ("capture", "reload")]),
    "battle-fixed": ("P0-02", "blank", [("capture", "fixed"), ("capture", "reload")]),
    "capture-clock": ("P1-10", "blank", [("capture", "run"), ("clock", "captured"), ("clock", "reload")]),
    "throws": ("P0-03", "throw", [("throws", "run"), ("throws", "reload")]),
    "defer": ("P0-03", "throw", [("throws", "defer"), ("throws", "defer-reload")]),
    **{f"retry-{kind}": ("P0-04", "throw", [("retry", kind), ("retry", "reload")])
       for kind in ("before", "after", "readback")},
    "progression": ("P0-05", "evolution", [("progression", "run"), ("progression", "reload")]),
    "target-owned": ("P0-06", "target-owned", [("conflict", "owned")]),
    "evolution-retry": ("P0-06", "evolution", [("conflict", "retry"), ("conflict", "reload")]),
    "storage": ("P0-07", "starter", []),
    "crash-pause": ("P0-08", "battle", [("crash", "paused"), ("crash", "reload-paused")]),
    "crash-write": ("P0-08", "throw", [("crash", "after-write"), ("crash", "reload-capture")]),
    "crash-partial": ("P0-08", "throw", [("crash", "partial-write"), ("crash", "reload-partial")]),
    "isolation": ("P1-09", "evolution", [("isolation", "run")]),
    "clock": ("P1-10", "blank", [("clock", "run"), ("clock", "reload")]),
    "midnight": ("P1-11", "starter", [("clock", "midnight"), ("clock", "reload")]),
    "migration": ("P1-12", "v1-3.bin", [("migration", "run"), ("migration", "reload")]),
    "migration-activity": ("P1-12", "schema3-activity.bin", [("migration", "activity")]),
    "migration-rule2": ("P1-12", "schema3-rule2.bin", [("migration", "rule2"), ("migration", "rule2-reload")]),
    **{f"lifecycle-{kind}": ("P1-13", "throw" if kind.startswith("throw") else "starter", [("lifecycle", kind)])
       for kind in ("sleep", "close-save", "close-clock", "throw-focus", "throw-close")},
    "edges": ("P1-14", "edges", [("edges", "run")]),
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n")


def hashes(directory):
    return {p.name: digest(p) for p in sorted(directory.iterdir()) if p.is_file() and p.name != "session.lock"}


def events(directory):
    path = directory / "events.jsonl"
    return [json.loads(line) for line in path.read_text().splitlines()] if path.exists() else []


def invoke(executable, save, output, scenario, phase, utc, kill=False):
    output.mkdir()
    command = list(map(str, [executable, save, output, scenario, phase, SEED, utc]))
    start = time.monotonic()
    if kill:
        os.mkfifo(output / "ready.fifo")
    with (output / "stdout.log").open("w") as stdout, (output / "stderr.log").open("w") as stderr:
        process = subprocess.Popen(command, cwd=ROOT, stdout=stdout, stderr=stderr)
        killed = False
        try:
            if kill:
                # A FIFO wakes the parent at the exact frozen boundary; no runtime tick polling.
                with os.fdopen(os.open(output / "ready.fifo", os.O_RDWR | os.O_NONBLOCK), "r") as ready:
                    with selectors.DefaultSelector() as selector:
                        selector.register(ready, selectors.EVENT_READ)
                        if not selector.select(120):
                            raise TimeoutError("kill-ready watchdog expired")
                        if ready.readline().strip() != "kill-ready":
                            raise RuntimeError("unexpected kill handshake")
                        process.kill()
                        killed = True
            code = process.wait(timeout=120)
        except BaseException as error:
            process.kill()
            code = process.wait(timeout=10)
            write_json(output / "result.json", {"command": command, "exitCode": code,
                       "wallSeconds": time.monotonic()-start, "status": "fail", "kill": killed,
                       "error": str(error)})
            raise
    observed = events(output)
    success = code == -signal.SIGKILL and killed if kill else code == 0 and any(e["kind"] == "complete" for e in observed)
    result = {"command": command, "exitCode": code, "wallSeconds": time.monotonic()-start,
              "status": "pass" if success else "fail", "kill": killed}
    write_json(output / "result.json", result)
    if not success:
        errors = [e for e in observed if e["kind"] in ("fail", "failure")]
        raise RuntimeError(str(errors[-1] if errors else (output / "stderr.log").read_text()))
    return observed[-1]["at"] if observed else 0


def storage_checks(executable, base, root):
    import fcntl
    for variant in ("truncated", "both-corrupt", "future", "locked"):
        save = root / variant / "save"
        save.mkdir(parents=True)
        for source in base.glob("save-*.bin"):
            shutil.copy2(source, save / source.name)
        if variant == "truncated":
            (save / "save-b.bin").write_bytes(b"AVD1\x06\x00partial")
        elif variant == "both-corrupt":
            for name in ("save-a.bin", "save-b.bin"):
                (save / name).write_bytes(b"invalid")
        elif variant == "future":
            original = bytearray((save / "save-a.bin").read_bytes())
            original[4:6] = (65535).to_bytes(2, "little")
            (save / "save-b.bin").write_bytes(original)
        # Include an existing clock sidecar to prove it is preserved on failed boot.
        (save / "clock.txt").write_text("0\n")
        before = hashes(save)
        if variant == "locked":
            output = root / variant / "run"
            output.mkdir()
            with (save / "session.lock").open("w") as lock:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                result = subprocess.run(list(map(str, [executable, save, output, "load", "valid", SEED, UTC])),
                                        cwd=ROOT, capture_output=True, text=True, timeout=15)
                (output / "stderr.log").write_text(result.stderr)
                if result.returncode == 0 or "无法锁定" not in result.stderr:
                    raise RuntimeError("second process did not reject locked save")
        else:
            invoke(executable, save, root / variant / "run", "load", "valid" if variant == "truncated" else "error", UTC)
        if variant != "truncated" and before != hashes(save):
            raise RuntimeError(f"{variant} changed protected save files")
        write_json(root / variant / "hashes.json", {"before": before, "after": hashes(save)})


def check_clock_restart(before_path, after_path, offline_seconds):
    before = json.loads(before_path.read_text())
    after = json.loads(after_path.read_text())
    checks = {key: before[key] == after[key] for key in (
        "starter", "partner", "totalSteps", "explorationCredit", "explorationSpent",
        "undatedSteps", "today", "history", "accounts", "ballsSpent", "rosterRevision")}
    checks["clock epoch and timezone"] = all(before["clock"][key] == after["clock"][key] for key in ("epoch", "offset"))
    expected = before["rtcUtc"] + offline_seconds + after["at"] // 1000
    checks["RTC includes offline interval"] = abs(after["rtcUtc"] - expected) <= 1
    evidence = {"checks": checks, "expectedUtc": expected, "observedUtc": after["rtcUtc"],
                "offlineSeconds": offline_seconds, "toleranceSeconds": 1}
    write_json(after_path.parent / "restart-check.json", evidence)
    if not all(checks.values()):
        raise RuntimeError(f"clock restart mismatch: {evidence}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", action="append", choices=CASES)
    parser.add_argument("--suite", choices=("core", "all"), default="all")
    parser.add_argument("--build-only", action="store_true")
    args = parser.parse_args()
    sources = [ROOT / "simulator" / name for name in ("app.cpp", "platform.cpp")]
    sources += sorted((ROOT / "test/simulator").glob("*.cpp"))
    build = HostBuild()
    executable = build.executable("poketrail-e2e", [*sources, *RUNTIME_SOURCES, APP / "file_store.cpp"])
    if args.build_only:
        print(executable)
        return 0
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    root = Path(os.environ.get("MYM5_VERIFICATION_DIR", str(ROOT / ".pio/verification" / f"simulator-e2e-{stamp}"))).resolve()
    root.mkdir(parents=True, exist_ok=False)
    source_files = set(sources + RUNTIME_SOURCES + build.sources +
                       [APP / "file_store.cpp", Path(__file__).resolve(), ROOT / "scripts/host_build.py",
                        ROOT / "mise.toml", ROOT / "platformio.ini", ROOT / "requirements-dev.txt"])
    for dependency in build.build.rglob("*.d"):
        import shlex
        source_files.update((ROOT / p).resolve() for p in shlex.split(
            dependency.read_text().replace("\\\n", " ").split(":", 1)[1]) if (ROOT / p).is_file())
    for directory in (ROOT / "simulator", ROOT / "test/simulator", APP, ROOT / "test/support", ROOT / "firmware/shared"):
        source_files.update(directory.rglob("*.hpp"))
        source_files.update(directory.rglob("*.h"))
    manifest = {"head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                "dirty": subprocess.check_output(["git", "status", "--short"], cwd=ROOT, text=True),
                "sources": {str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p): digest(p) for p in sorted(source_files)},
                "compiler": subprocess.check_output([os.environ.get("CXX", "c++"), "--version"], text=True).splitlines()[0],
                "binary": digest(executable), "seed": SEED, "utc": UTC, "python": sys.version,
                "platform": sys.platform, "sdl": subprocess.check_output(["sdl2-config", "--version"], text=True).strip()}
    write_json(root / "manifest.json", manifest)
    selected = args.case or [name for name, (identifier, _, _) in CASES.items() if args.suite == "all" or identifier.startswith("P0")]
    results = []
    for name in selected:
        identifier, fixture, stages = CASES[name]
        output = root / name
        save = output / "save"
        save.mkdir(parents=True)
        result = {"case": name, "id": identifier, "fixture": fixture, "status": "fail"}
        try:
            if fixture.endswith(".bin"):
                source = ROOT / "test/fixtures" / ("simulator" if fixture == "schema3-rule2.bin" else "progression") / fixture
                if fixture == "schema3-rule2.bin":
                    frozen = json.loads((source.parent / "manifest.json").read_text())["fixtures"]
                    if digest(source) != frozen[fixture]:
                        raise RuntimeError("schema3 historical fixture hash mismatch")
                if fixture == "v1-3.bin":
                    frozen = json.loads((source.parent / "manifest.json").read_text())["fixtures"]
                    if digest(source) != frozen[str(source.relative_to(ROOT))]:
                        raise RuntimeError("historical fixture hash mismatch")
                shutil.copy2(source, save / "save-a.bin")
            else:
                with (output / "fixture.stdout.log").open("w") as out, (output / "fixture.stderr.log").open("w") as err:
                    subprocess.run([str(executable), "--fixture", fixture, str(save)], cwd=ROOT, check=True,
                                   timeout=15, stdout=out, stderr=err)
            write_json(output / "fixture.json", hashes(save))
            shutil.copytree(save, output / "before")
            utc = UTC if name != "midnight" else 1793375990
            previous = None
            offline = 3600 if name == "midnight" else 5
            for index, (scenario, phase) in enumerate(stages):
                kill = scenario == "crash" and not phase.startswith("reload")
                stage = output / f"{index}-{phase}"
                elapsed = invoke(executable, save, stage, scenario, phase, utc, kill)
                if scenario == "capture" and phase == "fixed":
                    from PIL import Image
                    with Image.open(stage / "fixed-cue-0.bmp") as image:
                        cue = hashlib.sha256(image.convert("RGB").crop((50, 24, 518, 492)).tobytes()).hexdigest()
                    # Frozen from the independently reviewed 20261031T033539 capture trace.
                    expected = "b93eac168aa8be654f57a67b57f8b5aeb94e904fbdf3e59fc1d2c7998abfdb61"
                    write_json(stage / "cue-check.json", {"expected": expected, "actual": cue})
                    if cue != expected:
                        raise RuntimeError("fixed battle cue pixels differ from reviewed trace")
                if scenario == "clock" and phase == "reload":
                    check_clock_restart(previous / "closed.json", stage / "reloaded.json", offline)
                previous = stage
                utc += elapsed // 1000 + offline
            if name == "storage":
                storage_checks(executable, save, output)
            result["status"] = "pass"
        except (OSError, RuntimeError, subprocess.SubprocessError, TimeoutError) as error:
            result["error"] = str(error)
        write_json(output / "after.json", hashes(save))
        from PIL import Image
        for image in output.rglob("*.bmp"):
            with Image.open(image) as source:
                source.save(image.with_suffix(".png"))
        write_json(output / "result.json", result)
        results.append(result)
        print(f"{result['status'].upper()} {name}: {result.get('error', '')}", flush=True)
    write_json(root / "result.json", results)
    print(f"E2E evidence: {root}")
    return 1 if any(r["status"] != "pass" for r in results) else 0


if __name__ == "__main__":
    sys.exit(main())
