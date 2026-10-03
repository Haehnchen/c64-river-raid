#!/usr/bin/env python3
"""Create native release ZIPs from an already built River Raid executable."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import platform
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile


ROOT = Path(__file__).resolve().parents[1]
_ADDRESS = r"\(0x[0-9a-fA-F]+\)"
_LDD_DEPENDENCY = re.compile(
    rf"^(?P<name>[^\s/]+)\s+=>\s+(?P<path>/[^\s]+)\s+{_ADDRESS}$"
)
_LDD_ABSOLUTE = re.compile(rf"^(?P<path>/[^\s]+)\s+{_ADDRESS}$")
# Graphics/audio drivers need the host's matching glibc and dynamic loader.
_HOST_GLIBC = re.compile(
    r"^(?:ld-linux[^/]*|ld64\.so\.\d+|"
    r"lib(?:c|m|mvec|pthread|dl|rt|resolv|util|anl|BrokenLocale|"
    r"thread_db|nss_[^/]+)\.so(?:\.\d+)*)$"
)
_WINDOWS_SYSTEM_DLLS = set(
    "advapi32 avrt bcrypt cfgmgr32 comctl32 combase comdlg32 crypt32 d3d9 "
    "d3d11 d3d12 d3dcompiler_47 dbghelp dinput8 dnsapi dwmapi dxgi gdi32 hid "
    "imm32 iphlpapi kernel32 ksuser mf mfplat mfreadwrite mfuuid mmdevapi "
    "msacm32 msimg32 msvcrt netapi32 normaliz ntdll ole32 oleacc oleaut32 "
    "powrprof propsys psapi rpcrt4 secur32 setupapi shell32 shlwapi ucrtbase "
    "urlmon user32 userenv usp10 version winhttp wininet winmm winnsi winscard "
    "winspool wlanapi ws2_32 wtsapi32 xinput1_4 xinput9_1_0".split()
)


class PackagingError(RuntimeError):
    """An input could not be turned into a runnable archive."""


def _run(command: list[str]) -> str:
    try:
        result = subprocess.run(
            command, check=False, capture_output=True, text=True,
            env={**os.environ, "LC_ALL": "C"},
        )
    except OSError as error:
        raise PackagingError(f"could not run {command[0]!r}: {error}") from error
    if result.returncode:
        detail = result.stderr.strip() or result.stdout.strip()
        raise PackagingError(
            f"{command[0]} failed with exit status {result.returncode}"
            + (f": {detail}" if detail else "")
        )
    return result.stdout


def _file(path: Path, description: str) -> Path:
    try:
        resolved = path.resolve(strict=True)
    except (OSError, RuntimeError) as error:
        raise PackagingError(f"missing {description}: {path}") from error
    if not resolved.is_file():
        raise PackagingError(f"{description} is not a regular file: {path}")
    return resolved


def _parse_ldd(output: str) -> dict[str, Path]:
    dependencies: dict[str, Path] = {}
    for raw in output.splitlines():
        line = raw.strip()
        if not line or line in {"statically linked", "not a dynamic executable"}:
            continue
        if re.fullmatch(rf"(?:linux-vdso|linux-gate)[^\s/]*\s+{_ADDRESS}", line):
            continue
        match = _LDD_DEPENDENCY.fullmatch(line)
        absolute = _LDD_ABSOLUTE.fullmatch(line)
        if match:
            name, path = match.group("name"), Path(match.group("path"))
        elif absolute:
            path = Path(absolute.group("path"))
            name = path.name
        elif re.fullmatch(r"[^\s/]+\s+=>\s+not\s+found", line):
            raise PackagingError(f"missing library reported by ldd: {line}")
        else:
            raise PackagingError(f"unrecognized ldd output: {raw!r}")
        if name in dependencies and dependencies[name] != path:
            raise PackagingError(f"ldd reports conflicting paths for {name!r}")
        dependencies[name] = path
    return dependencies


def _linux(executable: Path, stage: Path, *, bundled: bool) -> None:
    if not bundled:
        shutil.copy2(executable, stage / "river_raid")
        return
    # ldd reports the complete transitive dependency set, including SDL when shared.
    dependencies = _parse_ldd(_run(["ldd", str(executable)]))
    (stage / "bin").mkdir()
    (stage / "lib").mkdir()
    shutil.copy2(executable, stage / "bin" / "river_raid")
    for name, source in sorted(dependencies.items()):
        if not _HOST_GLIBC.fullmatch(name):
            shutil.copy2(_file(source, f"library {name!r}"), stage / "lib" / name)
    launcher = stage / "river_raid"
    launcher.write_text(
        "#!/bin/sh\nset -eu\n"
        'root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
        'export LD_LIBRARY_PATH="$root/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\n'
        'exec "$root/bin/river_raid" "$@"\n',
        encoding="utf-8",
    )
    launcher.chmod(0o755)


def _parse_imports(output: str) -> list[str]:
    names = re.findall(r"^\s*DLL Name:\s*(.*?)\s*$", output, re.MULTILINE)
    if any(not name or "/" in name or "\\" in name or not name.lower().endswith(".dll")
           for name in names):
        raise PackagingError("objdump reported an invalid DLL import name")
    return names


def _find_dll(name: str, directories: list[Path]) -> Path | None:
    for directory in directories:
        if directory.is_dir():
            for candidate in directory.iterdir():
                if candidate.name.lower() == name.lower() and candidate.is_file():
                    return candidate
    return None


def _windows_system_dll(name: str) -> bool:
    lower = name.lower()
    if lower.startswith(("api-ms-win-", "ext-ms-win-")):
        return True
    if lower.removesuffix(".dll") in _WINDOWS_SYSTEM_DLLS:
        return True
    windows = os.environ.get("SystemRoot") or os.environ.get("WINDIR")
    return bool(windows and _find_dll(name, [Path(windows) / "System32"]))


def _windows(executable: Path, stage: Path) -> None:
    shutil.copy2(executable, stage / "river_raid.exe")
    search = [executable.parent] + [
        Path(entry) for entry in os.environ.get("PATH", "").split(os.pathsep) if entry
    ]
    pending = [executable]
    libraries: dict[str, Path] = {}
    while pending:
        source = pending.pop()
        for name in _parse_imports(_run(["objdump", "-p", str(source)])):
            if _windows_system_dll(name):
                continue
            dependency = _find_dll(name, [source.parent, *search])
            if dependency is None:
                raise PackagingError(f"missing DLL {name!r} imported by {source}")
            dependency = _file(dependency, f"DLL {name!r}")
            key = name.lower()
            if key in libraries:
                if libraries[key] != dependency:
                    raise PackagingError(f"conflicting paths for DLL {name!r}")
                continue
            libraries[key] = dependency
            shutil.copy2(dependency, stage / name)
            pending.append(dependency)


def _parse_otool(output: str) -> list[str]:
    names: list[str] = []
    for raw in output.splitlines()[1:]:
        if not raw.strip():
            continue
        match = re.fullmatch(r"\s+(.+?)\s+\(compatibility version .+\)", raw)
        if not match:
            raise PackagingError(f"unrecognized otool dependency: {raw!r}")
        names.append(match.group(1))
    return names


def _parse_rpaths(output: str) -> list[str]:
    return re.findall(
        r"\bcmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset \d+\)", output
    )


def _apple_system_path(name: str) -> bool:
    return name.startswith(("/usr/lib/", "/System/Library/"))


def _expand_macho_path(name: str, source: Path, executable: Path) -> Path | None:
    for token, directory in (("@loader_path", source.parent),
                             ("@executable_path", executable.parent)):
        if name == token:
            return directory
        if name.startswith(token + "/"):
            return directory / name[len(token) + 1:]
    return Path(name) if name.startswith("/") else None


def _macho_dependency(name: str, source: Path, executable: Path,
                      rpaths: list[Path]) -> Path:
    if name.startswith("@rpath/"):
        candidates = [path / name[len("@rpath/"):] for path in rpaths]
    else:
        expanded = _expand_macho_path(name, source, executable)
        candidates = [expanded] if expanded is not None else []
    for candidate in candidates:
        if candidate.is_file():
            return _file(candidate, f"library {name!r}")
    raise PackagingError(f"cannot resolve macOS library {name!r} imported by {source}")


def _macos(executable: Path, stage: Path) -> None:
    app = stage / "River Raid.app"
    contents = app / "Contents"
    frameworks = contents / "Frameworks"
    frameworks.mkdir(parents=True)
    (contents / "MacOS").mkdir()
    game = contents / "MacOS" / "River Raid"
    shutil.copy2(executable, game)
    game.chmod(0o755)
    with (contents / "Info.plist").open("wb") as info:
        plistlib.dump({
            "CFBundleExecutable": "River Raid",
            "CFBundleIdentifier": "org.river-raid.desktop",
            "CFBundleName": "River Raid",
            "CFBundlePackageType": "APPL",
            "CFBundleShortVersionString": "1.0",
            "CFBundleVersion": "1",
            "NSHighResolutionCapable": True,
        }, info)

    libraries: dict[str, Path] = {}
    pending = [(executable, game, [])]
    while pending:
        source, destination, inherited_rpaths = pending.pop()
        rpaths = [
            expanded for name in _parse_rpaths(_run(["otool", "-l", str(source)]))
            if (expanded := _expand_macho_path(name, source, executable)) is not None
        ] + inherited_rpaths
        dependencies = _parse_otool(_run(["otool", "-L", str(source)]))
        own_names = set()
        if destination != game:
            own_names = set(_run(["otool", "-D", str(source)]).splitlines()[1:])
        for name in dependencies:
            if name in own_names or _apple_system_path(name):
                continue
            dependency = _macho_dependency(name, source, executable, rpaths)
            if _apple_system_path(str(dependency)):
                continue
            basename = dependency.name
            target = frameworks / basename
            if basename in libraries and libraries[basename] != dependency:
                raise PackagingError(f"conflicting macOS library name {basename!r}")
            if basename not in libraries:
                libraries[basename] = dependency
                shutil.copy2(dependency, target)
                _run(["install_name_tool", "-id", f"@rpath/{basename}", str(target)])
                pending.append((dependency, target, rpaths))
            prefix = "@executable_path/../Frameworks" if destination == game else "@loader_path"
            _run(["install_name_tool", "-change", name,
                  f"{prefix}/{basename}", str(destination)])

    # Fix every load command before signing nested code, then the enclosing app.
    for basename in sorted(libraries):
        _run(["codesign", "--force", "--sign", "-", "--timestamp=none",
              str(frameworks / basename)])
    _run(["codesign", "--force", "--sign", "-", "--timestamp=none", str(app)])
    _run(["codesign", "--verify", "--deep", "--strict", str(app)])


def _zip_tree(stage: Path, output: Path) -> None:
    temporary = output.with_suffix(".zip.tmp")
    try:
        with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for path in sorted(stage.rglob("*")):
                if path.is_file():
                    archive.write(path, path.relative_to(stage))
        temporary.replace(output)
    finally:
        temporary.unlink(missing_ok=True)


def _architecture(value: str) -> str:
    normalized = {"amd64": "x86_64", "aarch64": "arm64"}.get(value.lower(), value.lower())
    if normalized not in {"x86_64", "arm64"}:
        raise PackagingError(f"unsupported release architecture: {value!r}")
    return normalized


def package(build_dir: Path, output_dir: Path | None = None,
            arch: str | None = None) -> list[Path]:
    system = platform.system()
    if system not in {"Linux", "Windows", "Darwin"}:
        raise PackagingError(f"unsupported release platform: {system!r}")
    architecture = _architecture(arch or platform.machine())
    executable = _file(build_dir / ("river_raid.exe" if system == "Windows" else "river_raid"),
                       "built executable")
    readme = _file(ROOT / "README.md", "README")
    notice_path = os.environ.get("RIVER_RAID_SDL_NOTICE")
    if not notice_path:
        raise PackagingError("set RIVER_RAID_SDL_NOTICE to the linked SDL3 source's LICENSE.txt")
    notice = _file(Path(notice_path), "SDL3 license notice")
    if not notice.read_text(encoding="utf-8").strip():
        raise PackagingError("SDL3 license notice is empty")
    output_dir = (output_dir or build_dir / "release").resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    label = "macOS" if system == "Darwin" else system
    archives = []
    for bundled in ([False, True] if system == "Linux" else [True]):
        output = output_dir / f"river-raid-{label}-{architecture}{'-bundled' if bundled else ''}.zip"
        with tempfile.TemporaryDirectory(prefix=".package-", dir=output_dir) as temporary:
            stage = Path(temporary)
            shutil.copy2(readme, stage / "README.md")
            shutil.copy2(notice, stage / "SDL-LICENSE.txt")
            if system == "Linux":
                _linux(executable, stage, bundled=bundled)
            elif system == "Windows":
                _windows(executable, stage)
            else:
                _macos(executable, stage)
            _zip_tree(stage, output)
        archives.append(output)
    return archives


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path(".build"))
    parser.add_argument("--output-dir", type=Path, help="defaults to BUILD_DIR/release")
    parser.add_argument("--arch", help="archive architecture (defaults to the host architecture)")
    args = parser.parse_args()
    try:
        for archive in package(args.build_dir, args.output_dir, args.arch):
            print(archive)
    except (PackagingError, OSError, UnicodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
