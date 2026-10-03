"""Focused tests for the native environment and shipped-asset doctor."""

from __future__ import annotations

import builtins
import contextlib
import io
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import doctor


def _install_assets(root: Path, *, missing: tuple[str, ...] = ()) -> None:
    assets = root / "src/assets"
    assets.mkdir(parents=True)
    for name in doctor.SHIPPED_ASSETS:
        if name not in missing:
            (assets / name).write_text("// test asset\n", encoding="utf-8")


class DoctorTests(unittest.TestCase):
    def run_doctor(self, root: Path, cmake_result: SimpleNamespace):
        output = io.StringIO()

        def which(name: str) -> str:
            return f"/test-tools/{name}"

        with patch.object(doctor, "ROOT", root), \
             patch.object(doctor.shutil, "which", side_effect=which) as mocked_which, \
             patch.object(doctor.subprocess, "run", return_value=cmake_result) as mocked_run, \
             patch("sys.argv", ["doctor.py"]), \
             contextlib.redirect_stdout(output):
            status = doctor.main()
        return status, output.getvalue(), mocked_which, mocked_run

    def test_cmake_config_probe_replaces_pkg_config_dependency(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _install_assets(root)

            def configure(command, **kwargs):
                self.assertEqual(command[0], "/test-tools/cmake")
                self.assertEqual(command[1], "-S")
                probe = Path(command[2])
                cmake_file = (probe / "CMakeLists.txt").read_text(encoding="utf-8")
                self.assertIn("find_package(SDL3 CONFIG REQUIRED)", cmake_file)
                self.assertIn("TARGET SDL3::SDL3", cmake_file)
                self.assertNotIn(str(root), cmake_file)
                self.assertTrue(kwargs["capture_output"])
                self.assertTrue(kwargs["text"])
                self.assertEqual(kwargs["timeout"], 30)
                return SimpleNamespace(returncode=0, stdout="configured", stderr="")

            output = io.StringIO()
            which = lambda name: f"/test-tools/{name}"
            with patch.object(doctor, "ROOT", root), \
                 patch.object(doctor.shutil, "which", side_effect=which) as mocked_which, \
                 patch.object(doctor.subprocess, "run", side_effect=configure) as mocked_run, \
                 patch("sys.argv", ["doctor.py"]), \
                 contextlib.redirect_stdout(output):
                status = doctor.main()

            self.assertEqual(status, 0)
            self.assertIn("OK SDL3 CMake config", output.getvalue())
            self.assertNotIn("pkg-config", [call.args[0] for call in mocked_which.call_args_list])
            mocked_run.assert_called_once()

    def test_missing_cmake_package_is_reported_as_failure(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _install_assets(root)
            diagnostic = "Could not find a package configuration file provided by SDL3"
            status, output, _, _ = self.run_doctor(
                root, SimpleNamespace(returncode=1, stdout="", stderr=diagnostic)
            )

        self.assertEqual(status, 1)
        self.assertIn("MISSING SDL3 CMake config", output)
        self.assertIn(diagnostic, output)

    def test_missing_product_assets_are_reported(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _install_assets(root, missing=(
                "player_data.hpp", "player_detail.hpp", "shot_data.hpp", "regional_timing.hpp",
            ))
            status, output, _, _ = self.run_doctor(
                root, SimpleNamespace(returncode=0, stdout="", stderr="")
            )

        self.assertEqual(status, 1)
        self.assertIn("MISSING shipped asset: player_data.hpp", output)
        self.assertIn("MISSING shipped asset: shot_data.hpp", output)
        self.assertIn("MISSING shipped asset: player_detail.hpp", output)
        self.assertIn("MISSING shipped asset: regional_timing.hpp", output)

    def test_native_path_does_not_import_reference_checker(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            _install_assets(root)
            original_import = builtins.__import__

            def guarded_import(name, *args, **kwargs):
                if name == "prepare_reference":
                    raise AssertionError("native doctor imported reference tooling")
                return original_import(name, *args, **kwargs)

            output = io.StringIO()
            with patch.object(doctor, "ROOT", root), \
                 patch.object(
                     doctor.shutil, "which",
                     side_effect=lambda name: f"/test-tools/{name}",
                 ), \
                 patch.object(doctor.subprocess, "run", return_value=SimpleNamespace(
                     returncode=0, stdout="", stderr="")), \
                 patch("builtins.__import__", side_effect=guarded_import), \
                 patch("sys.argv", ["doctor.py"]), \
                 contextlib.redirect_stdout(output):
                status = doctor.main()

        self.assertEqual(status, 0)

    def test_analysis_option_is_rejected(self):
        with patch("sys.argv", ["doctor.py", "--analysis"]), \
             contextlib.redirect_stderr(io.StringIO()) as stderr:
            with self.assertRaises(SystemExit) as failure:
                doctor.main()

        self.assertEqual(failure.exception.code, 2)
        self.assertIn("unrecognized arguments: --analysis", stderr.getvalue())

    def test_cmake_probe_timeout_and_launch_errors_are_explicit_failures(self):
        failures = (
            subprocess.TimeoutExpired(["cmake", "-S"], timeout=30),
            FileNotFoundError("cmake executable is unavailable"),
        )
        for failure in failures:
            with self.subTest(failure=type(failure).__name__):
                output = io.StringIO()
                with patch.object(doctor.subprocess, "run", side_effect=failure), \
                     contextlib.redirect_stdout(output):
                    result = doctor.check_sdl3_config("cmake")

                self.assertFalse(result)
                self.assertIn("ERROR SDL3 CMake config probe failed", output.getvalue())


if __name__ == "__main__":
    unittest.main()
