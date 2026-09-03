# SPDX-License-Identifier: GPL-3.0-or-later
"""Installed private-desktop proof for window-aware panel producers."""

from __future__ import annotations

import argparse
import dataclasses
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path, PurePosixPath

import test_desktop_session_nested as base
import desktop_session_runtime as runtime
from desktop_session_capture import CaptureContractError, _decode_png
from desktop_session_evidence import _authenticate_processes, _build_evidence
from desktop_session_launch import _start_desktop
from desktop_session_matrix import DesktopMatrixScenario, MatrixOutput, load_matrix_scenario
from desktop_session_process import RuntimeState, identity_is_live
from desktop_session_sandbox import FORBIDDEN_ENVIRONMENT, ReadOnlyMount, SandboxContractError
from desktop_session_stage import resolve_stage
from desktop_session_topology import interactive_matrix_topology
from nested_session_scenario import VirtualOutputSpec


PROFILE_ID = "panel-visibility-proof"
PHASES = (
    "window-overlap-hidden", "window-moved-away", "window-close-hidden",
    "window-closed-restored", "edge-revealed", "shortcut-revealed",
    "popup-held", "popup-closed",
)


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
    if not isinstance(document, dict) or len(document.get("phases", [])) != len(PHASES):
        raise RuntimeError("panel visibility probe evidence is incomplete")
    return document


def _geometry(value: object, location: str, width: int, height: int) -> dict[str, int]:
    if not isinstance(value, dict) or set(value) != {"x", "y", "width", "height"}:
        raise RuntimeError(f"{location} geometry is malformed")
    if any(isinstance(value[key], bool) or not isinstance(value[key], int)
           for key in value):
        raise RuntimeError(f"{location} geometry is not integral")
    geometry = {key: value[key] for key in ("x", "y", "width", "height")}
    if (geometry["x"] < 0 or geometry["y"] < 0 or geometry["width"] <= 0
            or geometry["height"] <= 0
            or geometry["x"] + geometry["width"] > width
            or geometry["y"] + geometry["height"] > height):
        raise RuntimeError(f"{location} geometry escapes the framebuffer")
    return geometry


def _intersects(first: dict[str, int], second: dict[str, int]) -> bool:
    return (
        first["x"] < second["x"] + second["width"]
        and second["x"] < first["x"] + first["width"]
        and first["y"] < second["y"] + second["height"]
        and second["y"] < first["y"] + first["height"]
    )


def _panel_geometry(surfaces: object, edge: str, thickness: int,
                    width: int, height: int) -> dict[str, int] | None:
    if not isinstance(surfaces, list):
        raise RuntimeError("panel phase surface authority is malformed")
    matches: list[dict[str, int]] = []
    for index, surface in enumerate(surfaces):
        if not isinstance(surface, dict):
            raise RuntimeError("panel phase surface record is malformed")
        geometry = _geometry(
            surface.get("geometry"), f"surface[{index}]", width, height
        )
        placed = (
            edge == "top" and geometry["y"] == 0
            and geometry["height"] == thickness
        ) or (
            edge == "left" and geometry["x"] == 0
            and geometry["width"] == thickness
        ) or (
            edge == "bottom" and geometry["height"] == thickness
            and geometry["y"] + geometry["height"] == height
        )
        if placed and surface.get("mapped") is True and surface.get("committed") is True:
            matches.append(geometry)
    if len(matches) > 1:
        raise RuntimeError(f"panel authority contains duplicate {edge} surfaces")
    return matches[0] if matches else None


def _validate_interaction(interaction: object, width: int,
                          height: int) -> dict[str, dict[str, int]]:
    if not isinstance(interaction, dict) or interaction.get("schemaVersion") != 1:
        raise RuntimeError("panel interaction envelope is malformed")
    if (interaction.get("outputWidth"), interaction.get("outputHeight")) != (width, height):
        raise RuntimeError("panel interaction output geometry disagrees with capture")
    phases = interaction.get("phases")
    if not isinstance(phases, list) or len(phases) != len(PHASES):
        raise RuntimeError("panel interaction phase count is not exact")
    documents: dict[str, dict[str, object]] = {}
    for expected, phase in zip(PHASES, phases, strict=True):
        if not isinstance(phase, dict) or phase.get("phase") != expected:
            raise RuntimeError("panel interaction phases are not canonical")
        if _panel_geometry(phase.get("surfaces"), "top", 30, width, height) is None:
            raise RuntimeError(f"top reservation authority is absent in {expected}")
        documents[expected] = phase

    left_visible = [
        _panel_geometry(documents[name].get("surfaces"), "left", 40, width, height)
        for name in ("window-moved-away", "window-closed-restored")
    ]
    if left_visible[0] is None or left_visible[0] != left_visible[1]:
        raise RuntimeError("left panel visible authority is missing or unstable")
    for name in ("window-overlap-hidden", "window-close-hidden"):
        if _panel_geometry(documents[name].get("surfaces"), "left", 40,
                           width, height) is not None:
            raise RuntimeError(f"left panel is not hidden in {name}")

    bottom_names = ("edge-revealed", "shortcut-revealed", "popup-held")
    bottom_visible = [
        _panel_geometry(documents[name].get("surfaces"), "bottom", 48, width, height)
        for name in bottom_names
    ]
    if bottom_visible[0] is None or any(
        geometry != bottom_visible[0] for geometry in bottom_visible[1:]
    ):
        raise RuntimeError("bottom panel visible authority is missing or unstable")
    if _panel_geometry(documents["popup-closed"].get("surfaces"), "bottom", 48,
                       width, height) is not None:
        raise RuntimeError("bottom panel is not hidden after popup close")

    move = interaction.get("move")
    if (not isinstance(move, dict)
            or set(move) != {"before", "after", "surfacesBefore"}):
        raise RuntimeError("window move geometry proof is absent")
    if _panel_geometry(move["surfacesBefore"], "top", 30, width, height) is None:
        raise RuntimeError("pre-drag top authority is absent")
    if _panel_geometry(move["surfacesBefore"], "left", 40,
                       width, height) is not None:
        raise RuntimeError("left panel was not hidden immediately before drag")
    before = _geometry(move["before"], "move.before", width, height)
    after = _geometry(move["after"], "move.after", width, height)
    if before["x"] == after["x"] and before["y"] == after["y"]:
        raise RuntimeError("injected drag did not move the proof window")
    if after["width"] >= width or after["height"] >= height:
        raise RuntimeError("injected drag did not leave a movable proof window")
    if _intersects(after, left_visible[0]):
        raise RuntimeError("window move does not clear the left panel")
    close = interaction.get("close")
    if (not isinstance(close, dict) or set(close) != {"before", "windowAbsentAfter"}
            or close.get("windowAbsentAfter") is not True):
        raise RuntimeError("window close authority proof is absent")
    close_before = _geometry(close["before"], "close.before", width, height)
    if not _intersects(close_before, left_visible[0]):
        raise RuntimeError("closing window did not cover the left panel")
    return {"left": left_visible[0], "bottom": bottom_visible[0]}


def _validate_panel_capture(path: Path, width: int, height: int,
                            geometry: dict[str, int]) -> dict[str, object]:
    actual_width, actual_height, pixels, channels = _decode_png(path)
    if (actual_width, actual_height) != (width, height):
        raise CaptureContractError("captured framebuffer dimensions are not exact")
    frame_colors: set[bytes] = set()
    for row in range(72):
        y = min(height - 1, row * height // 72)
        for column in range(128):
            x = min(width - 1, column * width // 128)
            start = (y * width + x) * channels
            frame_colors.add(pixels[start:start + channels])
    if len(frame_colors) < 8:
        raise CaptureContractError("captured framebuffer is visually uniform")
    digest = hashlib.sha256()
    colors: set[bytes] = set()
    for y in range(geometry["y"], geometry["y"] + geometry["height"]):
        start = (y * width + geometry["x"]) * channels
        row = pixels[start:start + geometry["width"] * channels]
        digest.update(row)
        for offset in range(0, len(row), channels):
            colors.add(row[offset:offset + channels])
    return {
        "tool": "weston-screenshooter", "path": path.name,
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "byteCount": path.stat().st_size, "width": width, "height": height,
        "sampledDistinctColors": len(frame_colors),
        "panelRegion": {**geometry, "sha256": digest.hexdigest(),
                        "sampledDistinctColors": len(colors)},
    }


def _validate_captures(width: int, height: int,
                       interaction: object) -> list[dict[str, object]]:
    root = Path("/var/lib/qindaqt-evidence")
    panel_geometries = _validate_interaction(interaction, width, height)
    captures = []
    for phase in PHASES:
        path = root / f"panel-{phase}.png"
        panel = "left" if phase.startswith("window-") else "bottom"
        capture = _validate_panel_capture(
            path, width, height, panel_geometries[panel]
        )
        state = "hidden" if phase in {
            "window-overlap-hidden", "window-close-hidden", "popup-closed"
        } else "visible"
        capture.update({"phase": phase, "panel": panel, "panelState": state})
        captures.append(capture)
    by_phase = {capture["phase"]: capture for capture in captures}
    pairs = (
        ("window-overlap-hidden", "window-moved-away"),
        ("window-close-hidden", "window-closed-restored"),
        ("popup-closed", "edge-revealed"),
        ("popup-closed", "shortcut-revealed"),
        ("popup-closed", "popup-held"),
    )
    # AGENT-NOTE: P1-3 accepted six copies of one unrelated image. Every
    # hidden/visible assertion now compares pixels in the exact authority-owned
    # panel rectangle, rather than whole-frame dimensions or color counts.
    for hidden, visible in pairs:
        if (by_phase[hidden]["panelRegion"]["sha256"]
                == by_phase[visible]["panelRegion"]["sha256"]):
            raise RuntimeError(
                f"panel pixels did not change from {hidden} to {visible}"
            )
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
            scenario.virtual.pixel_width, scenario.virtual.pixel_height,
            interaction,
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
