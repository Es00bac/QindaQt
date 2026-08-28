# SPDX-License-Identifier: GPL-3.0-or-later
"""Canonical KWin virtual-output inventory validation for desktop evidence."""

from __future__ import annotations

import re
from typing import Any, Mapping, Sequence


class OutputInventoryError(ValueError):
    """Outputs and ShellVisibility do not describe one exact virtual output."""


_KWIN_VIRTUAL_OUTPUT_NAME = re.compile(r"Virtual-(?:0|[1-9][0-9]*)\Z")
_MAX_OUTPUT_NAME_CHARACTERS = 512


def _mapping(value: Any, location: str) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        raise OutputInventoryError(f"{location} must be an object")
    return value


def _sequence(value: Any, location: str) -> Sequence[Any]:
    if isinstance(value, (str, bytes)) or not isinstance(value, Sequence):
        raise OutputInventoryError(f"{location} must be an array")
    return value


def _canonical_name(value: Any, location: str) -> str:
    # AGENT-CONTRACT: KWin's virtual backend publishes CLI-created connectors
    # as Virtual-<zero-based decimal index>. The exact ordinal is runtime
    # inventory, not a stable test fixture; every other public view must bind
    # to the one name derived here.
    if (
        not isinstance(value, str)
        or len(value) > _MAX_OUTPUT_NAME_CHARACTERS
        or _KWIN_VIRTUAL_OUTPUT_NAME.fullmatch(value) is None
    ):
        raise OutputInventoryError(
            f"{location} must be a canonical KWin virtual output name"
        )
    return value


def _exact_geometry(
    record: Mapping[str, Any], *, name_field: str, location: str,
    width: int, height: int, scale: float,
) -> str:
    output_name = _canonical_name(record.get(name_field), location)
    geometry = _mapping(record.get("geometry"), f"{location}.geometry")
    geometry_values = tuple(
        geometry.get(field) for field in ("x", "y", "width", "height")
    )
    observed_scale = record.get("scale")
    if (
        any(isinstance(value, bool) or not isinstance(value, (int, float))
            for value in geometry_values)
        or isinstance(observed_scale, bool)
        or not isinstance(observed_scale, (int, float))
        or geometry.get("x") != 0
        or geometry.get("y") != 0
        or geometry.get("width") != width
        or geometry.get("height") != height
        or observed_scale != scale
    ):
        raise OutputInventoryError("the output is not exact 1920x1080@1")
    return output_name


def validate_output_inventory(
    evidence: Mapping[str, Any], *, width: int, height: int, scale: float,
) -> str:
    """Return the one canonical output name after both public inventories agree."""

    outputs = _sequence(evidence.get("outputs"), "evidence.outputs")
    if len(outputs) != 1:
        raise OutputInventoryError("S1 requires exactly one output")
    output_name = _exact_geometry(
        _mapping(outputs[0], "outputs[0]"), name_field="name",
        location="outputs[0]", width=width, height=height, scale=scale,
    )
    visibility_outputs = _sequence(
        evidence.get("visibilityOutputs"), "evidence.visibilityOutputs"
    )
    if len(visibility_outputs) != 1:
        raise OutputInventoryError("ShellVisibility requires exactly one output")
    visibility_name = _exact_geometry(
        _mapping(visibility_outputs[0], "visibilityOutputs[0]"), name_field="id",
        location="visibilityOutputs[0]", width=width, height=height, scale=scale,
    )
    if visibility_name != output_name:
        raise OutputInventoryError(
            "Outputs and ShellVisibility identify different outputs"
        )
    return output_name
