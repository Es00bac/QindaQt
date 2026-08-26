# SPDX-License-Identifier: GPL-3.0-or-later
"""Translate display scenarios into isolated, exactly representable KWin inputs."""

from __future__ import annotations

import json
import math
import os
from dataclasses import dataclass
from pathlib import Path
from typing import Any


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
    """Create the process environment for one hermetic nested KWin run."""
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
        "QINDAQT_EXPECT_HYBRID_POINTER",
        "QINDAQT_DOTOOL",
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
