"""Focused tests for bounded standalone build-tool failures."""

from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import check_standalone


class StandaloneStepTests(unittest.TestCase):
    def test_product_copy_scope_is_cmake_src_and_tests_only(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for relative in (
                "CMakeLists.txt",
                "src/main.cpp",
                "src/assets/game_data.hpp",
                "tests/CMakeLists.txt",
                "tests/game/life_cycle_test.cpp",
                "tests/support/session_steps.hpp",
                "assets/generated/analysis_only.hpp",
            ):
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("// fixture\n", encoding="utf-8")
            (root / "src/assets/river_raid.ico").write_bytes(b"\x00\x00\x01\x00\xff")
            for preview in ("icon.png", "icon-32.png", "icon-64.png"):
                (root / preview).write_bytes(b"\x89PNG\r\n\x1a\n")

            product = check_standalone.standalone_product_files(root)
            copied = {path.relative_to(root).as_posix() for path in product}

        self.assertEqual(copied, {
            "CMakeLists.txt",
            "src/main.cpp",
            "src/assets/game_data.hpp",
            "src/assets/river_raid.ico",
            "tests/CMakeLists.txt",
            "tests/game/life_cycle_test.cpp",
            "tests/support/session_steps.hpp",
        })

    def test_native_step_passes_timeout_and_checks_exit_status(self):
        command = ["ctest", "--output-on-failure"]
        with patch.object(check_standalone.subprocess, "run") as run:
            check_standalone.run_native_step("complete CTest suite", command, 600)

        run.assert_called_once_with(command, check=True, timeout=600, cwd=None, env=None)

    def test_native_step_reports_nonzero_exit_as_failure(self):
        command = ["cmake", "--build", "build"]
        failure = subprocess.CalledProcessError(7, command)
        with patch.object(check_standalone.subprocess, "run", side_effect=failure):
            with self.assertRaisesRegex(
                SystemExit,
                r"Standalone check failed during fresh product build \(exit 7\):",
            ):
                check_standalone.run_native_step("fresh product build", command, 600)

    def test_native_step_reports_timeout_and_stage_limit(self):
        command = ["ctest", "--output-on-failure"]
        failure = subprocess.TimeoutExpired(command, timeout=180)
        with patch.object(check_standalone.subprocess, "run", side_effect=failure) as run:
            with self.assertRaisesRegex(
                SystemExit,
                r"Standalone check timed out during complete CTest suite after 180s:",
            ):
                check_standalone.run_native_step("complete CTest suite", command, 180)

        run.assert_called_once_with(command, check=True, timeout=180, cwd=None, env=None)

    def test_native_step_reports_missing_tool(self):
        command = ["cmake", "--build", "build"]
        with patch.object(
            check_standalone.subprocess,
            "run",
            side_effect=FileNotFoundError("cmake not found"),
        ):
            with self.assertRaisesRegex(
                SystemExit,
                r"Could not start standalone fresh CMake configure: cmake not found",
            ):
                check_standalone.run_native_step("fresh CMake configure", command, 180)


if __name__ == "__main__":
    unittest.main()
