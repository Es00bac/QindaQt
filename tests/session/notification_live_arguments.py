# SPDX-License-Identifier: GPL-3.0-or-later
"""Strict two-mode CLI contract for the notification live-session driver."""

from __future__ import annotations

import argparse
from pathlib import Path


def _add_artifact_options(parser: argparse.ArgumentParser, relative: bool) -> None:
    suffix = "-relative" if relative else ""
    for name in (
        "launcher",
        "session",
        "notification-host",
        "settings-service",
        "settings-app",
        "shell",
        "compositor-plugin",
    ):
        parser.add_argument(f"--{name}{suffix}", type=Path)


def parse_arguments(description: str) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("--inner", action="store_true")
    parser.add_argument("--runtime-root", type=Path)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--cmake")
    parser.add_argument("--build-directory", type=Path)
    parser.add_argument("--install-prefix", type=Path)
    parser.add_argument("--dbus-runner")
    parser.add_argument("--busctl")
    parser.add_argument("--configuration", default="")
    parser.add_argument("--repeat", type=int, default=1)
    _add_artifact_options(parser, relative=False)
    _add_artifact_options(parser, relative=True)
    arguments = parser.parse_args()
    artifact_names = (
        "launcher",
        "session",
        "notification_host",
        "settings_service",
        "settings_app",
        "shell",
        "compositor_plugin",
    )
    if arguments.inner:
        if arguments.runtime_root is None:
            parser.error("--inner requires --runtime-root")
        if not arguments.busctl:
            parser.error("--inner requires --busctl")
        arguments.artifacts = {
            name: getattr(arguments, name) for name in artifact_names
        }
        if any(path is None for path in arguments.artifacts.values()):
            parser.error("--inner requires every staged artifact path")
    else:
        required = {
            "cmake": arguments.cmake,
            "build-directory": arguments.build_directory,
            "install-prefix": arguments.install_prefix,
            "dbus-runner": arguments.dbus_runner,
            "busctl": arguments.busctl,
        }
        required.update(
            {
                f"{name.replace('_', '-')}-relative": getattr(
                    arguments, f"{name}_relative"
                )
                for name in artifact_names
            }
        )
        missing = [name for name, value in required.items() if value is None]
        if missing:
            parser.error("outer mode missing: " + ", ".join(missing))
    return arguments
