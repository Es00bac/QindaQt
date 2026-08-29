# SPDX-License-Identifier: GPL-3.0-or-later
"""Typed, executable rows for the contained interactive desktop matrix."""

from __future__ import annotations

import json
import math
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Mapping

from nested_session_scenario import ScenarioCoverageError, VirtualOutputSpec, load_virtual_spec


class DesktopMatrixError(ValueError):
    """A selected matrix row cannot be represented or applied exactly."""


# These rows deliberately maximize acceptance breadth without pretending to be
# the complete release matrix. Together with the accepted S2 1080p/dusk row,
# they cover WUXGA, 1440p, 125% and 150%, all three standard variants, and a
# real two-output arrangement.
EXECUTABLE_MATRIX_ROWS = (
    "single-wuxga",
    "single-1440p-125",
    "single-1080p-150",
    "dual-1080p-horizontal",
)

_SAFE_CATALOG_ID = re.compile(r"[a-z0-9]+(?:-[a-z0-9]+)*\Z")


@dataclass(frozen=True)
class MatrixOutput:
    ordinal: int
    pixel_width: int
    pixel_height: int
    logical_x: int
    logical_y: int
    logical_width: int
    logical_height: int
    scale: float


@dataclass(frozen=True)
class DesktopMatrixScenario:
    scenario_id: str
    profile_id: str
    theme_id: str
    virtual: VirtualOutputSpec
    outputs: tuple[MatrixOutput, ...]


def _mapping(value: Any, location: str) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        raise DesktopMatrixError(f"{location} must be an object")
    return value


def _safe_catalog_id(value: Any, location: str) -> str:
    if not isinstance(value, str) or _SAFE_CATALOG_ID.fullmatch(value) is None:
        raise DesktopMatrixError(f"{location} must be a canonical catalog id")
    return value


def load_matrix_scenario(source_root: Path, scenario_id: str) -> DesktopMatrixScenario:
    """Load one approved row and reject any declaration the runtime cannot apply.

    The path is selected from a closed row set rather than caller-controlled
    input. This keeps the read-only source bind from becoming a generic file
    oracle inside the qualification sandbox.
    """

    if scenario_id not in EXECUTABLE_MATRIX_ROWS:
        raise DesktopMatrixError(f"desktop matrix row is not approved: {scenario_id!r}")
    scenario_path = source_root / "tests" / "scenarios" / f"{scenario_id}.json"
    try:
        document = _mapping(
            json.loads(scenario_path.read_text(encoding="utf-8")), str(scenario_path)
        )
        virtual = load_virtual_spec(scenario_path)
    except (OSError, json.JSONDecodeError, ScenarioCoverageError) as error:
        raise DesktopMatrixError(str(error)) from error
    if virtual.scenario_id != scenario_id:
        raise DesktopMatrixError("matrix filename and declared scenario id disagree")
    profile_id = _safe_catalog_id(document.get("profile"), "scenario.profile")
    theme_id = _safe_catalog_id(document.get("theme"), "scenario.theme")
    raw_outputs = document.get("outputs")
    if not isinstance(raw_outputs, list):
        raise DesktopMatrixError("scenario.outputs must be an array")
    enabled = [
        _mapping(output, f"scenario.outputs[{index}]")
        for index, output in enumerate(raw_outputs)
        if isinstance(output, Mapping) and output.get("enabled") is True
    ]
    if len(enabled) != virtual.output_count:
        raise DesktopMatrixError("enabled output count changed during matrix parsing")

    outputs: list[MatrixOutput] = []
    for ordinal, output in enumerate(enabled):
        position = _mapping(output.get("position"), f"enabled[{ordinal}].position")
        expected_x = ordinal * virtual.logical_width
        if position.get("x") != expected_x or position.get("y") != 0:
            raise DesktopMatrixError(
                "approved matrix rows must use the exactly applied horizontal arrangement"
            )
        outputs.append(
            MatrixOutput(
                ordinal=ordinal,
                pixel_width=virtual.pixel_width,
                pixel_height=virtual.pixel_height,
                logical_x=expected_x,
                logical_y=0,
                logical_width=virtual.logical_width,
                logical_height=virtual.logical_height,
                scale=virtual.scale,
            )
        )
    return DesktopMatrixScenario(
        scenario_id=scenario_id,
        profile_id=profile_id,
        theme_id=theme_id,
        virtual=virtual,
        outputs=tuple(outputs),
    )


def physical_content_region(
    logical: Mapping[str, Any], output: MatrixOutput, *, physical_scale: float | None = None
) -> dict[str, int]:
    """Map compositor-global logical geometry to one parent framebuffer.

    KWin publishes integer logical geometry even when an edge lands between
    physical pixels. The proof samples the smallest outward-rounded physical
    rectangle containing that complete logical surface; at most one boundary
    pixel of compositor-owned fringe is admitted on each axis.
    """

    keys = ("x", "y", "width", "height")
    if set(logical) != set(keys) or any(
        isinstance(logical.get(key), bool) or not isinstance(logical.get(key), int)
        for key in keys
    ):
        raise DesktopMatrixError("interactive surface geometry is malformed")
    local = {
        "x": logical["x"] - output.logical_x,
        "y": logical["y"] - output.logical_y,
        "width": logical["width"],
        "height": logical["height"],
    }
    if (
        local["x"] < 0
        or local["y"] < 0
        or local["width"] <= 0
        or local["height"] <= 0
        or local["x"] + local["width"] > output.logical_width
        or local["y"] + local["height"] > output.logical_height
    ):
        raise DesktopMatrixError("interactive surface escapes its declared output")
    scale = output.scale if physical_scale is None else physical_scale
    left = math.floor(local["x"] * scale)
    top = math.floor(local["y"] * scale)
    right = math.ceil((local["x"] + local["width"]) * scale)
    bottom = math.ceil((local["y"] + local["height"]) * scale)
    return {"x": left, "y": top, "width": right - left, "height": bottom - top}
