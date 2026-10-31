"""Shared, pinned M5GFX/Runtime build for host rendering and the desktop app."""

from pathlib import Path
import fcntl
import json
import os
import shlex
import shutil
import subprocess
import sys

from host_dependencies import ROOT, library_source

APP = ROOT / "firmware/apps/20_pokedex"
RUNTIME_SOURCES = [APP / name for name in (
    "runtime.cpp runtime_clock.cpp runtime_input.cpp runtime_progression.cpp runtime_power.cpp "
    "clock_sync.cpp screens.cpp screens_home.cpp screens_exploration.cpp screens_battle.cpp "
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
        self.flags = ["-O1", "-DLGFX_SDL", "-isystem", str(self.lib), *flags]
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
            *[APP / name for name in ("game.cpp", "sprite.cpp", "sprites_generated.cpp", "battery_screen.cpp")],
        ]

    def object(self, source):
        source = Path(source).resolve()
        relative = source.relative_to(ROOT)
        obj = self.build / relative.with_suffix(relative.suffix + ".o")
        obj.parent.mkdir(parents=True, exist_ok=True)
        deps, signature = obj.with_suffix(".d"), obj.with_suffix(".json")
        is_c = source.suffix == ".c"
        compiler = shutil.which(os.environ.get("CC" if is_c else "CXX", "cc" if is_c else "c++"))
        if not compiler:
            raise SystemExit("找不到主机 C/C++ 编译器")
        command = [compiler, *self.flags, *self.includes,
                   *([] if is_c else ["-std=gnu++11", "-Wall", "-Wextra"]),
                   "-MD", "-MF", str(deps), "-c", str(source), "-o", str(obj)]
        fingerprint = json.dumps(command)
        if obj.exists() and deps.exists() and signature.exists() and signature.read_text() == fingerprint:
            dependencies = shlex.split(deps.read_text().replace("\\\n", " ").split(":", 1)[1])
            if all((ROOT / path).exists() and (ROOT / path).stat().st_mtime_ns <= obj.stat().st_mtime_ns
                   for path in dependencies):
                return obj
        print(f"Compile {relative}", flush=True)
        signature.unlink(missing_ok=True)
        run(command)
        signature.write_text(fingerprint)
        return obj

    def executable(self, name, extra_sources):
        with (self.build / "build.lock").open("a") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            return self._executable(name, extra_sources)

    def _executable(self, name, extra_sources):
        objects = [self.object(source) for source in [*self.sources, *extra_sources]]
        executable = self.build / name
        linker = ["-Wl,-dead_strip"] if sys.platform == "darwin" else ["-Wl,--gc-sections"]
        command = [os.environ.get("CXX", "c++"), *map(str, objects), *self.libs, "-lz", *linker,
                   "-o", str(executable)]
        signature = executable.with_suffix(".link.json")
        fingerprint = json.dumps(command)
        if (not executable.exists() or not signature.exists() or signature.read_text() != fingerprint
                or any(obj.stat().st_mtime_ns > executable.stat().st_mtime_ns for obj in objects)):
            signature.unlink(missing_ok=True)
            run(command)
            signature.write_text(fingerprint)
        return executable
