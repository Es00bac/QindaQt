# SPDX-License-Identifier: GPL-3.0-or-later
"""Inner PID-namespace orchestration for the complete desktop proof."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import time
from pathlib import Path
from typing import Any, Mapping

from desktop_session_capture import (
    capture_parent_frame,
    capture_parent_matrix,
    capture_private_kwin_matrix,
)
from desktop_session_evidence import (
    PRODUCTION_PSS_ROLES,
    _authenticate_processes,
    _build_evidence,
)
from desktop_session_interaction_runtime import _prepare_secondary_evidence, _run_interaction
from desktop_session_interactive import validate_interactive_evidence
from desktop_session_launch import (
    DesktopLaunch,
    _read_exact_process_arguments,
    _session_program,
    _start_desktop,
)
from desktop_session_matrix import (
    DesktopMatrixScenario,
    load_matrix_scenario,
    physical_content_region,
)
from desktop_session_measure import aggregate_pss_kib, read_process_sample
from desktop_session_process import (
    CleanupRecord,
    ProcessContractError,
    RuntimeState,
    capture_process_identity,
    identity_is_live,
    terminate_processes,
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
    DesktopTopology,
    desktop_1080p_topology,
    interactive_1080p_topology,
    interactive_matrix_topology,
    validate_boot_evidence,
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
            [str(arguments.probe)], env=dict(environment),
            stdout=subprocess.PIPE, stderr=log, text=True,
            start_new_session=True,
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
            if current.returncode != 0:
                raise RuntimeError("desktop readiness probe failed")
        lifetime = require_probe_lifetime(remaining)
        attempt += 1
        current = _spawn_probe(arguments, environment, state, attempt)
        current_deadline = time.monotonic() + lifetime
        return read_probe_document(current, current_deadline)

    document = await_complete_snapshot(sample, topology=topology)
    if current is None:
        raise RuntimeError("desktop readiness completed without a probe")
    return document, current


def _cleanup(state: RuntimeState) -> list[CleanupRecord]:
    tracked = {identity.pid for identity in state.identities}
    failures: list[str] = []
    for process, allowed in state.spawned:
        if process.pid in tracked or process.poll() is not None:
            continue
        try:
            identity = capture_process_identity(
                f"direct-{process.pid}", process.pid, allowed
            )
        except ProcessContractError as error:
            # AGENT-GUARD: Namespace containment cannot replace exact teardown
            # evidence for a live child whose executable cannot authenticate.
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


def _scenario_and_topology(
    arguments: argparse.Namespace,
) -> tuple[DesktopMatrixScenario | None, DesktopTopology]:
    scenario = (
        load_matrix_scenario(Path("/opt/qindaqt-source"), arguments.scenario_id)
        if arguments.interactive and arguments.scenario_id != "single-1080p"
        else None
    )
    if scenario is not None:
        return scenario, interactive_matrix_topology(scenario)
    topology = (
        interactive_1080p_topology()
        if arguments.interactive else desktop_1080p_topology()
    )
    return scenario, topology


def _role_process_ids(launch: DesktopLaunch) -> dict[str, int]:
    result = {
        "private-bus": launch.private_bus_process_id,
        "compositor": launch.compositor_process_id,
    }
    if launch.parent_bus_process_id is not None:
        result["parent-private-bus"] = launch.parent_bus_process_id
    if launch.parent_process_id is not None:
        result["parent-compositor"] = launch.parent_process_id
    return result


def _matrix_process_arguments(
    arguments: argparse.Namespace,
    stage: ResolvedStage,
    scenario: DesktopMatrixScenario | None,
    pids: Mapping[str, int],
) -> tuple[list[str] | None, list[str] | None, list[str] | None]:
    if scenario is None:
        return None, None, None
    session = _read_exact_process_arguments(
        pids["session"], stage.executables["session"],
        ["--profile", scenario.profile_id, "--theme", scenario.theme_id],
        "session",
    )
    editor = _read_exact_process_arguments(
        pids["editor-app"], stage.executables["editor-app"],
        ["--theme", scenario.theme_id], "text editor",
    )
    parent = None
    if scenario.virtual.scale != 1.0:
        parent = _read_exact_process_arguments(
            pids["parent-compositor"], arguments.kwin_wayland,
            [
                "--virtual", "--width", str(scenario.virtual.logical_width),
                "--height", str(scenario.virtual.logical_height),
                "--scale", str(scenario.virtual.scale), "--output-count", "1",
                "--socket", "qindaqt-parent-wayland", "--no-lockscreen",
                "--no-global-shortcuts",
            ],
            "private parent compositor",
        )
    return session, editor, parent


def _matrix_captures(
    arguments: argparse.Namespace,
    launch: DesktopLaunch,
    scenario: DesktopMatrixScenario,
    surface_name: object,
    geometry: Mapping[str, Any],
) -> list[dict[str, object]]:
    output_names = tuple(f"WL-{item.ordinal}" for item in scenario.outputs)
    if surface_name not in output_names:
        raise RuntimeError("interaction did not bind to a matrix output")
    output = scenario.outputs[output_names.index(surface_name)]
    common = {
        "scenario_id": scenario.scenario_id,
        "expected_width": output.pixel_width,
        "expected_height": output.pixel_height,
        "content_region": physical_content_region(geometry, output),
    }
    evidence_root = Path("/var/lib/qindaqt-evidence")
    if scenario.virtual.scale != 1.0:
        if launch.parent_process_id is None:
            raise RuntimeError("fractional capture omitted its parent PID")
        return capture_private_kwin_matrix(
            arguments.python,
            launch.parent_process_id,
            launch.parent_environment or {},
            evidence_root,
            output_name=str(surface_name),
            expected_logical_width=scenario.virtual.logical_width,
            expected_logical_height=scenario.virtual.logical_height,
            expected_scale=scenario.virtual.scale,
            **common,
        )
    return capture_parent_matrix(
        arguments.weston_screenshooter,
        launch.parent_environment or {},
        evidence_root,
        # Weston owns one parent framebuffer; public inventories and per-output
        # docks prove the complete child arrangement.
        output_names=(str(surface_name),),
        content_output_name=str(surface_name),
        **common,
    )


def _add_interactive_evidence(
    arguments: argparse.Namespace,
    launch: DesktopLaunch,
    state: RuntimeState,
    scenario: DesktopMatrixScenario | None,
    evidence: dict[str, Any],
    matrix_arguments: tuple[list[str] | None, list[str] | None, list[str] | None],
) -> None:
    if launch.parent_environment is None:
        raise RuntimeError("interactive launch omitted its private parent endpoint")
    fractional_parent = scenario is not None and scenario.virtual.scale != 1.0
    evidence["containment"].update({
        "parentBackend": (
            "kwin-virtual-qpaint" if fractional_parent else "weston-headless-pixman"
        ),
        "qindaqtBackend": "kwin-windowed-qpaint",
        "parentWaylandSocket": "qindaqt-parent-wayland",
        "childWaylandSocket": launch.child_socket,
    })
    secondary = scenario is not None and scenario.virtual.output_count == 2
    if secondary:
        _prepare_secondary_evidence(arguments, launch.app_environment, state, evidence)
    interaction = _run_interaction(
        arguments, launch.app_environment, state, secondary_output=secondary
    )
    evidence["interaction"] = interaction
    surface = interaction.get("surface", {})
    if not isinstance(surface, Mapping):
        raise RuntimeError("interactive surface evidence was malformed")
    geometry = surface.get("geometry", {})
    if not isinstance(geometry, Mapping):
        raise RuntimeError("interactive surface geometry was malformed")
    if scenario is None:
        evidence["capture"] = capture_parent_frame(
            arguments.weston_screenshooter,
            launch.parent_environment,
            Path("/var/lib/qindaqt-evidence"),
            content_region=geometry,
        )
        return
    session, editor, parent = matrix_arguments
    evidence["matrixPresentation"] = {
        "scenarioId": scenario.scenario_id,
        "profileId": scenario.profile_id,
        "themeId": scenario.theme_id,
        "requestedScale": scenario.virtual.scale,
        "parentArguments": parent,
        "sessionArguments": session,
        "editorArguments": editor,
    }
    evidence["matrixCaptures"] = _matrix_captures(
        arguments, launch, scenario, surface.get("outputName"), geometry
    )


def _measure_and_finish_probe(
    pids: Mapping[str, int],
    evidence: dict[str, Any],
    probe: subprocess.Popen[str],
) -> None:
    samples = [read_process_sample(pids[role]) for role in PRODUCTION_PSS_ROLES]
    pss = aggregate_pss_kib(samples)
    evidence["measurements"] = {
        "residentPssKiB": pss,
        "ceilingKiB": 1024 * 1024,
    }
    if pss > 1024 * 1024:
        raise RuntimeError(f"resident PSS exceeded 1024 MiB: {pss} KiB")
    probe.wait(timeout=2)
    if probe.returncode != 0:
        raise RuntimeError("desktop session probe failed")


def _publish_final_evidence(
    arguments: argparse.Namespace,
    topology: DesktopTopology,
    state: RuntimeState,
    evidence: dict[str, Any] | None,
    cleanup_records: list[CleanupRecord],
) -> None:
    survivors = sorted(
        identity.pid for identity in state.identities if identity_is_live(identity)
    )
    if survivors:
        raise ProcessContractError(
            f"authenticated processes survived final observation: {survivors}"
        )
    if evidence is None:
        raise RuntimeError("desktop evidence was not constructed")
    evidence["cleanup"] = {
        "bounded": True,
        "survivorPids": survivors,
        "terminalPhases": [record.document() for record in cleanup_records],
    }
    if arguments.interactive:
        validate_interactive_evidence(evidence, topology)
    else:
        validate_boot_evidence(evidence)
    artifact = Path("/var/lib/qindaqt-evidence/desktop-session-evidence.json")
    artifact.write_text(json.dumps(evidence, sort_keys=True, indent=2) + "\n")
    print("QINDAQT_DESKTOP_SESSION_EVIDENCE=" + json.dumps(evidence, sort_keys=True))


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
    scenario, topology = _scenario_and_topology(arguments)
    state = RuntimeState()
    evidence: dict[str, Any] | None = None
    cleanup_records: list[CleanupRecord] = []
    try:
        launch = _start_desktop(arguments, stage, dict(os.environ), state, scenario)
        snapshot, probe = _await_runtime_snapshot(
            arguments, launch.app_environment, state, topology
        )
        evidence, pids = _build_evidence(
            snapshot, topology, _role_process_ids(launch)
        )
        _authenticate_processes(arguments, stage, state, pids, probe, topology)
        matrix_arguments = _matrix_process_arguments(
            arguments, stage, scenario, pids
        )
        if arguments.interactive:
            _add_interactive_evidence(
                arguments, launch, state, scenario, evidence, matrix_arguments
            )
        _measure_and_finish_probe(pids, evidence, probe)
    finally:
        cleanup_records = _cleanup(state)
    _publish_final_evidence(arguments, topology, state, evidence, cleanup_records)
    return 0
