"""Shared incremental host compiler/linker for M5GFX, desktop simulator, and unit suites."""

from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import argparse
import fcntl
import hashlib
import json
import os
import shlex
import shutil
import subprocess
import sys

from host_dependencies import ROOT, library_source

APP = ROOT / "firmware/apps/20_pokedex"
RUNTIME_SOURCES = [APP / name for name in (
    "runtime.cpp runtime_clock.cpp runtime_input.cpp runtime_progression.cpp runtime_power.cpp runtime_language.cpp "
    "clock_sync.cpp screens.cpp screens_home.cpp screens_menu.cpp screens_exploration.cpp screens_battle.cpp "
    "screens_capture.cpp screens_practice.cpp screens_records.cpp screens_clock.cpp "
    "screens_progression.cpp screens_evolution.cpp screens_components.cpp screens_test.cpp "
    "scene_backgrounds_generated.cpp test_mode.cpp test_mode_platform.cpp test_console.cpp "
    "viewer_screen.cpp adventure.cpp adventure_steps.cpp adventure_clock.cpp adventure_capture.cpp "
    "adventure_model.cpp adventure_progression.cpp adventure_migration.cpp battle.cpp "
    "practice_capture.cpp capture_anim.cpp encounter_catalog.cpp icons_generated.cpp "
    "save_codec.cpp motion.cpp calendar.cpp"
).split()]


def run(args):
    subprocess.run([str(arg) for arg in args], cwd=ROOT, check=True)


def _resolve_compiler(is_c: bool, preferred: str | None = None) -> str:
    default = preferred or ("cc" if is_c else "c++")
    compiler = shutil.which(os.environ.get("CC" if is_c else "CXX", default))
    if not compiler:
        raise SystemExit("找不到主机 C/C++ 编译器")
    return compiler


def _object_paths(build_dir: Path, source: Path) -> tuple[Path, Path, Path, Path]:
    resolved = Path(source).resolve()
    relative = resolved.relative_to(ROOT)
    obj = build_dir / relative.with_suffix(relative.suffix + ".o")
    return resolved, obj, obj.with_suffix(".d"), obj.with_suffix(".json")


def _parse_depfile(deps: Path) -> list[str] | None:
    text = deps.read_text()
    if ":" not in text:
        return None
    return shlex.split(text.replace("\\\n", " ").split(":", 1)[1])


def _compile_objects(sources, build_dir: Path, command_for, verified: dict[Path, Path]) -> list[Path]:
    ordered = [Path(source).resolve() for source in sources]
    unique = list(dict.fromkeys(ordered))
    stale = []
    for source in unique:
        if source in verified:
            continue
        resolved, obj, deps, signature = _object_paths(build_dir, source)
        command = command_for(resolved, obj, deps)
        fingerprint = json.dumps(command)
        up_to_date = False
        if obj.exists() and deps.exists() and signature.exists() and signature.read_text() == fingerprint:
            dependencies = _parse_depfile(deps)
            obj_mtime = obj.stat().st_mtime_ns
            if (dependencies is not None and resolved.stat().st_mtime_ns <= obj_mtime and
                    all((ROOT / path).exists() and (ROOT / path).stat().st_mtime_ns <= obj_mtime
                        for path in dependencies)):
                up_to_date = True
        if up_to_date:
            verified[source] = obj
        else:
            stale.append((resolved, obj, signature, command, fingerprint))
    if stale:
        for _resolved, obj, signature, _command, _fingerprint in stale:
            obj.parent.mkdir(parents=True, exist_ok=True)
            signature.unlink(missing_ok=True)

        def compile_one(item):
            resolved, _obj, signature, command, fingerprint = item
            print(f"Compile {resolved.relative_to(ROOT)}", flush=True)
            run(command)
            signature.write_text(fingerprint)

        if len(stale) == 1:
            compile_one(stale[0])
        else:
            workers = min(os.cpu_count() or 4, len(stale))
            with ThreadPoolExecutor(max_workers=workers) as pool:
                for future in [pool.submit(compile_one, item) for item in stale]:
                    future.result()
        for resolved, obj, _signature, _command, _fingerprint in stale:
            verified[resolved] = obj
    return [verified[source] for source in ordered]


def _link_executable(executable: Path, objects: list[Path], command: list[str]) -> Path:
    signature = executable.with_suffix(".link.json")
    fingerprint = json.dumps(command)
    if (not executable.exists() or not signature.exists() or signature.read_text() != fingerprint
            or any(obj.stat().st_mtime_ns > executable.stat().st_mtime_ns for obj in objects)):
        executable.parent.mkdir(parents=True, exist_ok=True)
        signature.unlink(missing_ok=True)
        run(command)
        signature.write_text(fingerprint)
    return executable


class HostBuild:
    def __init__(self):
        self.lib = library_source("M5GFX")
        self.unified = library_source("M5Unified")
        if not shutil.which("sdl2-config"):
            raise SystemExit("需要 SDL2 原生库及 sdl2-config；请参阅 docs/development.md")
        self.build = ROOT / ".pio/host-build"
        self.build.mkdir(parents=True, exist_ok=True)
        flags = shlex.split(subprocess.check_output(["sdl2-config", "--cflags"], text=True))
        self.libs = shlex.split(subprocess.check_output(["sdl2-config", "--libs"], text=True))
        section_flags = [] if sys.platform == "darwin" else ["-ffunction-sections", "-fdata-sections"]
        self.flags = ["-O1", "-DLGFX_SDL", *section_flags, "-isystem", str(self.lib), *flags]
        self.includes = ["-Itest/support", "-Ifirmware/shared", "-I" + str(APP),
                         "-isystem", str(self.unified)]
        self.sources = [
            *sorted((self.lib / "lgfx/utility").glob("*.c")),
            *sorted((self.lib / "lgfx/Fonts/efont").glob("*.c")),
            self.lib / "lgfx/Fonts/IPA/lgfx_font_japan.c",
            self.lib / "lgfx/v1/LGFXBase.cpp", self.lib / "lgfx/v1/lgfx_fonts.cpp",
            self.lib / "lgfx/v1/LGFX_Sprite.cpp",
            *sorted((self.lib / "lgfx/v1/misc").glob("*.cpp")),
            self.lib / "lgfx/v1/panel/Panel_Device.cpp",
            self.lib / "lgfx/v1/platforms/sdl/common.cpp",
            ROOT / "firmware/shared/board.cpp", self.unified / "utility/Button_Class.cpp",
            *[APP / name for name in ("game.cpp", "sprite.cpp", "sprites_generated.cpp", "battery_screen.cpp",
                                     "i18n.cpp", "pokedex_text.cpp", "pokedex_en_generated.cpp")],
        ]
        self._verified_objects: dict[Path, Path] = {}

    def _object_command(self, source: Path, obj: Path, deps: Path) -> list[str]:
        is_c = source.suffix == ".c"
        compiler = _resolve_compiler(is_c)
        return [compiler, *self.flags, *self.includes,
                *([] if is_c else ["-std=gnu++11", "-Wall", "-Wextra"]),
                "-MMD", "-MF", str(deps), "-c", str(source), "-o", str(obj)]

    def object(self, source):
        return _compile_objects([source], self.build, self._object_command, self._verified_objects)[0]

    def dependencies_for(self, sources) -> set[Path]:
        files: set[Path] = set()
        for source in sources:
            resolved, _obj, deps, _sig = _object_paths(self.build, source)
            if resolved.is_relative_to(ROOT) and resolved.is_file():
                files.add(resolved)
            if deps.exists():
                parsed = _parse_depfile(deps)
                if parsed is not None:
                    for path in parsed:
                        candidate = (ROOT / path).resolve()
                        if candidate.is_relative_to(ROOT) and candidate.is_file():
                            files.add(candidate)
        return files

    def executable(self, name, extra_sources):
        with (self.build / "build.lock").open("a") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            return self._executable(name, extra_sources)

    def _executable(self, name, extra_sources):
        objects = _compile_objects([*self.sources, *extra_sources], self.build,
                                   self._object_command, self._verified_objects)
        executable = self.build / name
        linker = ["-Wl,-dead_strip"] if sys.platform == "darwin" else ["-Wl,--gc-sections"]
        command = [_resolve_compiler(False), *map(str, objects), *self.libs, "-lz", *linker,
                   "-o", str(executable)]
        return _link_executable(executable, objects, command)


def build_unit_targets(cxx_default: str, compile_flags: list[str],
                       targets: list[tuple[str, list[Path]]], run_tests: bool = False) -> list[Path]:
    compiler = _resolve_compiler(False, cxx_default)
    flags_hash = hashlib.sha256(json.dumps([compiler, compile_flags]).encode()).hexdigest()[:16]
    build_dir = ROOT / ".pio/host-build/unit" / flags_hash
    build_dir.mkdir(parents=True, exist_ok=True)
    verified: dict[Path, Path] = {}

    def command_for(source: Path, obj: Path, deps: Path) -> list[str]:
        return [compiler, *compile_flags, "-MMD", "-MF", str(deps), "-c", str(source), "-o", str(obj)]

    link_flags = [flag for flag in compile_flags if not flag.startswith(("-I", "-D", "-std=", "-W"))]
    executables: list[Path] = []
    with (build_dir / "build.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        all_sources = [source for _name, sources in targets for source in sources]
        _compile_objects(all_sources, build_dir, command_for, verified)
        for name, sources in targets:
            objects = [verified[Path(source).resolve()] for source in sources]
            executable = build_dir / name
            command = [compiler, *link_flags, *map(str, objects), "-o", str(executable)]
            executables.append(_link_executable(executable, objects, command))
    if run_tests:
        for executable in executables:
            run([executable])
    return executables


def _cli(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    unit = subparsers.add_parser("unit", help="Incremental compile & link for C++ unit test targets")
    unit.add_argument("--cxx", default="clang++")
    unit.add_argument("--shared", nargs="*", default=[])
    unit.add_argument("--target", action="append", required=True, help="NAME:SRC1[,SRC2...]")
    unit.add_argument("--run", action="store_true")
    unit.add_argument("compile_flags", nargs="*")
    args = parser.parse_args(argv)
    shared = [ROOT / item for item in args.shared]
    targets: list[tuple[str, list[Path]]] = []
    for spec in args.target:
        name, raw_sources = spec.split(":", 1)
        extra = [ROOT / item for item in raw_sources.split(",") if item]
        targets.append((name, [*extra, *shared]))
    build_unit_targets(args.cxx, args.compile_flags, targets, run_tests=args.run)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(_cli(sys.argv[1:]))
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
