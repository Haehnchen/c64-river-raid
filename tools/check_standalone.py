#!/usr/bin/env python3
"""Verify a fresh product-only build and runtime checks; this is not original-parity or full-gameplay acceptance."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CONFIGURE_TIMEOUT_SECONDS = 180
BUILD_TIMEOUT_SECONDS = 600
TEST_TIMEOUT_SECONDS = 600


def standalone_product_files(root=ROOT):
    """Return only files shipped to the fresh product-only build."""
    product = [root / "CMakeLists.txt"]
    for directory in ("src", "tests"):
        product.extend(path for path in (root / directory).rglob("*") if path.is_file())
    return product


def run_native_step(stage, command, timeout, *, cwd=None, env=None):
    """Run a bounded native command with a concise failure diagnostic."""
    try:
        subprocess.run(command, check=True, timeout=timeout, cwd=cwd, env=env)
    except subprocess.TimeoutExpired as error:
        raise SystemExit(
            f"Standalone check timed out during {stage} after {timeout}s: {error.cmd}"
        )
    except subprocess.CalledProcessError as error:
        raise SystemExit(
            f"Standalone check failed during {stage} (exit {error.returncode}): {error.cmd}"
        )
    except OSError as error:
        raise SystemExit(f"Could not start standalone {stage}: {error}")


def check_runtime(executable: Path, runtime: Path):
    """Exercise the native CLI from a directory with no product assets."""
    environment = os.environ.copy()
    environment.pop("WAYLAND_DISPLAY", None)
    environment.update(SDL_VIDEODRIVER="dummy", SDL_RENDER_DRIVER="software", SDL_AUDIODRIVER="dummy")

    def smoke(option):
        run_native_step(option, [str(executable), option], 10, cwd=runtime, env=environment)

    def dump(option, filename, header, description, *arguments):
        run_native_step(option, [str(executable), option, filename, *arguments], 10, cwd=runtime)
        if not (runtime / filename).read_bytes().startswith(header):
            raise SystemExit(f"Standalone preview did not produce {description}")

    smoke("--smoke-test")
    dump("--dump-atlas", "atlas.ppm", b"P6\n", "an atlas")
    dump("--dump-title", "title.ppm", b"P6\n320 200\n", "the title")
    smoke("--smoke-options")
    dump("--dump-options", "options.ppm", b"P6\n320 200\n", "the terrain/HUD view", "1888")
    dump("--dump-sprites", "sprites.ppm", b"P6\n512 224\n", "the sprite sheet")
    smoke("--smoke-sprites")
    dump("--dump-world", "world.ppm", b"P6\n320 200\n", "the world snapshot", "53")
    smoke("--smoke-world")
    smoke("--smoke-flight")


def main():
    product = standalone_product_files(ROOT)
    forbidden = re.compile(r"reference/|analysis/|River_Raid\.crt|/home/|playground-ghostbusters|playground-c64-weatherwar")
    for path in product:
        if path == ROOT / "src/assets/river_raid.ico":
            continue
        contents = path.read_text()
        if forbidden.search(contents):
            raise SystemExit(f"External reference in product source: {path.relative_to(ROOT)}")
        if path.is_relative_to(ROOT / "src") and re.search(r"fixtures/|river_row_cases|river_long_trace|object_motion_cases|demo_timing_cases", contents):
            raise SystemExit(f"Test trace referenced by runtime source: {path.relative_to(ROOT)}")

    with tempfile.TemporaryDirectory(prefix="river-raid-standalone-") as temporary:
        root = Path(temporary)
        source, build, runtime = root / "product", root / "build", root / "run"
        source.mkdir()
        runtime.mkdir()
        for original in product:
            target = source / original.relative_to(ROOT)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(original, target)
        run_native_step(
            "fresh CMake configure",
            ["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
             "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_TESTING=ON"],
            CONFIGURE_TIMEOUT_SECONDS,
        )
        run_native_step(
            "fresh product build",
            ["cmake", "--build", str(build), "--parallel"],
            BUILD_TIMEOUT_SECONDS,
        )
        run_native_step(
            "complete CTest suite",
            ["ctest", "--test-dir", str(build), "--output-on-failure"],
            TEST_TIMEOUT_SECONDS,
        )
        check_runtime(build / "river_raid", runtime)

    report = dict(scope="Fresh product-only build/tests and empty-directory startup, title/options, sprite/world, and flight runtime checks; original parity and full-gameplay acceptance are not established",
                  copied_files=[str(path.relative_to(ROOT)) for path in product],
                  reference_files_copied=False, build_passed=True, tests_passed=True,
                  empty_directory_runtime_passed=True)
    output = ROOT / ".build/standalone-check.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n")
    print("Standalone build, tests and listed runtime checks passed without original references; original parity and full-gameplay acceptance are not claimed.")


if __name__ == "__main__":
    main()
