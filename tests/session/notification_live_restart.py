# SPDX-License-Identifier: GPL-3.0-or-later
"""Resident-owner and shell-restart orchestration for Notification Live."""

from __future__ import annotations

import json
import select
import subprocess
from collections.abc import Callable
from pathlib import Path
from typing import Any

from notification_live_process import (
    InnerProcesses,
    await_service_pid,
    terminate,
    terminate_authenticated_private_service,
)


HOST_SERVICE = "org.freedesktop.Notifications"
SHELL_EVIDENCE_SERVICE = "org.qindaqt.ShellDevelopment"
RESIDENT_OWNER_SERVICE = "org.qindaqt.NotificationLiveOwner"
RESIDENT_MARKER = "QINDAQT_NOTIFICATION_RESIDENT="


def _start_resident_owner(
    probe: Path, log_path: Path
) -> tuple[subprocess.Popen[str], int]:
    """Keep the notification's exact private D-Bus sender alive across restart."""
    with log_path.open("a", encoding="utf-8") as error_log:
        process = subprocess.Popen(
            [str(probe), "--resident-owner"],
            text=True,
            stdout=subprocess.PIPE,
            stderr=error_log,
        )
    if process.stdout is None:
        terminate(process)
        raise RuntimeError("resident owner stdout pipe is absent")
    readable, _, _ = select.select([process.stdout], [], [], 5.0)
    if not readable:
        terminate(process)
        raise RuntimeError("resident owner did not publish its notification id")
    line = process.stdout.readline().strip()
    if not line.startswith(RESIDENT_MARKER):
        terminate(process)
        raise RuntimeError(f"resident owner returned invalid marker: {line}")
    payload = json.loads(line.removeprefix(RESIDENT_MARKER))
    notification_id = payload.get("notificationId") if isinstance(payload, dict) else None
    if not isinstance(notification_id, str) or not notification_id.isdecimal():
        terminate(process)
        raise RuntimeError("resident owner returned invalid notification id")
    return process, int(notification_id)


def run_restart_lifecycle(
    arguments: Any,
    processes: InnerProcesses,
    outcomes: list[dict[str, Any]],
    compositor_pid: int,
    host_pid: int,
    settings_pid: int,
    shell_pid: int,
    spec: Any,
    log_path: Path,
    run_probe: Callable[..., dict[str, Any]],
) -> int:
    """Prove one owner-bound record survives one production shell restart."""
    processes.resident_owner, notification_id = _start_resident_owner(
        arguments.probe, log_path
    )
    owner_pid = await_service_pid(
        arguments.busctl,
        RESIDENT_OWNER_SERVICE,
        expected=processes.resident_owner.pid,
    )
    common = (
        arguments,
        compositor_pid,
        host_pid,
        settings_pid,
    )
    outcomes.append(
        run_probe(
            common[0], "settings-restart", *common[1:], shell_pid,
            spec.logical_width, spec.logical_height, spec.scale,
            notification_id, owner_pid,
        )
    )
    terminate_authenticated_private_service(
        arguments.busctl, SHELL_EVIDENCE_SERVICE, shell_pid
    )
    replacement_shell_pid = await_service_pid(
        arguments.busctl, SHELL_EVIDENCE_SERVICE, different_from=shell_pid
    )
    if await_service_pid(arguments.busctl, HOST_SERVICE, expected=host_pid) != host_pid:
        raise RuntimeError("resident notification host changed during shell restart")
    outcomes.append(
        run_probe(
            common[0], "shell-restart", *common[1:], replacement_shell_pid,
            spec.logical_width, spec.logical_height, spec.scale,
            notification_id, owner_pid,
        )
    )
    processes.resident_owner.wait(timeout=5)
    return replacement_shell_pid
