# SPDX-License-Identifier: GPL-3.0-or-later
"""Boot QindaQt/KWin with one exactly representable display scenario."""

from __future__ import annotations

import argparse
import json
import math
import os
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any


KWIN_ABI = "6.6.5"


class ScenarioCoverageError(ValueError):
    """The virtual CLI cannot faithfully bootstrap the requested initial outputs."""


@dataclass(frozen=True)
class VirtualOutputSpec:
    """The common initial output state representable by KWin's virtual CLI."""

    scenario_id: str
    output_count: int
    pixel_width: int
    pixel_height: int
    logical_width: int
    logical_height: int
    scale: float

    @property
    def coverage(self) -> dict[str, object]:
        return {
            "scenario": self.scenario_id,
            "applied": [
                "initial enabled output count",
                "common pixel mode",
                "common scale",
            ],
            "notApplied": [
                "output names",
                "primary selection",
                "positions",
                "refresh rates",
                "event sequence",
            ],
        }


def _mapping(value: Any, location: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ScenarioCoverageError(f"{location} must be an object")
    return value


def _positive_int(value: Any, location: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
        raise ScenarioCoverageError(f"{location} must be a positive integer")
    return value


def _positive_number(value: Any, location: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ScenarioCoverageError(f"{location} must be a positive number")
    number = float(value)
    if not math.isfinite(number) or number <= 0:
        raise ScenarioCoverageError(f"{location} must be a finite positive number")
    return number


def _logical_extent(pixels: int, scale: float, location: str) -> int:
    logical = pixels / scale
    rounded = round(logical)
    if not math.isclose(logical, rounded, rel_tol=0.0, abs_tol=1e-9):
        raise ScenarioCoverageError(
            f"{location}={pixels} at scale {scale:g} has non-integral logical size "
            f"{logical:g}; qindaqt-wm's current CLI cannot represent it exactly"
        )
    return rounded


def virtual_spec_from_document(document: Any, source: str = "scenario") -> VirtualOutputSpec:
    """Extract only the initial output state the current virtual CLI can apply."""
    root = _mapping(document, source)
    scenario_id = root.get("id")
    if not isinstance(scenario_id, str) or not scenario_id:
        raise ScenarioCoverageError(f"{source}.id must be a non-empty string")
    raw_outputs = root.get("outputs")
    if not isinstance(raw_outputs, list):
        raise ScenarioCoverageError(f"{source}.outputs must be an array")

    enabled: list[tuple[str, int, int, float, str]] = []
    for index, raw_output in enumerate(raw_outputs):
        output = _mapping(raw_output, f"{source}.outputs[{index}]")
        if output.get("enabled") is not True:
            continue
        mode = _mapping(output.get("mode"), f"{source}.outputs[{index}].mode")
        enabled.append(
            (
                str(output.get("name", f"output-{index}")),
                _positive_int(mode.get("width"), f"{source}.outputs[{index}].mode.width"),
                _positive_int(mode.get("height"), f"{source}.outputs[{index}].mode.height"),
                _positive_number(output.get("scale"), f"{source}.outputs[{index}].scale"),
                str(output.get("transform", "")),
            )
        )
    if not enabled:
        raise ScenarioCoverageError(f"{source} has no initially enabled outputs")

    common_states = {(width, height, scale) for _, width, height, scale, _ in enabled}
    if len(common_states) != 1:
        states = ", ".join(
            f"{name}={width}x{height}@{scale:g}"
            for name, width, height, scale, _ in enabled
        )
        raise ScenarioCoverageError(
            f"{source} has heterogeneous enabled output modes/scales ({states}); "
            "qindaqt-wm currently accepts only one common width, height, and scale"
        )
    transformed = [name for name, *_, transform in enabled if transform != "normal"]
    if transformed:
        raise ScenarioCoverageError(
            f"{source} requests transforms for {', '.join(transformed)}; "
            "qindaqt-wm's current virtual CLI cannot apply output transforms"
        )

    pixel_width, pixel_height, scale = next(iter(common_states))
    return VirtualOutputSpec(
        scenario_id=scenario_id,
        output_count=len(enabled),
        pixel_width=pixel_width,
        pixel_height=pixel_height,
        logical_width=_logical_extent(pixel_width, scale, f"{source}.common width"),
        logical_height=_logical_extent(pixel_height, scale, f"{source}.common height"),
        scale=scale,
    )


def load_virtual_spec(path: Path) -> VirtualOutputSpec:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ScenarioCoverageError(f"{path}: {error}") from error
    return virtual_spec_from_document(document, str(path))


def isolated_environment(root: Path) -> dict[str, str]:
    environment = dict(os.environ)
    for key in (
        "DBUS_SESSION_BUS_ADDRESS",
        "DISPLAY",
        "WAYLAND_DISPLAY",
        "QINDAQT_DEVELOPMENT_CONTROL",
        "QINDAQT_TEST_SCENARIO",
        "QINDAQT_EXPECT_COMPOSITOR_PLUGIN",
        "QINDAQT_EXPECT_COMPOSITOR_OUTPUTS",
        "QINDAQT_EXPECT_READ_ONLY_CONTROL",
    ):
        environment.pop(key, None)
    for name in ("home", "config", "data", "cache", "state"):
        (root / name).mkdir()
    runtime = root / "runtime"
    runtime.mkdir(mode=0o700)
    environment.update(
        {
            "HOME": str(root / "home"),
            "XDG_CONFIG_HOME": str(root / "config"),
            "XDG_DATA_HOME": str(root / "data"),
            "XDG_CACHE_HOME": str(root / "cache"),
            "XDG_STATE_HOME": str(root / "state"),
            "XDG_RUNTIME_DIR": str(runtime),
            "XDG_CURRENT_DESKTOP": "QindaQt",
            "XDG_SESSION_DESKTOP": "qindaqt",
            "KWIN_COMPOSE": "Q",
            "QT_QPA_PLATFORM": "wayland",
            "QT_QUICK_BACKEND": "software",
        }
    )
    return environment


def write_virtual_output_config(config_home: Path, spec: VirtualOutputSpec) -> None:
    """Pin KWin's startup normalization to the scenario's common scale."""
    outputs = []
    setup_outputs = []
    for index in range(spec.output_count):
        outputs.append(
            {
                "connectorName": f"Virtual-{index}",
                "mode": {
                    "width": spec.pixel_width,
                    "height": spec.pixel_height,
                    "refreshRate": 60000,
                },
                "scale": spec.scale,
                "transform": "Normal",
            }
        )
        setup_outputs.append(
            {
                "enabled": True,
                "outputIndex": index,
                # AGENT-NOTE: This is a generic bootstrap placement, not the
                # scenario's declared topology. The result reports positions
                # as unapplied until a compositor scenario adapter owns them.
                "position": {"x": index * spec.logical_width, "y": 0},
                "priority": index,
            }
        )
    document = [
        {"name": "outputs", "data": outputs},
        {"name": "setups", "data": [{"lidClosed": False, "outputs": setup_outputs}]},
    ]
    # AGENT-GUARD: KWin's output store otherwise replaces the CLI scale with 1
    # for virtual outputs, whose physical size is intentionally unknown. Keep
    # this file inside the disposable XDG tree and assert the live result.
    (config_home / "kwinoutputconfig.json").write_text(
        json.dumps(document, sort_keys=True), encoding="utf-8"
    )


def extract_probe_result(stdout: str) -> dict[str, Any]:
    marker = next(
        (
            line.removeprefix("QINDAQT_PROBE=")
            for line in stdout.splitlines()
            if line.startswith("QINDAQT_PROBE=")
        ),
        None,
    )
    if marker is None:
        raise RuntimeError("probe result marker was missing")
    result = json.loads(marker)
    if not isinstance(result, dict):
        raise RuntimeError("probe result must be a JSON object")
    return result


def _close(first: Any, second: float) -> bool:
    return isinstance(first, (int, float)) and not isinstance(first, bool) and math.isclose(
        float(first), second, rel_tol=0.0, abs_tol=1e-6
    )


def validate_probe_result(
    result: dict[str, Any],
    spec: VirtualOutputSpec,
    *,
    inspect_compositor_outputs: bool,
    expect_workflow: bool,
    expect_read_only: bool = False,
) -> None:
    if (
        result.get("platform") != "wayland"
        or result.get("xwaylandReachable") is not True
        or result.get("windowExposed") is not True
    ):
        raise RuntimeError("probe did not expose mapped Wayland windows plus XWayland")
    qt_outputs = result.get("outputs")
    if not isinstance(qt_outputs, list) or len(qt_outputs) != spec.output_count:
        raise RuntimeError(
            f"Qt observed {len(qt_outputs) if isinstance(qt_outputs, list) else 'invalid'} "
            f"outputs; expected {spec.output_count}"
        )
    if (
        not all(isinstance(output, dict) for output in qt_outputs)
        or any(
            output.get("width") != spec.logical_width
            or output.get("height") != spec.logical_height
            or not _close(output.get("bufferScale"), math.ceil(spec.scale))
            for output in qt_outputs
        )
    ):
        raise RuntimeError(
            "Qt output geometry/buffer scale did not match "
            f"{spec.logical_width}x{spec.logical_height}@{math.ceil(spec.scale)}"
        )

    if inspect_compositor_outputs:
        compositor_outputs = result.get("compositorOutputs")
        if (
            result.get("compositorService") is not True
            or result.get("compositorKWinAbi") != KWIN_ABI
            or result.get("compositorInputObserverActive") is not True
            or result.get("compositorInputConsumesEvents") is not False
            or not isinstance(result.get("compositorInputDevices"), list)
        ):
            raise RuntimeError("the release-matched compositor output/input endpoint was unavailable")
        if (
            not isinstance(compositor_outputs, list)
            or len(compositor_outputs) != spec.output_count
        ):
            raise RuntimeError(
                "compositor output inventory did not match the requested enabled output count"
            )
        for output in compositor_outputs:
            geometry = output.get("geometry") if isinstance(output, dict) else None
            if (
                not isinstance(geometry, dict)
                or not _close(geometry.get("width"), spec.logical_width)
                or not _close(geometry.get("height"), spec.logical_height)
                or not _close(output.get("scale"), spec.scale)
            ):
                raise RuntimeError(
                    "compositor output geometry/scale did not match "
                    f"{spec.logical_width}x{spec.logical_height}@{spec.scale:g}"
                )
    if expect_workflow and result.get("compositorWorkflow") is not True:
        raise RuntimeError("the compositor container workflow did not pass")
    if expect_workflow and (
        result.get("compositorControlMode") != "development-test"
        or result.get("compositorMutationsEnabled") is not True
    ):
        raise RuntimeError("the development workflow mutation gate was not enabled")
    if expect_read_only and (
        result.get("compositorWorkflow") is not True
        or result.get("compositorControlMode") != "read-only"
        or result.get("compositorMutationsEnabled") is not False
    ):
        raise RuntimeError("the production control endpoint was not proven read-only")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("launcher")
    parser.add_argument("probe")
    parser.add_argument("dbus_runner")
    parser.add_argument("scenario", type=Path)
    parser.add_argument("--plugin-root", type=Path, required=True)
    parser.add_argument("--inspect-compositor-outputs", action="store_true")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--expect-plugin", action="store_true")
    mode.add_argument("--expect-read-only", action="store_true")
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        spec = load_virtual_spec(arguments.scenario)
    except ScenarioCoverageError as error:
        print(f"scenario is not representable: {error}", file=sys.stderr)
        return 2
    inspect_outputs = (
        arguments.inspect_compositor_outputs
        or arguments.expect_plugin
        or arguments.expect_read_only
    )
    with tempfile.TemporaryDirectory(prefix="qindaqt-nested-test-") as directory:
        environment = isolated_environment(Path(directory))
        write_virtual_output_config(Path(environment["XDG_CONFIG_HOME"]), spec)
        if arguments.expect_plugin:
            environment["QINDAQT_EXPECT_COMPOSITOR_PLUGIN"] = "1"
        if arguments.expect_read_only:
            environment["QINDAQT_EXPECT_READ_ONLY_CONTROL"] = "1"
        if inspect_outputs:
            environment["QINDAQT_EXPECT_COMPOSITOR_OUTPUTS"] = "1"
        # AGENT-GUARD: Integration tests exercise build artifacts, while the
        # launcher's default is intentionally the installed KDE plugin root.
        # Never infer a build layout from the launcher path here.
        launcher_command = [
            arguments.launcher,
            "--plugin-root",
            str(arguments.plugin_root),
        ]
        launcher_command.extend(
            [
                "--virtual",
                "--width",
                str(spec.logical_width),
                "--height",
                str(spec.logical_height),
                "--scale",
                f"{spec.scale:.12g}",
                "--output-count",
                str(spec.output_count),
            ]
        )
        # AGENT-GUARD: This mode must not pass the launcher marker that enables
        # the development mutation surface. The same display document is used
        # only to derive deterministic virtual dimensions for validation.
        if not arguments.expect_read_only:
            launcher_command.extend(["--test-scenario", str(arguments.scenario)])
        launcher_command.extend(
            [
                "--no-lockscreen",
                "--no-global-shortcuts",
                "--session",
                arguments.probe,
            ]
        )
        completed = subprocess.run(
            [arguments.dbus_runner, "--", *launcher_command],
            env=environment,
            text=True,
            capture_output=True,
            timeout=20,
            check=False,
        )
    if completed.returncode != 0:
        print(completed.stdout, file=sys.stderr)
        print(completed.stderr, file=sys.stderr)
        return completed.returncode or 1
    try:
        result = extract_probe_result(completed.stdout)
        validate_probe_result(
            result,
            spec,
            inspect_compositor_outputs=inspect_outputs,
            expect_workflow=arguments.expect_plugin,
            expect_read_only=arguments.expect_read_only,
        )
    except (RuntimeError, json.JSONDecodeError) as error:
        print(f"nested session validation failed: {error}", file=sys.stderr)
        print(completed.stdout, file=sys.stderr)
        return 1
    result["scenarioCoverage"] = spec.coverage
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
