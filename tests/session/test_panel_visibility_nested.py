# SPDX-License-Identifier: GPL-3.0-or-later
"""Installed private-desktop proof for window-aware panel producers."""

from __future__ import annotations

import argparse
import dataclasses
import json
import os
import subprocess
import sys
from pathlib import Path, PurePosixPath

import test_desktop_session_nested as base
import desktop_session_runtime as runtime
from desktop_session_capture import validate_capture
from desktop_session_evidence import _authenticate_processes, _build_evidence
from desktop_session_launch import _start_desktop
from desktop_session_matrix import DesktopMatrixScenario, MatrixOutput, load_matrix_scenario
from desktop_session_process import RuntimeState, identity_is_live
from desktop_session_sandbox import FORBIDDEN_ENVIRONMENT, ReadOnlyMount, SandboxContractError
from desktop_session_stage import resolve_stage
from desktop_session_topology import interactive_matrix_topology
from nested_session_scenario import VirtualOutputSpec


PROFILE_ID = "panel-visibility-proof"


def _scenario(identifier: str) -> DesktopMatrixScenario:
    if identifier == "single-1080p":
        virtual = VirtualOutputSpec(identifier, 1, 1920, 1080, 1920, 1080, 1.0)
        return DesktopMatrixScenario(
            identifier, PROFILE_ID, "qinda-dark", virtual,
            (MatrixOutput(0, 1920, 1080, 0, 0, 1920, 1080, 1.0),),
        )
    loaded = load_matrix_scenario(Path("/opt/qindaqt-source"), identifier)
    return dataclasses.replace(loaded, profile_id=PROFILE_ID, theme_id="qinda-dark")


def _role_process_ids(launch: object) -> dict[str, int]:
    result = {
        "private-bus": launch.private_bus_process_id,
        "compositor": launch.compositor_process_id,
    }
    if launch.parent_bus_process_id is not None:
        result["parent-private-bus"] = launch.parent_bus_process_id
    if launch.parent_process_id is not None:
        result["parent-compositor"] = launch.parent_process_id
    return result


def _run_visibility_probe(arguments: argparse.Namespace, environment: dict[str, str],
                          state: RuntimeState) -> dict[str, object]:
    log_path = Path("/var/log/qindaqt-desktop/panel-visibility-probe.log")
    log = log_path.open("w", encoding="utf-8")
    try:
        process = subprocess.Popen(
            [str(arguments.visibility_probe), str(arguments.weston_screenshooter)],
            env=environment, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
            stderr=log, text=True, start_new_session=True,
        )
    except BaseException:
        log.close()
        raise
    process._qindaqt_log = log  # type: ignore[attr-defined]
    state.track(process, [arguments.visibility_probe])
    output, _ = process.communicate(timeout=35)
    log.write(output)
    log.flush()
    marker = "QINDAQT_PANEL_VISIBILITY_EVIDENCE="
    lines = [line for line in output.splitlines() if line.startswith(marker)]
    if process.returncode != 0 or len(lines) != 1:
        raise RuntimeError(
            f"panel visibility probe exited {process.returncode}; see {log_path}"
        )
    document = json.loads(lines[0].removeprefix(marker))
    if not isinstance(document, dict) or len(document.get("phases", [])) != 6:
        raise RuntimeError("panel visibility probe evidence is incomplete")
    return document


def _validate_captures(width: int, height: int) -> list[dict[str, object]]:
    root = Path("/var/lib/qindaqt-evidence")
    phases = (
        "window-overlap-hidden", "window-moved-away", "edge-revealed",
        "shortcut-revealed", "popup-held", "popup-closed",
    )
    captures = []
    for phase in phases:
        path = root / f"panel-{phase}.png"
        captures.append(validate_capture(
            path, expected_width=width, expected_height=height,
            minimum_colors=8, evidence_path=path.name,
        ))
    return captures


def run_inner(arguments: argparse.Namespace) -> int:
    if FORBIDDEN_ENVIRONMENT.intersection(os.environ):
        raise SandboxContractError("inner process inherited a forbidden host endpoint")
    stage = resolve_stage(
        arguments.stage_root, bin_directory=arguments.bin_directory,
        plugin_relative=arguments.plugin_relative,
        decoration_relative=arguments.decoration_relative,
        settings_service_directory=arguments.settings_service_directory,
        audio_service_directory=arguments.audio_service_directory,
    )
    scenario = _scenario(arguments.scenario_id)
    topology = interactive_matrix_topology(scenario)
    state = RuntimeState()
    evidence: dict[str, object] | None = None
    cleanup = []
    try:
        launch = _start_desktop(arguments, stage, dict(os.environ), state, scenario)
        snapshot, readiness_probe = runtime._await_runtime_snapshot(
            arguments, launch.app_environment, state, topology
        )
        evidence, pids = _build_evidence(
            snapshot, topology, _role_process_ids(launch)
        )
        _authenticate_processes(
            arguments, stage, state, pids, readiness_probe, topology
        )
        runtime._measure_and_finish_probe(pids, evidence, readiness_probe)
        interaction = _run_visibility_probe(
            arguments, launch.app_environment, state
        )
        evidence["panelVisibility"] = interaction
        evidence["panelVisibilityCaptures"] = _validate_captures(
            scenario.virtual.pixel_width, scenario.virtual.pixel_height
        )
    finally:
        cleanup = runtime._cleanup(state)
    survivors = sorted(
        identity.pid for identity in state.identities if identity_is_live(identity)
    )
    if survivors or evidence is None:
        raise RuntimeError(f"panel visibility teardown survivors: {survivors}")
    evidence["cleanup"] = {
        "bounded": True, "survivorPids": survivors,
        "terminalPhases": [record.document() for record in cleanup],
    }
    artifact = Path("/var/lib/qindaqt-evidence/panel-visibility-evidence.json")
    artifact.write_text(json.dumps(evidence, sort_keys=True, indent=2) + "\n")
    print("QINDAQT_PANEL_VISIBILITY_SESSION=" + json.dumps(evidence, sort_keys=True))
    return 0


def _install_custom_spec() -> None:
    original = base._make_spec

    def make_spec(arguments: argparse.Namespace, run_id: str, paths: object):
        spec = original(arguments, run_id, paths)
        command = list(spec.command)
        command[1] = "/opt/qindaqt-source/tests/session/test_panel_visibility_nested.py"
        command.extend((
            "--visibility-probe",
            "/opt/qindaqt-tools/qindaqt-panel-visibility-session-probe",
        ))
        return dataclasses.replace(
            spec,
            probe=ReadOnlyMount(
                arguments.probe.parent, PurePosixPath("/opt/qindaqt-tools")
            ),
            command=tuple(command),
        )

    base._make_spec = make_spec


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--outer", action="store_true")
    mode.add_argument("--inner", action="store_true")
    parser.add_argument("--build-root", type=Path)
    parser.add_argument("--source-root", type=Path)
    parser.add_argument("--stage-root", type=Path, required=True)
    parser.add_argument("--bin-directory", required=True)
    parser.add_argument("--plugin-relative", required=True)
    parser.add_argument("--decoration-relative", required=True)
    parser.add_argument("--settings-service-directory", required=True)
    parser.add_argument("--audio-service-directory", required=True)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--visibility-probe", type=Path)
    parser.add_argument("--bwrap", type=Path)
    parser.add_argument("--python", type=Path, default=Path(sys.executable))
    parser.add_argument("--dbus-daemon", type=Path, required=True)
    parser.add_argument("--kwin-wayland", type=Path, required=True)
    parser.add_argument("--weston", type=Path, required=True)
    parser.add_argument("--weston-screenshooter", type=Path, required=True)
    parser.add_argument("--scenario-id", default="single-1080p")
    parser.add_argument("--interactive", action="store_true", default=True)
    parser.add_argument("--kscreen-doctor", type=Path)
    parser.add_argument("--kscreen-wayland-backend", type=Path)
    parser.add_argument("--run-id", default="")
    parser.add_argument("--print-command-json", action="store_true")
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        if arguments.outer:
            if arguments.visibility_probe is None:
                raise SandboxContractError("outer mode requires the visibility probe")
            _install_custom_spec()
            return base.run_outer(arguments)
        if arguments.visibility_probe is None:
            raise SandboxContractError("inner visibility probe was not mounted")
        return run_inner(arguments)
    except (OSError, RuntimeError, subprocess.SubprocessError, ValueError) as error:
        print(f"panel visibility qualification failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
