# SPDX-License-Identifier: GPL-3.0-or-later
"""Prove production layer panels and work-area reservation under nested KWin."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any

from nested_session_scenario import (
    ScenarioCoverageError,
    VirtualOutputSpec,
    isolated_environment,
    load_virtual_spec,
    write_virtual_output_config,
)
from shell_surface_protocol_validation import (
    EXPECTED_RESERVATION,
    validate_active_protocol,
    validate_final_trace,
)


MARKER = "QINDAQT_SHELL_SURFACE_PROBE="


def _object(value: Any, location: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise RuntimeError(f"{location} must be a JSON object")
    return value


def _validate_surface_topology(
    protocol: dict[str, Any], spec: VirtualOutputSpec, location: str
) -> set[tuple[str, str, str]]:
    return validate_active_protocol(protocol, spec, location)


def _validate_final_trace(
    protocol: dict[str, Any], active_identities: set[tuple[str, str, str]]
) -> None:
    validate_final_trace(protocol, active_identities)


def extract_probe_result(stdout: str) -> dict[str, Any]:
    markers = [
        line.removeprefix(MARKER)
        for line in stdout.splitlines()
        if line.startswith(MARKER)
    ]
    if len(markers) != 1:
        raise RuntimeError(f"expected exactly one shell-surface marker, observed {len(markers)}")
    return _object(json.loads(markers[0]), "probe result")


def _geometry(result: dict[str, Any], name: str) -> dict[str, Any]:
    geometry = _object(result.get(name), name)
    for field in ("x", "y", "width", "height"):
        if not isinstance(geometry.get(field), int) or isinstance(geometry.get(field), bool):
            raise RuntimeError(f"{name}.{field} must be an integer")
    return geometry


def validate_probe_result(result: dict[str, Any], spec: VirtualOutputSpec) -> None:
    if result.get("passed") is not True or result.get("platform") != "wayland":
        raise RuntimeError(f"probe did not pass on Wayland: {result.get('failure', '')}")
    if result.get("shellStarted") is not True or result.get("shellStopped") is not True:
        raise RuntimeError("production shell lifecycle was not bounded by the probe")
    if result.get("expectedReservation") != EXPECTED_RESERVATION:
        raise RuntimeError("probe and QindaQt profile reservation contract disagree")

    output = _geometry(result, "outputGeometry")
    if output["width"] != spec.logical_width or output["height"] != spec.logical_height:
        raise RuntimeError(
            "live output geometry did not match "
            f"{spec.logical_width}x{spec.logical_height}"
        )

    baseline = _geometry(result, "baselineWindowGeometry")
    reserved = _geometry(result, "reservedWindowGeometry")
    restored = _geometry(result, "restoredWindowGeometry")
    # AGENT-NOTE: xdg_toplevel does not expose compositor-global placement to
    # ordinary Wayland clients. A frameless maximized client's configured size
    # is the exact observable work-area contract; x/y remain diagnostic only.
    expected_baseline = (spec.logical_width, spec.logical_height)
    expected_reserved = (spec.logical_width, spec.logical_height - EXPECTED_RESERVATION)
    if (baseline["width"], baseline["height"]) != expected_baseline:
        raise RuntimeError(f"baseline maximize did not occupy {expected_baseline}")
    if (reserved["width"], reserved["height"]) != expected_reserved:
        raise RuntimeError(f"panel-aware maximize did not occupy {expected_reserved}")
    if (restored["width"], restored["height"]) != expected_baseline:
        raise RuntimeError("maximize area did not return to the full output after shell exit")
    if (
        result.get("maximizedWorkAreaAffected") is not True
        or result.get("workAreaRestoredAfterShellExit") is not True
    ):
        raise RuntimeError("causal work-area flags were not established")

    if result.get("activeMappedSnapshotTaken") is not True:
        raise RuntimeError("probe did not snapshot the active mapped panel epoch")
    active_protocol = _object(
        result.get("activeMappedLayerProtocol"), "activeMappedLayerProtocol"
    )
    identities = _validate_surface_topology(
        active_protocol, spec, "activeMappedLayerProtocol"
    )
    _validate_final_trace(_object(result.get("layerProtocol"), "layerProtocol"), identities)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("launcher", type=Path)
    parser.add_argument("probe", type=Path)
    parser.add_argument("shell", type=Path)
    parser.add_argument("dbus_runner", type=Path)
    parser.add_argument("scenario", type=Path)
    parser.add_argument("--plugin-root", type=Path, required=True)
    parser.add_argument("--profile-dir", type=Path, required=True)
    parser.add_argument("--theme-dir", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        spec = load_virtual_spec(arguments.scenario)
    except ScenarioCoverageError as error:
        print(f"scenario is not representable: {error}", file=sys.stderr)
        return 2
    if spec.output_count != 1:
        print("shell-surface proof currently requires one output", file=sys.stderr)
        return 2

    for path, description in (
        (arguments.launcher, "qindaqt-wm"),
        (arguments.probe, "shell-surface probe"),
        (arguments.shell, "qindaqt-shell"),
        (arguments.dbus_runner, "dbus-run-session"),
    ):
        if not path.is_file():
            print(f"{description} is unavailable: {path}", file=sys.stderr)
            return 2
    for path, description in (
        (arguments.plugin_root, "plugin root"),
        (arguments.profile_dir, "profile catalog"),
        (arguments.theme_dir, "theme catalog"),
    ):
        if not path.is_dir():
            print(f"{description} is unavailable: {path}", file=sys.stderr)
            return 2

    with tempfile.TemporaryDirectory(prefix="qindaqt-shell-surface-") as directory:
        environment = isolated_environment(Path(directory))
        write_virtual_output_config(Path(environment["XDG_CONFIG_HOME"]), spec)
        environment.update(
            {
                "QINDAQT_SHELL_EXECUTABLE": str(arguments.shell),
                "QINDAQT_SHELL_PROFILE_DIR": str(arguments.profile_dir),
                "QINDAQT_SHELL_THEME_DIR": str(arguments.theme_dir),
            }
        )
        launcher_command = [
            str(arguments.launcher),
            "--plugin-root",
            str(arguments.plugin_root),
            "--virtual",
            "--width",
            str(spec.logical_width),
            "--height",
            str(spec.logical_height),
            "--scale",
            f"{spec.scale:.12g}",
            "--output-count",
            str(spec.output_count),
            "--no-lockscreen",
            "--no-global-shortcuts",
            "--session",
            str(arguments.probe),
        ]
        # AGENT-GUARD: This is a production shell-client proof, not a
        # compositor mutation test. Do not add --test-scenario or any
        # development-control environment marker to this command.
        try:
            completed = subprocess.run(
                [str(arguments.dbus_runner), "--", *launcher_command],
                env=environment,
                text=True,
                capture_output=True,
                timeout=30,
                check=False,
            )
        except subprocess.TimeoutExpired as error:
            print(f"nested shell-surface proof timed out: {error}", file=sys.stderr)
            return 1

    if completed.returncode != 0:
        print(completed.stdout, file=sys.stderr)
        print(completed.stderr, file=sys.stderr)
        return completed.returncode or 1
    try:
        result = extract_probe_result(completed.stdout)
        validate_probe_result(result, spec)
    except (RuntimeError, json.JSONDecodeError) as error:
        print(f"nested shell-surface validation failed: {error}", file=sys.stderr)
        print(completed.stdout, file=sys.stderr)
        print(completed.stderr, file=sys.stderr)
        return 1

    result["scenarioCoverage"] = spec.coverage
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
