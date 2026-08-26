# SPDX-License-Identifier: GPL-3.0-or-later
"""Prove a staged launcher discovers its compositor and decoration plugins."""

from __future__ import annotations

import argparse
import configparser
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


def installed_artifact_path(install_prefix: Path, relative: Path, label: str) -> Path:
    if relative.is_absolute():
        raise RuntimeError(f"{label} install path must be relative: {relative}")
    artifact = (install_prefix / relative).resolve()
    if install_prefix not in artifact.parents:
        raise RuntimeError(f"{label} install path escapes the staged prefix: {relative}")
    if not artifact.is_file():
        raise RuntimeError(f"staged install omitted {label}: {artifact}")
    return artifact


def stage_install(arguments: argparse.Namespace) -> tuple[Path, Path, Path]:
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

    launcher = installed_artifact_path(
        install_prefix, arguments.launcher_relative, "launcher"
    )
    compositor_plugin = installed_artifact_path(
        install_prefix, arguments.compositor_plugin_relative, "compositor plugin"
    )
    decoration = installed_artifact_path(
        install_prefix, arguments.decoration_relative, "decoration plugin"
    )
    return launcher, compositor_plugin, decoration


def decoration_library(config_home: Path) -> str:
    # AGENT-CONTRACT: SessionDefaults and KWin share this persisted section and
    # key. Reading the real file proves the installed launcher executed its
    # first-run policy before replacing itself with KWin.
    parser = configparser.ConfigParser(interpolation=None)
    kwin_config = config_home / "kwinrc"
    try:
        with kwin_config.open(encoding="utf-8") as stream:
            parser.read_file(stream)
    except (OSError, configparser.Error) as error:
        raise RuntimeError(
            f"could not read installed session defaults from {kwin_config}: {error}"
        )
    try:
        return parser.get("org.kde.kdecoration2", "library")
    except (configparser.Error, KeyError) as error:
        raise RuntimeError(
            f"installed session did not seed org.kde.kdecoration2/library in {kwin_config}"
        ) from error


def run_installed_session(arguments: argparse.Namespace, launcher: Path) -> dict[str, object]:
    spec = load_virtual_spec(arguments.scenario)
    with tempfile.TemporaryDirectory(prefix="qindaqt-installed-session-") as directory:
        environment = isolated_environment(Path(directory))
        config_home = Path(environment["XDG_CONFIG_HOME"])
        write_virtual_output_config(config_home, spec)
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
        selected_decoration = decoration_library(config_home)
        if selected_decoration != "org.qindaqt":
            raise RuntimeError(
                "fresh installed session selected an unexpected KDecoration3 module: "
                f"{selected_decoration!r}"
            )
        result = extract_probe_result(completed.stdout)
        validate_probe_result(
            result,
            spec,
            inspect_compositor_outputs=True,
            expect_workflow=True,
            expect_read_only=False,
        )
        result["firstRunDecorationLibrary"] = selected_decoration
    return result


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cmake")
    parser.add_argument("build_directory", type=Path)
    parser.add_argument("install_prefix", type=Path)
    parser.add_argument("launcher_relative", type=Path)
    parser.add_argument("compositor_plugin_relative", type=Path)
    parser.add_argument("decoration_relative", type=Path)
    parser.add_argument("probe", type=Path)
    parser.add_argument("dbus_runner")
    parser.add_argument("scenario", type=Path)
    parser.add_argument("--configuration", default="")
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        launcher, plugin, decoration = stage_install(arguments)
        result = run_installed_session(arguments, launcher)
    except (OSError, RuntimeError, ScenarioCoverageError, subprocess.TimeoutExpired) as error:
        print(f"installed plugin discovery failed: {error}", file=sys.stderr)
        return 1
    result["installedLauncher"] = str(launcher)
    result["installedPlugin"] = str(plugin)
    result["installedDecoration"] = str(decoration)
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
