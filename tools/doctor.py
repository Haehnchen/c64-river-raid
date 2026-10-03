#!/usr/bin/env python3
"""Check native build requirements and shipped product assets."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
NATIVE_TOOLS = ("cmake", "ninja", "c++", "ctest")
SHIPPED_ASSETS = (
    "startup_glyphs.hpp", "title_data.hpp", "presentation_palette.hpp",
    "game_data.hpp", "river_generator_data.hpp", "session_presets.hpp",
    "world_data.hpp", "object_sprites.hpp", "bridge_sprites.hpp",
    "auxiliary_sprite.hpp", "demo_clock_data.hpp", "audio_data.hpp",
    "player_data.hpp", "player_detail.hpp", "shot_data.hpp", "regional_timing.hpp",
)


def check_sdl3_config(cmake: str) -> bool:
    """Resolve SDL3 exactly as the product does, in a disposable CMake project."""
    with tempfile.TemporaryDirectory(prefix="river-raid-doctor-") as temporary:
        probe = Path(temporary)
        source = probe / "CMakeLists.txt"
        source.write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(river_raid_doctor_probe LANGUAGES CXX)\n"
            "find_package(SDL3 CONFIG REQUIRED)\n"
            "if(NOT TARGET SDL3::SDL3)\n"
            "  message(FATAL_ERROR \"SDL3 package did not define SDL3::SDL3\")\n"
            "endif()\n",
            encoding="utf-8",
        )
        command = [cmake, "-S", str(probe), "-B", str(probe / "build"), "-G", "Ninja"]
        try:
            result = subprocess.run(command, capture_output=True, text=True, timeout=30)
        except (OSError, subprocess.SubprocessError) as error:
            print(f"ERROR SDL3 CMake config probe failed: {error}")
            return False

    if result.returncode == 0:
        print("OK SDL3 CMake config: find_package(SDL3 CONFIG REQUIRED)")
        return True

    details = (result.stderr or result.stdout).strip()
    print("MISSING SDL3 CMake config: find_package(SDL3 CONFIG REQUIRED) failed")
    if details:
        print(details)
    return False


def check_shipped_assets(root: Path = ROOT) -> bool:
    failed = False
    for name in SHIPPED_ASSETS:
        asset = root / "src/assets" / name
        present = asset.is_file()
        print(f"{'OK' if present else 'MISSING'} shipped asset: {name}")
        failed |= not present
    return not failed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    failed = False
    available = {}
    for name in NATIVE_TOOLS:
        path = shutil.which(name)
        print(f"{'OK' if path else 'MISSING'} required: {name}: {path or '-'}")
        available[name] = path
        failed |= path is None

    if all(available[name] for name in ("cmake", "ninja", "c++")):
        failed |= not check_sdl3_config(available["cmake"])
    else:
        print("SKIP SDL3 CMake config probe: CMake, Ninja, and C++ are required")

    failed |= not check_shipped_assets(ROOT)
    print("Native scope: one standard F1 path through title/options, attract, and flight lifecycle.")
    print("Original parity and full-gameplay acceptance are not claimed.")
    return int(failed)


if __name__ == "__main__":
    sys.exit(main())
