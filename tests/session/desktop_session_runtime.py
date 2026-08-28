# SPDX-License-Identifier: GPL-3.0-or-later
"""Inner PID-namespace orchestration for the first complete desktop boot."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Mapping

from desktop_session_measure import aggregate_pss_kib, read_process_sample
from desktop_session_process import (
    ProcessContractError,
    ProcessIdentity,
    capture_process_identity,
    terminate_processes,
)
from desktop_session_sandbox import FORBIDDEN_ENVIRONMENT, SandboxContractError
from desktop_session_stage import ResolvedStage, resolve_stage
from desktop_session_topology import desktop_1080p_topology, validate_boot_evidence
from nested_session_scenario import VirtualOutputSpec, write_virtual_output_config


MARKER = "QINDAQT_DESKTOP_SESSION_PROBE="


@dataclass
class RuntimeState:
    processes: list[subprocess.Popen[str]] = field(default_factory=list)
    spawned: list[tuple[subprocess.Popen[str], list[Path]]] = field(default_factory=list)
    identities: list[ProcessIdentity] = field(default_factory=list)

    def track(self, process: subprocess.Popen[str], allowed: list[Path]) -> None:
        self.processes.append(process)
        self.spawned.append((process, allowed))


def _spawn(
    name: str, command: list[str], environment: Mapping[str, str]
) -> subprocess.Popen[str]:
    log = Path(f"/var/log/qindaqt-desktop/{name}.log").open("w", encoding="utf-8")
    process = subprocess.Popen(
        command,
        env=dict(environment),
        stdout=log,
        stderr=subprocess.STDOUT,
        text=True,
        start_new_session=True,
    )
    process._qindaqt_log = log  # type: ignore[attr-defined]
    return process


def _wait_for_path(path: Path, state: RuntimeState, seconds: float) -> None:
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if path.exists():
            return
        failed = [process.pid for process in state.processes if process.poll() is not None]
        if failed:
            raise RuntimeError(f"required process exited before {path}: {failed}")
        time.sleep(0.02)
    raise RuntimeError(f"timed out waiting for {path}")


def _parse_probe(line: str) -> dict[str, Any]:
    if not line.startswith(MARKER):
        raise RuntimeError("desktop probe marker was missing")
    document = json.loads(line.removeprefix(MARKER))
    if not isinstance(document, dict) or document.get("schemaVersion") != 1:
        raise RuntimeError("desktop probe returned an invalid document")
    return document


def _proc_record(pid: int) -> tuple[str, int]:
    process = Path("/proc") / str(pid)
    executable = (process / "exe").resolve(strict=True).name
    contents = (process / "stat").read_text(encoding="ascii")
    closing = contents.rfind(")")
    fields = contents[closing + 2 :].split()
    if closing < 2 or len(fields) < 2:
        raise RuntimeError(f"malformed process stat for PID {pid}")
    return executable, int(fields[1], 10)


def _process_evidence(probe: Mapping[str, Any]) -> tuple[dict[str, Any], dict[str, int]]:
    topology = desktop_1080p_topology()
    by_executable = {item.executable: item for item in topology.processes}
    observed: dict[str, tuple[int, int]] = {}
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            executable, parent = _proc_record(int(entry.name))
        except (OSError, RuntimeError, ValueError):
            continue
        expectation = by_executable.get(executable)
        if expectation is not None:
            if expectation.role in observed:
                raise RuntimeError(f"multiple processes claimed {expectation.role}")
            observed[expectation.role] = (int(entry.name), parent)
    observed["session-probe"] = (
        int(str(probe.get("selfPid", "0"))),
        int(str(probe.get("parentPid", "0"))),
    )
    if set(observed) != {item.role for item in topology.processes}:
        raise RuntimeError(f"process topology incomplete: {sorted(observed)}")
    roles_by_pid = {pid: role for role, (pid, _) in observed.items()}
    records: dict[str, Any] = {}
    pids: dict[str, int] = {}
    for expected in topology.processes:
        pid, parent_pid = observed[expected.role]
        records[expected.role] = {
            "pid": pid,
            "executable": expected.executable,
            "parentRole": roles_by_pid.get(parent_pid),
        }
        pids[expected.role] = pid
    return records, pids


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


def _applications(windows: list[Any]) -> list[dict[str, Any]]:
    result = []
    for expected in desktop_1080p_topology().applications:
        match = next(
            (item for item in windows if isinstance(item, dict)
             and expected.window_title_contains in str(item.get("title", ""))),
            None,
        )
        if match is None:
            raise RuntimeError(f"mapped test application was missing: {expected.app_id}")
        result.append(
            {"appId": expected.app_id, "windowTitle": match.get("title"), "mapped": True}
        )
    return result


def _build_evidence(probe: Mapping[str, Any]) -> tuple[dict[str, Any], dict[str, int]]:
    processes, pids = _process_evidence(probe)
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
            "generations": {
                "outputs": output.get("outputGeneration"),
                "shellVisibility": visibility.get("outputGeneration"),
            },
            "inputDevices": values["inputCapabilities"].get("devices", []),
            "dockSurfaces": values["developmentShellSurfaces"].get("surfaces", []),
            "applications": _applications(list(values["windows"].get("windows", []))),
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
) -> subprocess.Popen[str]:
    runtime = Path(environment["XDG_RUNTIME_DIR"])
    bus = _spawn(
        "dbus-daemon",
        [str(arguments.dbus_daemon), "--session", "--nofork", "--nopidfile",
         f"--address={environment['DBUS_SESSION_BUS_ADDRESS']}"],
        environment,
    )
    state.track(bus, [arguments.dbus_daemon])
    _wait_for_path(runtime / "bus", state, 5)
    for role in ("settings-service", "audio-service"):
        child = _spawn(role, [str(stage.executables[role])], environment)
        state.track(child, [stage.executables[role]])
    socket_name = _configure_private_session(environment)
    compositor = _spawn(
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
    _wait_for_path(runtime / socket_name, state, 15)
    app_environment = dict(environment)
    app_environment["WAYLAND_DISPLAY"] = socket_name
    for role, command in (
        ("settings-app", [str(stage.executables["settings-app"]), "--page", "notifications"]),
        ("editor-app", [str(stage.executables["editor-app"])])
    ):
        child = _spawn(role, command, app_environment)
        state.track(child, [stage.executables[role]])
    probe = subprocess.Popen(
        [str(arguments.probe)], env=app_environment, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, text=True, start_new_session=True
    )
    state.track(probe, [arguments.probe])
    return probe


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
        if role != "session-probe" or probe.poll() is None:
            state.identities.append(capture_process_identity(role, pid, allowed[role]))


def _cleanup(state: RuntimeState) -> None:
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
    try:
        if state.identities:
            terminate_processes(reversed(state.identities))
    except ProcessContractError as error:
        failures.append(str(error))
    finally:
        for process in state.processes:
            log = getattr(process, "_qindaqt_log", None)
            if log is not None:
                log.close()
    if failures:
        raise ProcessContractError("exact desktop cleanup failed: " + "; ".join(failures))


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
    try:
        probe = _start_desktop(arguments, stage, dict(os.environ), state)
        marker = probe.stdout.readline() if probe.stdout is not None else ""
        evidence, pids = _build_evidence(_parse_probe(marker))
        samples = [read_process_sample(pids[role]) for role in (
            "compositor", "session", "notification", "shell",
            "settings-service", "audio-service")]
        pss = aggregate_pss_kib(samples)
        evidence["measurements"] = {"residentPssKiB": pss, "ceilingKiB": 1024 * 1024}
        if pss > 1024 * 1024:
            raise RuntimeError(f"resident PSS exceeded 1024 MiB: {pss} KiB")
        _authenticate_processes(arguments, stage, state, pids, probe)
        probe.wait(timeout=2)
        if probe.returncode != 0:
            raise RuntimeError("desktop session probe failed")
    finally:
        _cleanup(state)
    if evidence is None:
        raise RuntimeError("desktop evidence was not constructed")
    evidence["cleanup"] = {"bounded": True, "survivorPids": []}
    validate_boot_evidence(evidence)
    artifact = Path("/var/lib/qindaqt-evidence/desktop-session-evidence.json")
    artifact.write_text(json.dumps(evidence, sort_keys=True, indent=2) + "\n")
    print("QINDAQT_DESKTOP_SESSION_EVIDENCE=" + json.dumps(evidence, sort_keys=True))
    return 0
