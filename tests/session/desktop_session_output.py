# SPDX-License-Identifier: GPL-3.0-or-later
"""Canonical KWin virtual-output inventory validation for desktop evidence."""

from __future__ import annotations

import re
from typing import Any, Mapping, Sequence


class OutputInventoryError(ValueError):
    """Outputs and ShellVisibility do not describe one exact virtual output."""


_KWIN_VIRTUAL_OUTPUT_NAME = re.compile(r"Virtual-(?:0|[1-9][0-9]*)\Z")
_KWIN_WAYLAND_OUTPUT_NAME = re.compile(r"WL-(?:0|[1-9][0-9]*)\Z")
_MAX_OUTPUT_NAME_CHARACTERS = 512


def _mapping(value: Any, location: str) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        raise OutputInventoryError(f"{location} must be an object")
    return value


def _sequence(value: Any, location: str) -> Sequence[Any]:
    if isinstance(value, (str, bytes)) or not isinstance(value, Sequence):
        raise OutputInventoryError(f"{location} must be an array")
    return value


def _canonical_name(value: Any, location: str, backend: str) -> str:
    # AGENT-CONTRACT: KWin's virtual backend publishes CLI-created connectors
    # as Virtual-<zero-based decimal index>. The exact ordinal is runtime
    # inventory, not a stable test fixture; every other public view must bind
    # to the one name derived here.
    patterns = {
        "virtual": _KWIN_VIRTUAL_OUTPUT_NAME,
        "wayland": _KWIN_WAYLAND_OUTPUT_NAME,
    }
    pattern = patterns.get(backend)
    if pattern is None:
        raise OutputInventoryError("output backend contract is unknown")
    if (
        not isinstance(value, str)
        or len(value) > _MAX_OUTPUT_NAME_CHARACTERS
        or pattern.fullmatch(value) is None
    ):
        raise OutputInventoryError(
            f"{location} must be a canonical KWin {backend} output name"
        )
    return value


def _exact_geometry(
    record: Mapping[str, Any], *, name_field: str, location: str,
    width: int, height: int, scale: float, backend: str,
) -> str:
    output_name = _canonical_name(record.get(name_field), location, backend)
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
        raise OutputInventoryError(
            f"the output is not exact ({width}x{height})@{scale:g} at (0,0)"
        )
    return output_name


def validate_output_inventory(
    evidence: Mapping[str, Any], *, width: int, height: int, scale: float,
    backend: str = "virtual",
) -> str:
    """Return the one canonical output name after both public inventories agree."""

    outputs = _sequence(evidence.get("outputs"), "evidence.outputs")
    if len(outputs) != 1:
        raise OutputInventoryError("S1 requires exactly one output")
    output_name = _exact_geometry(
        _mapping(outputs[0], "outputs[0]"), name_field="name",
        location="outputs[0]", width=width, height=height, scale=scale,
        backend=backend,
    )
    visibility_outputs = _sequence(
        evidence.get("visibilityOutputs"), "evidence.visibilityOutputs"
    )
    if len(visibility_outputs) != 1:
        raise OutputInventoryError("ShellVisibility requires exactly one output")
    visibility_name = _exact_geometry(
        _mapping(visibility_outputs[0], "visibilityOutputs[0]"), name_field="id",
        location="visibilityOutputs[0]", width=width, height=height, scale=scale,
        backend=backend,
    )
    if visibility_name != output_name:
        raise OutputInventoryError(
            "Outputs and ShellVisibility identify different outputs"
        )
    return output_name


def _matrix_geometry(
    record: Mapping[str, Any], *, name_field: str, location: str,
    expectation: Any, backend: str,
) -> str:
    output_name = _canonical_name(record.get(name_field), location, backend)
    geometry = _mapping(record.get("geometry"), f"{location}.geometry")
    expected_geometry = (
        expectation.logical_x,
        expectation.logical_y,
        expectation.logical_width,
        expectation.logical_height,
    )
    actual_geometry = tuple(
        geometry.get(field) for field in ("x", "y", "width", "height")
    )
    if (
        any(isinstance(value, bool) or not isinstance(value, (int, float))
            for value in actual_geometry)
        or actual_geometry != expected_geometry
        or record.get("scale") != expectation.scale
    ):
        raise OutputInventoryError(
            f"{location} does not match the applied matrix geometry"
        )
    return output_name


def validate_matrix_output_inventory(
    evidence: Mapping[str, Any], *, expectations: Sequence[Any],
    backend: str = "wayland",
) -> tuple[str, ...]:
    """Bind every matrix output across Outputs and ShellVisibility."""

    if not expectations:
        raise OutputInventoryError("the matrix must declare at least one output")
    outputs = _sequence(evidence.get("outputs"), "evidence.outputs")
    visibility = _sequence(
        evidence.get("visibilityOutputs"), "evidence.visibilityOutputs"
    )
    if len(outputs) != len(expectations) or len(visibility) != len(expectations):
        raise OutputInventoryError("matrix output inventories have the wrong cardinality")

    expected_names = tuple(f"WL-{index}" for index in range(len(expectations)))
    output_by_name: dict[str, Mapping[str, Any]] = {}
    visibility_by_name: dict[str, Mapping[str, Any]] = {}
    for index, raw in enumerate(outputs):
        record = _mapping(raw, f"outputs[{index}]")
        name = _canonical_name(record.get("name"), f"outputs[{index}]", backend)
        if name in output_by_name:
            raise OutputInventoryError("matrix output names must be unique")
        output_by_name[name] = record
    for index, raw in enumerate(visibility):
        record = _mapping(raw, f"visibilityOutputs[{index}]")
        name = _canonical_name(
            record.get("id"), f"visibilityOutputs[{index}]", backend
        )
        if name in visibility_by_name:
            raise OutputInventoryError("matrix visibility names must be unique")
        visibility_by_name[name] = record
    if set(output_by_name) != set(expected_names) or set(visibility_by_name) != set(
        expected_names
    ):
        raise OutputInventoryError("matrix inventories do not expose exact WL ordinals")
    for name, expectation in zip(expected_names, expectations, strict=True):
        if _matrix_geometry(
            output_by_name[name], name_field="name", location=f"outputs.{name}",
            expectation=expectation, backend=backend,
        ) != name or _matrix_geometry(
            visibility_by_name[name], name_field="id",
            location=f"visibilityOutputs.{name}", expectation=expectation,
            backend=backend,
        ) != name:
            raise OutputInventoryError("matrix public output identities disagree")
    return expected_names
