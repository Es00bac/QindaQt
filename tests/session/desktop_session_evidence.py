# SPDX-License-Identifier: GPL-3.0-or-later
"""Canonical evidence assembly and process authentication for desktop S3."""

from __future__ import annotations

import argparse
import os
import subprocess
from typing import Any, Mapping

from desktop_session_process import (
    RuntimeState,
    capture_process_identity,
    process_evidence,
)
from desktop_session_stage import ResolvedStage
from desktop_session_topology import DesktopTopology, observed_applications


# AGENT-CONTRACT: ADR-0049's product PSS number covers every long-lived QindaQt
# role present in the qualified desktop, including both visible applications.
PRODUCTION_PSS_ROLES = (
    "compositor", "session", "notification", "shell",
    "settings-service", "audio-service", "settings-app", "editor-app",
)


def _service_evidence(
    probe: Mapping[str, Any],
    pids: Mapping[str, int],
    topology: DesktopTopology,
) -> list[dict[str, Any]]:
    roles = {
        "org.qindaqt.Compositor": "compositor",
        "org.qindaqt.Settings1": "settings-service",
        "org.qindaqt.Audio1": "audio-service",
        "org.freedesktop.Notifications": "notification",
    }
    names = {item.role: item.executable for item in topology.processes}
    result: list[dict[str, Any]] = []
    for raw in probe.get("services", []):
        if not isinstance(raw, dict) or raw.get("status") != "owned":
            raise RuntimeError("a required D-Bus service was not owned")
        name = str(raw.get("name", ""))
        role = roles.get(name)
        if role is None or int(str(raw.get("pid", "0"))) != pids[role]:
            raise RuntimeError(f"D-Bus service {name!r} is not bound to its process")
        result.append({
            "name": name,
            "owner": raw.get("owner"),
            "pid": pids[role],
            "executable": names[role],
        })
    return result


def _build_evidence(
    probe: Mapping[str, Any],
    topology: DesktopTopology,
    role_process_ids: Mapping[str, int],
) -> tuple[dict[str, Any], dict[str, int]]:
    processes, pids = process_evidence(
        probe,
        topology,
        role_pid_hints=role_process_ids,
        direct_parent_pid=os.getpid(),
    )
    keys = (
        "outputs", "shellVisibility", "inputCapabilities",
        "developmentShellSurfaces", "windows",
    )
    values = {key: probe.get(key, {}) for key in keys}
    if not all(isinstance(value, dict) for value in values.values()):
        raise RuntimeError("probe method evidence was malformed")
    output = values["outputs"]
    visibility = values["shellVisibility"]
    return (
        {
            "schemaVersion": 1,
            "topology": topology.document(),
            "containment": {
                "mode": "bwrap-pid-network-ipc",
                "hostDisplayReachable": False,
                "hostSessionBusReachable": False,
                "hostInputReachable": False,
            },
            "processes": processes,
            "services": _service_evidence(probe, pids, topology),
            "outputs": output.get("outputs", []),
            "visibilityOutputs": visibility.get("outputs", []),
            "generations": {
                "outputs": output.get("outputGeneration"),
                "shellVisibility": visibility.get("outputGeneration"),
            },
            "inputDevices": values["inputCapabilities"].get("devices", []),
            "dockSurfaces": values["developmentShellSurfaces"].get("surfaces", []),
            "applications": observed_applications(
                list(values["windows"].get("windows", [])), topology
            ),
        },
        pids,
    )


def _authenticate_processes(
    arguments: argparse.Namespace,
    stage: ResolvedStage,
    state: RuntimeState,
    pids: Mapping[str, int],
    probe: subprocess.Popen[str],
    topology: DesktopTopology,
) -> None:
    allowed = {
        item.role: [stage.executables[item.role]]
        for item in topology.processes
        if item.role in stage.executables
    }
    allowed.update({
        "private-bus": [arguments.dbus_daemon],
        "compositor": [arguments.kwin_wayland],
        "session-probe": [arguments.probe],
    })
    if arguments.interactive:
        parent = next(
            item for item in topology.processes if item.role == "parent-compositor"
        )
        allowed["parent-compositor"] = [
            arguments.kwin_wayland
            if parent.executable == "kwin_wayland" else arguments.weston
        ]
        if any(item.role == "parent-private-bus" for item in topology.processes):
            allowed["parent-private-bus"] = [arguments.dbus_daemon]
    for role, pid in pids.items():
        state.identities.append(
            capture_process_identity(role, pid, allowed[role])
        )
