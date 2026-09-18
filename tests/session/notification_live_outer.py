# SPDX-License-Identifier: GPL-3.0-or-later
"""Outer private-runtime orchestration for notification live qualification."""

from __future__ import annotations

import json
import os
import sys
import tempfile
from pathlib import Path
from typing import Any

from nested_session_scenario import create_private_session_bus, isolated_environment
from notification_live_process import run_private_process_group
from notification_live_stage import stage_installed

REPETITION_TIMEOUT_SECONDS = 180


def _object(value: Any, location: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise RuntimeError(f"{location} must be a JSON object")
    return value


def _inner_arguments(
    arguments: Any, runtime_root: Path, driver_path: Path
) -> list[str]:
    result = [
        sys.executable,
        str(driver_path),
        "--inner",
        "--runtime-root",
        str(runtime_root),
        "--probe",
        str(arguments.probe),
        "--scenario",
        str(arguments.scenario),
        "--busctl",
        str(arguments.busctl),
    ]
    for name, path in arguments.artifacts.items():
        result.extend([f"--{name.replace('_', '-')}", str(path)])
    return result


def successful_inner_result(
    spec: Any,
    compositor_pid: int,
    host_pid: int,
    initial_shell_pid: int,
    replacement_shell_pid: int,
    outcomes: list[dict[str, Any]],
) -> dict[str, Any]:
    """Assemble the stable per-run evidence envelope consumed by outer mode."""
    return {
        "passed": True,
        "scenario": spec.scenario_id,
        "scale": spec.scale,
        "logicalSize": [spec.logical_width, spec.logical_height],
        "compositorPid": compositor_pid,
        "notificationHostPid": host_pid,
        "initialShellPid": initial_shell_pid,
        "replacementShellPid": replacement_shell_pid,
        "hostPidContinuous": True,
        "freshShellPid": replacement_shell_pid != initial_shell_pid,
        "phases": outcomes,
        "scenarioCoverage": spec.coverage,
        "lockerPolicy": "private-require-password-false",
    }


def _private_bus_with_staged_activation(arguments: Any, root: Path):
    """Give the repetition's bus the staged activation entries, minus Settings1.

    AGENT-GUARD: a stock ``dbus-run-session`` loads the host's service
    directories, so activation inside this "private" session started the
    installed package's Audio1/Power1/Network1/... binaries instead of the
    staged ones, and during the settings-outage phase any client's retry (the
    on-screen keyboard's, since it became the compositor's input method)
    activated the installed Settings1 and the outage assertion failed. The bus
    now sees exactly one service directory holding the stage's own entries.
    Settings1's entry is left out on purpose: this driver starts, stops and
    restarts that process itself, and an activation entry would race it.
    """
    bus = create_private_session_bus(root, Path(arguments.dbus_runner))
    staged = arguments.install_prefix.resolve() / "share" / "dbus-1" / "services"
    for entry in sorted(staged.glob("*.service")):
        if entry.name == "org.qindaqt.Settings1.service":
            continue
        (bus.service_directory / entry.name).write_text(
            entry.read_text(encoding="utf-8"), encoding="utf-8"
        )
    return bus


def run_outer(arguments: Any, driver_path: Path) -> dict[str, Any]:
    """Stage once, then run each repetition on a fresh bus and XDG tree."""
    arguments.artifacts = stage_installed(arguments)
    results: list[dict[str, Any]] = []
    for repetition in range(arguments.repeat):
        with tempfile.TemporaryDirectory(
            prefix="qindaqt-notification-live-"
        ) as directory:
            root = Path(directory)
            environment = isolated_environment(root)
            bus = _private_bus_with_staged_activation(arguments, root)
            environment["QINDAQT_NOTIFICATION_LIVE_PRIVATE_BUS"] = "1"
            environment["PATH"] = (
                str(
                    arguments.install_prefix.resolve()
                    / arguments.launcher_relative.parent
                )
                + os.pathsep
                + environment.get("PATH", "")
            )
            completed = run_private_process_group(
                [
                    arguments.dbus_runner,
                    f"--config-file={bus.configuration}",
                    "--",
                    *_inner_arguments(arguments, root, driver_path),
                ],
                environment,
                REPETITION_TIMEOUT_SECONDS,
            )
            if completed.returncode != 0:
                raise RuntimeError(
                    f"nested repetition {repetition + 1} failed:\n"
                    + completed.stdout
                    + completed.stderr
                )
            results.append(_object(json.loads(completed.stdout), "inner result"))
    return {
        "passed": True,
        "scenario": results[0]["scenario"],
        "scale": results[0]["scale"],
        "repetitions": len(results),
        "runs": results,
    }
