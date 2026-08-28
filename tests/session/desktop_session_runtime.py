# SPDX-License-Identifier: GPL-3.0-or-later
"""Inner PID-namespace orchestration for the first complete desktop boot."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import time
from pathlib import Path
from typing import Any, Mapping

from desktop_session_measure import aggregate_pss_kib, read_process_sample
from desktop_session_process import (
    CleanupRecord,
    ProcessContractError,
    RuntimeState,
    capture_process_identity,
    process_evidence,
    spawn_logged_process,
    terminate_processes,
    wait_for_path,
)
from desktop_session_readiness import (
    ReadinessDeadlineExpired,
    await_complete_snapshot,
    read_probe_document,
    remaining_probe_lifetime,
    require_probe_lifetime,
)
from desktop_session_sandbox import FORBIDDEN_ENVIRONMENT, SandboxContractError
from desktop_session_stage import ResolvedStage, resolve_stage
from desktop_session_topology import (
    desktop_1080p_topology,
    observed_applications,
    validate_boot_evidence,
)
from nested_session_scenario import VirtualOutputSpec, write_virtual_output_config


def _service_evidence(probe: Mapping[str, Any], pids: Mapping[str, int]) -> list[dict[str, Any]]:
    roles = {
        "org.qindaqt.Compositor": "compositor",
        "org.qindaqt.Settings1": "settings-service",
        "org.qindaqt.Audio1": "audio-service",
        "org.freedesktop.Notifications": "notification",
    }
    names = {item.role: item.executable for item in desktop_1080p_topology().processes}
    result: list[dict[str, Any]] = []
    for raw in probe.get("services", []):
        if not isinstance(raw, dict) or raw.get("status") != "owned":
            raise RuntimeError("a required D-Bus service was not owned")
        name = str(raw.get("name", ""))
        role = roles.get(name)
        if role is None or int(str(raw.get("pid", "0"))) != pids[role]:
            raise RuntimeError(f"D-Bus service {name!r} is not bound to its process")
        result.append(
            {"name": name, "owner": raw.get("owner"), "pid": pids[role],
             "executable": names[role]}
        )
    return result


def _build_evidence(probe: Mapping[str, Any]) -> tuple[dict[str, Any], dict[str, int]]:
    processes, pids = process_evidence(probe)
    keys = ("outputs", "shellVisibility", "inputCapabilities",
            "developmentShellSurfaces", "windows")
    values = {key: probe.get(key, {}) for key in keys}
    if not all(isinstance(value, dict) for value in values.values()):
        raise RuntimeError("probe method evidence was malformed")
    output = values["outputs"]
    visibility = values["shellVisibility"]
    return (
        {
            "schemaVersion": 1,
            "topology": desktop_1080p_topology().document(),
            "containment": {
                "mode": "bwrap-pid-network-ipc",
                "hostDisplayReachable": False,
                "hostSessionBusReachable": False,
                "hostInputReachable": False,
            },
            "processes": processes,
            "services": _service_evidence(probe, pids),
            "outputs": output.get("outputs", []),
            "visibilityOutputs": visibility.get("outputs", []),
            "generations": {
                "outputs": output.get("outputGeneration"),
                "shellVisibility": visibility.get("outputGeneration"),
            },
            "inputDevices": values["inputCapabilities"].get("devices", []),
            "dockSurfaces": values["developmentShellSurfaces"].get("surfaces", []),
            "applications": observed_applications(
                list(values["windows"].get("windows", []))
            ),
        },
        pids,
    )


def _configure_private_session(environment: Mapping[str, str]) -> str:
    config = Path(environment["XDG_CONFIG_HOME"])
    config.mkdir(parents=True, exist_ok=True)
    write_virtual_output_config(
        config, VirtualOutputSpec("single-1080p", 1, 1920, 1080, 1920, 1080, 1.0)
    )
    (config / "kscreenlockerrc").write_text(
        "[Daemon]\nAutolock=false\nLockOnResume=false\n", encoding="utf-8"
    )
    return f"qindaqt-{environment['QINDAQT_SESSION_RUN_ID'][:12]}"


def _start_desktop(
    arguments: argparse.Namespace,
    stage: ResolvedStage,
    environment: dict[str, str],
    state: RuntimeState,
) -> dict[str, str]:
    runtime = Path(environment["XDG_RUNTIME_DIR"])
    bus = spawn_logged_process(
        "dbus-daemon",
        [str(arguments.dbus_daemon), "--session", "--nofork", "--nopidfile",
         f"--address={environment['DBUS_SESSION_BUS_ADDRESS']}"],
        environment,
    )
    state.track(bus, [arguments.dbus_daemon])
    wait_for_path(runtime / "bus", state, 5)
    for role in ("settings-service", "audio-service"):
        child = spawn_logged_process(role, [str(stage.executables[role])], environment)
        state.track(child, [stage.executables[role]])
    socket_name = _configure_private_session(environment)
    compositor = spawn_logged_process(
        "compositor",
        [str(stage.executables["launcher"]), "--plugin-root",
         str(stage.compositor_plugin.parents[2]), "--virtual", "--width", "1920",
         "--height", "1080", "--scale", "1", "--output-count", "1", "--socket",
         socket_name, "--test-scenario",
         "/opt/qindaqt-source/tests/scenarios/single-1080p.json", "--session",
         str(stage.executables["session"])],
        environment,
    )
    state.track(compositor, [stage.executables["launcher"], arguments.kwin_wayland])
    wait_for_path(runtime / socket_name, state, 15)
    app_environment = dict(environment)
    app_environment["WAYLAND_DISPLAY"] = socket_name
    for role, command in (
        ("settings-app", [str(stage.executables["settings-app"]), "--page", "notifications"]),
        ("editor-app", [str(stage.executables["editor-app"])])
    ):
        child = spawn_logged_process(role, command, app_environment)
        state.track(child, [stage.executables[role]])
    return app_environment


def _spawn_probe(
    arguments: argparse.Namespace,
    environment: Mapping[str, str],
    state: RuntimeState,
    attempt: int,
) -> subprocess.Popen[str]:
    log = Path(f"/var/log/qindaqt-desktop/session-probe-{attempt:03d}.log").open(
        "w", encoding="utf-8"
    )
    try:
        probe = subprocess.Popen(
            [str(arguments.probe)], env=dict(environment), stdout=subprocess.PIPE,
            stderr=log, text=True, start_new_session=True
        )
    except BaseException:
        log.close()
        raise
    probe._qindaqt_log = log  # type: ignore[attr-defined]
    state.track(probe, [arguments.probe])
    return probe


def _await_runtime_snapshot(
    arguments: argparse.Namespace,
    environment: Mapping[str, str],
    state: RuntimeState,
) -> tuple[Mapping[str, Any], subprocess.Popen[str]]:
    current: subprocess.Popen[str] | None = None
    current_deadline = 0.0
    attempt = 0

    def sample(remaining: float) -> Mapping[str, Any]:
        nonlocal current, current_deadline, attempt
        if current is not None:
            wait_started = time.monotonic()
            try:
                current.wait(timeout=remaining_probe_lifetime(current_deadline))
            except subprocess.TimeoutExpired as error:
                raise ReadinessDeadlineExpired(
                    "probe did not exit within its fixed lifetime"
                ) from error
            remaining -= time.monotonic() - wait_started
            if current.returncode != 0: raise RuntimeError("desktop readiness probe failed")
        lifetime = require_probe_lifetime(remaining)
        attempt += 1
        current = _spawn_probe(arguments, environment, state, attempt)
        current_deadline = time.monotonic() + lifetime
        return read_probe_document(current, current_deadline)

    document = await_complete_snapshot(sample)
    if current is None:
        raise RuntimeError("desktop readiness completed without a probe")
    return document, current


def _authenticate_processes(
    arguments: argparse.Namespace, stage: ResolvedStage,
    state: RuntimeState, pids: Mapping[str, int], probe: subprocess.Popen[str]
) -> None:
    allowed = {
        item.role: [stage.executables[item.role]]
        for item in desktop_1080p_topology().processes
        if item.role in stage.executables
    }
    allowed.update({"private-bus": [arguments.dbus_daemon],
                    "compositor": [arguments.kwin_wayland],
                    "session-probe": [arguments.probe]})
    for role, pid in pids.items():
        state.identities.append(capture_process_identity(role, pid, allowed[role]))


def _cleanup(state: RuntimeState) -> list[CleanupRecord]:
    tracked = {identity.pid for identity in state.identities}
    failures: list[str] = []
    for process, allowed in state.spawned:
        if process.pid in tracked or process.poll() is not None:
            continue
        try:
            identity = capture_process_identity(f"direct-{process.pid}", process.pid, allowed)
        except ProcessContractError as error:
            # AGENT-GUARD: The PID namespace remains the final containment
            # boundary, but a live child that cannot be authenticated is not
            # evidence of exact teardown. Reap every authenticated group, then
            # fail the row instead of silently upgrading namespace cleanup.
            failures.append(f"PID {process.pid}: {error}")
            continue
        state.identities.append(identity)
        tracked.add(identity.pid)
    ledger: list[CleanupRecord] = []
    try:
        if state.identities:
            ledger = terminate_processes(reversed(state.identities))
    except ProcessContractError as error:
        failures.append(str(error))
    finally:
        for process in state.processes:
            log = getattr(process, "_qindaqt_log", None)
            if log is not None:
                log.close()
    if failures:
        raise ProcessContractError("exact desktop cleanup failed: " + "; ".join(failures))
    return ledger


def run_inner(arguments: argparse.Namespace) -> int:
    if FORBIDDEN_ENVIRONMENT.intersection(os.environ):
        raise SandboxContractError("inner process inherited a forbidden host endpoint")
    stage = resolve_stage(
        arguments.stage_root,
        bin_directory=arguments.bin_directory,
        plugin_relative=arguments.plugin_relative,
        decoration_relative=arguments.decoration_relative,
        settings_service_directory=arguments.settings_service_directory,
        audio_service_directory=arguments.audio_service_directory,
    )
    state = RuntimeState()
    evidence: dict[str, Any] | None = None
    cleanup_records: list[CleanupRecord] = []
    try:
        app_environment = _start_desktop(arguments, stage, dict(os.environ), state)
        snapshot, probe = _await_runtime_snapshot(arguments, app_environment, state)
        evidence, pids = _build_evidence(snapshot)
        _authenticate_processes(arguments, stage, state, pids, probe)
        samples = [read_process_sample(pids[role]) for role in (
            "compositor", "session", "notification", "shell",
            "settings-service", "audio-service")]
        pss = aggregate_pss_kib(samples)
        evidence["measurements"] = {"residentPssKiB": pss, "ceilingKiB": 1024 * 1024}
        if pss > 1024 * 1024:
            raise RuntimeError(f"resident PSS exceeded 1024 MiB: {pss} KiB")
        probe.wait(timeout=2)
        if probe.returncode != 0:
            raise RuntimeError("desktop session probe failed")
    finally:
        cleanup_records = _cleanup(state)
    if evidence is None:
        raise RuntimeError("desktop evidence was not constructed")
    evidence["cleanup"] = {
        "bounded": True,
        "survivorPids": [],
        "terminalPhases": [record.document() for record in cleanup_records],
    }
    validate_boot_evidence(evidence)
    artifact = Path("/var/lib/qindaqt-evidence/desktop-session-evidence.json")
    artifact.write_text(json.dumps(evidence, sort_keys=True, indent=2) + "\n")
    print("QINDAQT_DESKTOP_SESSION_EVIDENCE=" + json.dumps(evidence, sort_keys=True))
    return 0
