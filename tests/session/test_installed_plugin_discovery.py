# SPDX-License-Identifier: GPL-3.0-or-later
"""Stage an install and prove its launcher discovers its installed KWin plugin."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True

from test_nested_session import (
    ScenarioCoverageError,
    extract_probe_result,
    isolated_environment,
    load_virtual_spec,
    validate_probe_result,
    write_virtual_output_config,
)


def stage_install(arguments: argparse.Namespace) -> tuple[Path, Path]:
    build_directory = arguments.build_directory.resolve()
    install_prefix = arguments.install_prefix.resolve()
    # AGENT-GUARD: The cleanup below is destructive by design. Never accept a
    # prefix outside this test's build tree or the build root itself.
    if install_prefix == build_directory or build_directory not in install_prefix.parents:
        raise RuntimeError("install smoke prefix must be a child of its build directory")
    shutil.rmtree(install_prefix, ignore_errors=True)

    command = [
        arguments.cmake,
        "--install",
        str(build_directory),
        "--prefix",
        str(install_prefix),
    ]
    if arguments.configuration:
        command.extend(["--config", arguments.configuration])
    completed = subprocess.run(command, text=True, capture_output=True, check=False)
    if completed.returncode != 0:
        raise RuntimeError(
            "staged installation failed:\n" + completed.stdout + completed.stderr
        )

    launcher = install_prefix / arguments.launcher_relative
    plugin = install_prefix / arguments.plugin_relative
    if not launcher.is_file() or not plugin.is_file():
        raise RuntimeError(
            f"staged install omitted launcher or plugin: launcher={launcher}, plugin={plugin}"
        )
    return launcher, plugin


def run_installed_session(arguments: argparse.Namespace, launcher: Path) -> dict[str, object]:
    spec = load_virtual_spec(arguments.scenario)
    with tempfile.TemporaryDirectory(prefix="qindaqt-installed-session-") as directory:
        environment = isolated_environment(Path(directory))
        write_virtual_output_config(Path(environment["XDG_CONFIG_HOME"]), spec)
        environment["QINDAQT_EXPECT_COMPOSITOR_PLUGIN"] = "1"
        environment["QINDAQT_EXPECT_COMPOSITOR_OUTPUTS"] = "1"
        command = [
            arguments.dbus_runner,
            "--",
            str(launcher),
            # AGENT-GUARD: Do not add --plugin-root here. This test exists to
            # exercise the installed launcher's KDEInstallDirs-derived default.
            "--virtual",
            "--width",
            str(spec.logical_width),
            "--height",
            str(spec.logical_height),
            "--scale",
            f"{spec.scale:.12g}",
            "--output-count",
            str(spec.output_count),
            "--test-scenario",
            str(arguments.scenario),
            "--no-lockscreen",
            "--no-global-shortcuts",
            "--session",
            str(arguments.probe),
        ]
        completed = subprocess.run(
            command,
            env=environment,
            text=True,
            capture_output=True,
            timeout=20,
            check=False,
        )
    if completed.returncode != 0:
        raise RuntimeError(
            "installed session failed:\n" + completed.stdout + completed.stderr
        )
    result = extract_probe_result(completed.stdout)
    validate_probe_result(
        result,
        spec,
        inspect_compositor_outputs=True,
        expect_workflow=True,
        expect_read_only=False,
    )
    return result


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cmake")
    parser.add_argument("build_directory", type=Path)
    parser.add_argument("install_prefix", type=Path)
    parser.add_argument("launcher_relative", type=Path)
    parser.add_argument("plugin_relative", type=Path)
    parser.add_argument("probe", type=Path)
    parser.add_argument("dbus_runner")
    parser.add_argument("scenario", type=Path)
    parser.add_argument("--configuration", default="")
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        launcher, plugin = stage_install(arguments)
        result = run_installed_session(arguments, launcher)
    except (OSError, RuntimeError, ScenarioCoverageError, subprocess.TimeoutExpired) as error:
        print(f"installed plugin discovery failed: {error}", file=sys.stderr)
        return 1
    result["installedLauncher"] = str(launcher)
    result["installedPlugin"] = str(plugin)
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
