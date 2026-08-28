# SPDX-License-Identifier: GPL-3.0-or-later
"""Contain and verify the installed notification-live qualification tree."""

from __future__ import annotations

import shutil
import subprocess
from pathlib import Path
from typing import Any


def safe_artifact(prefix: Path, relative: Path, label: str) -> Path:
    """Resolve one required file without permitting escape from the stage."""
    if relative.is_absolute():
        raise RuntimeError(f"{label} install path must be relative")
    artifact = (prefix / relative).resolve()
    if prefix not in artifact.parents or not artifact.is_file():
        raise RuntimeError(f"staged install omitted {label}: {artifact}")
    return artifact


def stage_installed(arguments: Any) -> dict[str, Path]:
    """Install into a validated child of this build and return exact artifacts."""
    build = arguments.build_directory.resolve()
    prefix = arguments.install_prefix.resolve()
    # AGENT-GUARD: This is the only recursive removal in the driver. The
    # destination must be a named child of this exact build tree.
    if prefix == build or build not in prefix.parents:
        raise RuntimeError("notification-live stage must be inside its build tree")
    shutil.rmtree(prefix, ignore_errors=True)
    command = [arguments.cmake, "--install", str(build), "--prefix", str(prefix)]
    if arguments.configuration:
        command.extend(["--config", arguments.configuration])
    completed = subprocess.run(command, text=True, capture_output=True, check=False)
    if completed.returncode != 0:
        raise RuntimeError(
            "notification-live staged install failed:\n"
            + completed.stdout
            + completed.stderr
        )
    return {
        name: safe_artifact(prefix, getattr(arguments, f"{name}_relative"), name)
        for name in (
            "launcher",
            "session",
            "notification_host",
            "settings_service",
            "settings_app",
            "shell",
            "compositor_plugin",
        )
    }
