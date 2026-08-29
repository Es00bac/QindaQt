# SPDX-License-Identifier: GPL-3.0-or-later
"""Inner PID-namespace orchestration for the first complete desktop boot."""

from __future__ import annotations

import argparse
import json
import os
import shlex
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Mapping

from desktop_session_measure import aggregate_pss_kib, read_process_sample
from desktop_session_capture import (
    capture_parent_frame,
    capture_parent_matrix,
    capture_private_kwin_matrix,
)
from desktop_session_interactive import validate_interactive_evidence
from desktop_session_matrix import (
    DesktopMatrixScenario,
    load_matrix_scenario,
    physical_content_region,
)
from desktop_session_process import (
    CleanupRecord,
    ProcessContractError,
    RuntimeState,
    capture_process_identity,
    identity_is_live,
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
    BootTopology,
    DesktopTopology,
    MatrixBootTopology,
    desktop_1080p_topology,
    interactive_1080p_topology,
    interactive_matrix_topology,
    observed_applications,
    validate_boot_evidence,
)
from nested_session_scenario import VirtualOutputSpec, write_virtual_output_config


@dataclass(frozen=True)
class DesktopLaunch:
    app_environment: dict[str, str]
    parent_environment: dict[str, str] | None
    child_socket: str
    private_bus_process_id: int
    parent_bus_process_id: int | None
    parent_process_id: int | None
    compositor_process_id: int


# AGENT-CONTRACT: ADR-0049's product PSS number covers every long-lived QindaQt
# role present in the qualified desktop, including both visible applications.
# The private Weston parent and short-lived probes are test infrastructure.
PRODUCTION_PSS_ROLES = (
    "compositor", "session", "notification", "shell",
    "settings-service", "audio-service", "settings-app", "editor-app",
)


def _service_evidence(
    probe: Mapping[str, Any], pids: Mapping[str, int], topology: DesktopTopology,
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
        result.append(
            {"name": name, "owner": raw.get("owner"), "pid": pids[role],
             "executable": names[role]}
        )
    return result


def _build_evidence(
    probe: Mapping[str, Any], topology: DesktopTopology,
    role_process_ids: Mapping[str, int],
) -> tuple[dict[str, Any], dict[str, int]]:
    processes, pids = process_evidence(
        probe,
        topology,
        role_pid_hints=role_process_ids,
        direct_parent_pid=os.getpid(),
    )
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


def _configure_private_session(
    environment: Mapping[str, str], scenario: DesktopMatrixScenario | None
) -> str:
    config = Path(environment["XDG_CONFIG_HOME"])
    config.mkdir(parents=True, exist_ok=True)
    spec = (
        scenario.virtual
        if scenario is not None
        else VirtualOutputSpec("single-1080p", 1, 1920, 1080, 1920, 1080, 1.0)
    )
    # AGENT-CONTRACT: Windowed KWin applies its mode and scale from the exact
    # CLI. A persisted virtual-output record would make output-store policy
    # normalize the nested Wayland connector instead. S1/S2 retain the accepted
    # Virtual-N bootstrap record; matrix rows intentionally have no host-like
    # display persistence in their disposable XDG tree.
    if scenario is None:
        write_virtual_output_config(config, spec)
    (config / "kscreenlockerrc").write_text(
        "[Daemon]\nAutolock=false\nLockOnResume=false\n", encoding="utf-8"
    )
    return f"qindaqt-{environment['QINDAQT_SESSION_RUN_ID'][:12]}"


def _session_program(
    stage: ResolvedStage,
    environment: Mapping[str, str],
    scenario: DesktopMatrixScenario | None,
) -> Path:
    if scenario is None:
        return stage.executables["session"]
    wrapper = Path(environment["XDG_RUNTIME_DIR"]) / "qindaqt-matrix-session"
    executable = shlex.quote(str(stage.executables["session"]))
    profile = shlex.quote(scenario.profile_id)
    theme = shlex.quote(scenario.theme_id)
    wrapper.write_text(
        # The empty-root sandbox intentionally has no /bin compatibility
        # symlink. Use the read-only system /usr mount explicitly.
        f"#!/usr/bin/sh\nexec {executable} --profile {profile} --theme {theme}\n",
        encoding="utf-8",
    )
    wrapper.chmod(0o700)
    return wrapper


def _read_exact_process_arguments(
    process_id: int, executable: Path, expected_tail: list[str], role: str
) -> list[str]:
    try:
        raw = Path(f"/proc/{process_id}/cmdline").read_bytes()
    except OSError as error:
        raise RuntimeError(f"matrix {role} arguments are not observable") from error
    arguments = [item.decode("utf-8", "strict") for item in raw.split(b"\0") if item]
    expected = [str(executable), *expected_tail]
    if arguments != expected:
        raise RuntimeError(f"live {role} process did not receive the matrix selection")
    return arguments[1:]


def _start_desktop(
    arguments: argparse.Namespace,
    stage: ResolvedStage,
    environment: dict[str, str],
    state: RuntimeState,
    scenario: DesktopMatrixScenario | None,
) -> DesktopLaunch:
    runtime = Path(environment["XDG_RUNTIME_DIR"])
    bus = spawn_logged_process(
        "dbus-daemon",
        [str(arguments.dbus_daemon), "--session", "--nofork", "--nopidfile",
         f"--address={environment['DBUS_SESSION_BUS_ADDRESS']}"],
        environment,
    )
    state.track(bus, [arguments.dbus_daemon])
    private_bus_process_id = bus.pid
    wait_for_path(runtime / "bus", state, 5)
    for role in ("settings-service", "audio-service"):
        child = spawn_logged_process(role, [str(stage.executables[role])], environment)
        state.track(child, [stage.executables[role]])
    socket_name = _configure_private_session(environment, scenario)
    virtual = (
        scenario.virtual
        if scenario is not None
        else VirtualOutputSpec("single-1080p", 1, 1920, 1080, 1920, 1080, 1.0)
    )
    compositor_environment = dict(environment)
    parent_environment: dict[str, str] | None = None
    parent_bus_process_id: int | None = None
    parent_process_id: int | None = None
    if arguments.interactive:
        parent_socket = "qindaqt-parent-wayland"
        fractional_parent = scenario is not None and virtual.scale != 1.0
        if fractional_parent:
            parent_environment_only = dict(environment)
            parent_bus_path = runtime / "qindaqt-parent-bus"
            parent_bus = spawn_logged_process(
                "parent-private-bus",
                [str(arguments.dbus_daemon), "--session", "--nofork", "--nopidfile",
                 f"--address=unix:path={parent_bus_path}"],
                environment,
            )
            state.track(parent_bus, [arguments.dbus_daemon])
            parent_bus_process_id = parent_bus.pid
            wait_for_path(parent_bus_path, state, 5)
            # AGENT-GUARD: The raw parent owns this second namespace-private
            # bus, never the QindaQt child or host bus. This prevents it from
            # claiming the child's KGlobalAccel name.
            parent_environment_only["DBUS_SESSION_BUS_ADDRESS"] = (
                f"unix:path={parent_bus_path}"
            )
            parent_environment_only.update({
                "QT_NO_XDG_DESKTOP_PORTAL": "1",
                "GTK_USE_PORTAL": "0",
            })
            parent = spawn_logged_process(
                "parent-compositor",
                [str(arguments.kwin_wayland), "--virtual",
                 "--width", str(virtual.logical_width),
                 "--height", str(virtual.logical_height),
                 "--scale", str(virtual.scale), "--output-count", "1",
                 "--socket", parent_socket, "--no-lockscreen",
                 "--no-global-shortcuts"],
                parent_environment_only,
            )
            state.track(parent, [arguments.kwin_wayland])
        else:
            parent = spawn_logged_process(
                "parent-compositor",
                [str(arguments.weston), "--backend=headless", "--renderer=pixman",
                 "--shell=kiosk", "--debug", "--no-config", "--idle-time=0",
                 "--fake-seat", f"--width={virtual.pixel_width}",
                 f"--height={virtual.pixel_height}", "--scale=1",
                 f"--socket={parent_socket}",
                 "--log=/var/log/qindaqt-desktop/parent-compositor-weston.log"],
                environment,
            )
            state.track(parent, [arguments.weston])
        parent_process_id = parent.pid
        wait_for_path(runtime / parent_socket, state, 5)
        compositor_environment["WAYLAND_DISPLAY"] = parent_socket
        parent_environment = (
            dict(parent_environment_only) if fractional_parent else dict(environment)
        )
        parent_environment["WAYLAND_DISPLAY"] = parent_socket
    backend_arguments = [
        "--windowed" if arguments.interactive else "--virtual",
        # KWin's width/height pair is logical. The fractional scale produces
        # the physical buffer selected on the private parent; feeding
        # it pixel dimensions would incorrectly multiply the output mode.
        "--width", str(virtual.logical_width),
        "--height", str(virtual.logical_height),
        "--scale", str(virtual.scale), "--output-count", str(virtual.output_count),
    ]
    scenario_path = (
        f"/opt/qindaqt-source/tests/scenarios/{scenario.scenario_id}.json"
        if scenario is not None
        else "/opt/qindaqt-source/tests/scenarios/single-1080p.json"
    )
    compositor = spawn_logged_process(
        "compositor",
        [str(stage.executables["launcher"]), "--plugin-root",
         str(stage.compositor_plugin.parents[2]), *backend_arguments, "--socket",
         socket_name, "--test-scenario", scenario_path, "--session",
         str(_session_program(stage, environment, scenario))],
        compositor_environment,
    )
    state.track(compositor, [stage.executables["launcher"], arguments.kwin_wayland])
    wait_for_path(runtime / socket_name, state, 15)
    app_environment = dict(environment)
    app_environment["WAYLAND_DISPLAY"] = socket_name
    editor_arguments = (
        ["--theme", scenario.theme_id] if scenario is not None else []
    )
    for role, command in (
        ("settings-app", [str(stage.executables["settings-app"]), "--page", "notifications"]),
        ("editor-app", [str(stage.executables["editor-app"]), *editor_arguments])
    ):
        child = spawn_logged_process(role, command, app_environment)
        state.track(child, [stage.executables[role]])
    return DesktopLaunch(
        app_environment,
        parent_environment,
        socket_name,
        private_bus_process_id,
        parent_bus_process_id,
        parent_process_id,
        compositor.pid,
    )


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
    topology: DesktopTopology,
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

    document = await_complete_snapshot(sample, topology=topology)
    if current is None:
        raise RuntimeError("desktop readiness completed without a probe")
    return document, current


def _authenticate_processes(
    arguments: argparse.Namespace, stage: ResolvedStage,
    state: RuntimeState, pids: Mapping[str, int], probe: subprocess.Popen[str],
    topology: DesktopTopology,
) -> None:
    allowed = {
        item.role: [stage.executables[item.role]]
        for item in topology.processes
        if item.role in stage.executables
    }
    allowed.update({"private-bus": [arguments.dbus_daemon],
                    "compositor": [arguments.kwin_wayland],
                    "session-probe": [arguments.probe]})
    if arguments.interactive:
        parent_expectation = next(
            item for item in topology.processes if item.role == "parent-compositor"
        )
        allowed["parent-compositor"] = [
            arguments.kwin_wayland
            if parent_expectation.executable == "kwin_wayland"
            else arguments.weston
        ]
        if any(item.role == "parent-private-bus" for item in topology.processes):
            allowed["parent-private-bus"] = [arguments.dbus_daemon]
    for role, pid in pids.items():
        state.identities.append(capture_process_identity(role, pid, allowed[role]))


def _run_interaction(
    arguments: argparse.Namespace, environment: Mapping[str, str], state: RuntimeState,
    *, secondary_output: bool = False,
) -> dict[str, Any]:
    log = Path("/var/log/qindaqt-desktop/session-interaction.log").open(
        "w", encoding="utf-8"
    )
    try:
        process = subprocess.Popen(
            [
                str(arguments.probe),
                (
                    "--open-notification-center-secondary"
                    if secondary_output else "--open-notification-center"
                ),
            ],
            env=dict(environment), stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
            stderr=log, text=True, start_new_session=True,
        )
    except BaseException:
        log.close()
        raise
    process._qindaqt_log = log  # type: ignore[attr-defined]
    state.track(process, [arguments.probe])
    output, _ = process.communicate(timeout=5)
    # Preserve the exact marker before parsing, just as the readiness probe
    # does. The final canonical evidence remains separately validated.
    log.write(output)
    log.flush()
    marker = "QINDAQT_DESKTOP_SESSION_INTERACTION="
    lines = [line for line in output.splitlines() if line.startswith(marker)]
    if process.returncode != 0 or len(lines) != 1:
        raise RuntimeError("private-seat interaction did not return exact evidence")
    document = json.loads(lines[0].removeprefix(marker))
    if not isinstance(document, dict):
        raise RuntimeError("private-seat interaction evidence was malformed")
    return document


def _select_secondary_primary(
    environment: Mapping[str, str], state: RuntimeState,
) -> None:
    """Make WL-1 the private dual-row primary before opening shell chrome."""

    executable = Path("/usr/bin/kscreen-doctor")
    if not executable.is_file():
        raise RuntimeError("private multi-output selector is unavailable")
    log = Path("/var/log/qindaqt-desktop/secondary-primary.log").open(
        "w", encoding="utf-8"
    )
    try:
        process = subprocess.Popen(
            [
                str(executable),
                "output.WL-1.primary",
            ],
            env=dict(environment), stdin=subprocess.DEVNULL,
            stdout=log, stderr=subprocess.STDOUT, text=True,
            start_new_session=True,
        )
    except BaseException:
        log.close()
        raise
    process._qindaqt_log = log  # type: ignore[attr-defined]
    state.track(process, [executable])
    if process.wait(timeout=5) != 0:
        raise RuntimeError("private multi-output primary selection failed")


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
    scenario = (
        load_matrix_scenario(Path("/opt/qindaqt-source"), arguments.scenario_id)
        if arguments.interactive and arguments.scenario_id != "single-1080p"
        else None
    )
    topology: DesktopTopology = (
        interactive_matrix_topology(scenario)
        if scenario is not None
        else (interactive_1080p_topology() if arguments.interactive else desktop_1080p_topology())
    )
    try:
        launch = _start_desktop(arguments, stage, dict(os.environ), state, scenario)
        snapshot, probe = _await_runtime_snapshot(
            arguments, launch.app_environment, state, topology
        )
        role_process_ids = {
            "private-bus": launch.private_bus_process_id,
            "compositor": launch.compositor_process_id,
        }
        if launch.parent_bus_process_id is not None:
            role_process_ids["parent-private-bus"] = launch.parent_bus_process_id
        if launch.parent_process_id is not None:
            role_process_ids["parent-compositor"] = launch.parent_process_id
        evidence, pids = _build_evidence(snapshot, topology, role_process_ids)
        _authenticate_processes(arguments, stage, state, pids, probe, topology)
        matrix_session_arguments = None
        matrix_editor_arguments = None
        matrix_parent_arguments = None
        if scenario is not None:
            matrix_session_arguments = _read_exact_process_arguments(
                pids["session"], stage.executables["session"],
                ["--profile", scenario.profile_id, "--theme", scenario.theme_id],
                "session",
            )
            matrix_editor_arguments = _read_exact_process_arguments(
                pids["editor-app"], stage.executables["editor-app"],
                ["--theme", scenario.theme_id], "text editor",
            )
            if scenario.virtual.scale != 1.0:
                matrix_parent_arguments = _read_exact_process_arguments(
                    pids["parent-compositor"],
                    arguments.kwin_wayland,
                    [
                        "--virtual", "--width", str(scenario.virtual.logical_width),
                        "--height", str(scenario.virtual.logical_height),
                        "--scale", str(scenario.virtual.scale), "--output-count", "1",
                        "--socket", "qindaqt-parent-wayland", "--no-lockscreen",
                        "--no-global-shortcuts",
                    ],
                    "private parent compositor",
                )
        if arguments.interactive:
            if launch.parent_environment is None:
                raise RuntimeError("interactive launch omitted its private parent endpoint")
            fractional_parent = scenario is not None and scenario.virtual.scale != 1.0
            evidence["containment"].update({
                "parentBackend": (
                    "kwin-virtual-qpaint" if fractional_parent
                    else "weston-headless-pixman"
                ),
                "qindaqtBackend": "kwin-windowed-qpaint",
                "parentWaylandSocket": "qindaqt-parent-wayland",
                "childWaylandSocket": launch.child_socket,
            })
            secondary_output = (
                scenario is not None and scenario.virtual.output_count == 2
            )
            if secondary_output:
                _select_secondary_primary(launch.app_environment, state)
            evidence["interaction"] = _run_interaction(
                arguments,
                launch.app_environment,
                state,
                secondary_output=secondary_output,
            )
            interaction_surface = evidence["interaction"].get("surface", {})
            if not isinstance(interaction_surface, Mapping):
                raise RuntimeError("interactive surface evidence was malformed")
            interaction_geometry = interaction_surface.get("geometry", {})
            if not isinstance(interaction_geometry, Mapping):
                raise RuntimeError("interactive surface geometry was malformed")
            if scenario is None:
                evidence["capture"] = capture_parent_frame(
                    arguments.weston_screenshooter,
                    launch.parent_environment,
                    Path("/var/lib/qindaqt-evidence"),
                    content_region=interaction_geometry,
                )
            else:
                surface_name = interaction_surface.get("outputName")
                output_names = tuple(f"WL-{item.ordinal}" for item in scenario.outputs)
                if surface_name not in output_names:
                    raise RuntimeError("interaction did not bind to a matrix output")
                output = scenario.outputs[output_names.index(surface_name)]
                capture_arguments = {
                    "scenario_id": scenario.scenario_id,
                    "expected_width": output.pixel_width,
                    "expected_height": output.pixel_height,
                    "content_region": physical_content_region(
                        interaction_geometry, output
                    ),
                }
                if fractional_parent:
                    if launch.parent_process_id is None:
                        raise RuntimeError("fractional capture omitted its parent PID")
                    captures = capture_private_kwin_matrix(
                        arguments.python,
                        launch.parent_process_id,
                        launch.parent_environment,
                        Path("/var/lib/qindaqt-evidence"),
                        output_name=surface_name,
                        expected_logical_width=scenario.virtual.logical_width,
                        expected_logical_height=scenario.virtual.logical_height,
                        expected_scale=scenario.virtual.scale,
                        **capture_arguments,
                    )
                else:
                    captures = capture_parent_matrix(
                        arguments.weston_screenshooter,
                        launch.parent_environment,
                        Path("/var/lib/qindaqt-evidence"),
                        # Weston owns one parent framebuffer. KWin may publish
                        # multiple child outputs inside it; public inventories
                        # and per-output docks prove the complete arrangement.
                        output_names=(surface_name,),
                        content_output_name=surface_name,
                        **capture_arguments,
                    )
                evidence["matrixPresentation"] = {
                    "scenarioId": scenario.scenario_id,
                    "profileId": scenario.profile_id,
                    "themeId": scenario.theme_id,
                    "requestedScale": scenario.virtual.scale,
                    "parentArguments": matrix_parent_arguments,
                    "sessionArguments": matrix_session_arguments,
                    "editorArguments": matrix_editor_arguments,
                }
                evidence["matrixCaptures"] = captures
        samples = [read_process_sample(pids[role]) for role in PRODUCTION_PSS_ROLES]
        pss = aggregate_pss_kib(samples)
        evidence["measurements"] = {"residentPssKiB": pss, "ceilingKiB": 1024 * 1024}
        if pss > 1024 * 1024:
            raise RuntimeError(f"resident PSS exceeded 1024 MiB: {pss} KiB")
        probe.wait(timeout=2)
        if probe.returncode != 0:
            raise RuntimeError("desktop session probe failed")
    finally:
        cleanup_records = _cleanup(state)
    survivor_pids = sorted(
        identity.pid for identity in state.identities if identity_is_live(identity)
    )
    if survivor_pids:
        raise ProcessContractError(
            f"authenticated processes survived final observation: {survivor_pids}"
        )
    if evidence is None:
        raise RuntimeError("desktop evidence was not constructed")
    evidence["cleanup"] = {
        "bounded": True,
        "survivorPids": survivor_pids,
        "terminalPhases": [record.document() for record in cleanup_records],
    }
    if arguments.interactive:
        validate_interactive_evidence(evidence, topology)
    else:
        validate_boot_evidence(evidence)
    artifact = Path("/var/lib/qindaqt-evidence/desktop-session-evidence.json")
    artifact.write_text(json.dumps(evidence, sort_keys=True, indent=2) + "\n")
    print("QINDAQT_DESKTOP_SESSION_EVIDENCE=" + json.dumps(evidence, sort_keys=True))
    return 0
