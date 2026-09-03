# SPDX-License-Identifier: GPL-3.0-or-later
"""Install DesktopVirtual and prove its loader and QML closure without loading it."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

from desktop_session_stage import StageContractError, install_stage, reset_stage_root
from desktop_session_stage_closure import StageClosureError, verify_stage_closure


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cmake", type=Path, required=True)
    parser.add_argument("--readelf", type=Path, required=True)
    parser.add_argument("--build-root", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--stage-root", type=Path, required=True)
    parser.add_argument("--bin-directory", required=True)
    parser.add_argument("--lib-directory", required=True)
    parser.add_argument("--qml-directory", required=True)
    parser.add_argument("--qml-source", type=Path, action="append", required=True)
    parser.add_argument("--system-library-directory", type=Path, action="append", required=True)
    parser.add_argument("--required-shell-library", action="append", required=True)
    parser.add_argument("--negative-library", required=True)
    parser.add_argument("--configuration", default="")
    return parser.parse_args()


def _verify(arguments: argparse.Namespace, stage: Path):
    return verify_stage_closure(
        stage,
        readelf=arguments.readelf,
        qml_directory=arguments.qml_directory,
        qml_sources=arguments.qml_source,
        system_library_directories=arguments.system_library_directory,
        forbidden_roots=(arguments.build_root, arguments.source_root),
        shell_relative=Path(arguments.bin_directory) / "qindaqt-shell",
        required_shell_libraries=arguments.required_shell_library,
    )


def _launch_shell(arguments: argparse.Namespace, stage: Path) -> None:
    shell = stage / arguments.bin_directory / "qindaqt-shell"
    environment = {
        "HOME": str(stage),
        "LC_ALL": "C.UTF-8",
        "PATH": os.defpath,
        "QML_IMPORT_PATH": str(stage / arguments.qml_directory),
        "QT_QPA_PLATFORM": "offscreen",
        "XDG_DATA_DIRS": str(stage / "share"),
    }
    completed = subprocess.run(
        [str(shell), "--help"],
        text=True,
        capture_output=True,
        check=False,
        timeout=20,
        env=environment,
    )
    output = completed.stdout + completed.stderr
    if completed.returncode != 0 or "Usage:" not in output:
        raise StageClosureError(
            "staged qindaqt-shell --help failed:\n" + output
        )


def _prove_missing_library_fails(arguments: argparse.Namespace, stage: Path) -> None:
    negative = arguments.stage_root.with_name(arguments.stage_root.name + "-missing-library")
    reset_stage_root(negative, arguments.build_root)
    shutil.copytree(stage, negative, dirs_exist_ok=True, symlinks=True)
    missing = negative / arguments.lib_directory / arguments.negative_library
    try:
        missing.unlink()
    except OSError as error:
        raise StageClosureError(f"negative-control library is unavailable: {missing}") from error
    try:
        _verify(arguments, negative)
    except StageClosureError as error:
        if arguments.negative_library not in str(error):
            raise StageClosureError(
                f"negative control failed for the wrong reason: {error}"
            ) from error
        return
    raise StageClosureError("missing-library negative control unexpectedly passed")


def main() -> int:
    arguments = parse_arguments()
    try:
        stage = install_stage(
            arguments.cmake,
            arguments.build_root,
            arguments.stage_root,
            configuration=arguments.configuration,
            component="DesktopVirtual",
        )
        report = _verify(arguments, stage)
        _launch_shell(arguments, stage)
        _prove_missing_library_fails(arguments, stage)
    except (OSError, subprocess.SubprocessError, StageClosureError, StageContractError) as error:
        print(f"DesktopVirtual stage closure failed: {error}", file=sys.stderr)
        return 1
    print(
        "DesktopVirtual stage closure passed: "
        f"{report.elf_files} ELF files, {report.needed_entries} DT_NEEDED entries, "
        f"{len(report.qml_modules)} QML modules, missing-library negative control"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
