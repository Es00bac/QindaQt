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


def validate_secondary_output_authority(
    snapshot: Any,
    *,
    previous_outputs: Any,
    previous_generation: Any,
) -> dict[str, Any]:
    """Validate the exact public Outputs transition from WL-0 to WL-1."""

    authority = _mapping(snapshot, "postSelectorOutputs")
    if set(authority) != {
        "schemaVersion", "status", "outputGeneration", "outputs"
    } or authority.get("schemaVersion") != 1 or authority.get("status") != "ok":
        raise OutputInventoryError("post-selector Outputs envelope is malformed")
    before = _sequence(previous_outputs, "evidence.outputs")
    after = _sequence(authority.get("outputs"), "postSelectorOutputs.outputs")
    if len(before) != 2 or len(after) != 2:
        raise OutputInventoryError("secondary authority requires exactly two outputs")

    before_records = [
        _mapping(record, f"evidence.outputs[{index}]")
        for index, record in enumerate(before)
    ]
    after_records = [
        _mapping(record, f"postSelectorOutputs.outputs[{index}]")
        for index, record in enumerate(after)
    ]
    before_names = tuple(
        _canonical_name(record.get("name"), f"evidence.outputs[{index}]", "wayland")
        for index, record in enumerate(before_records)
    )
    after_names = tuple(
        _canonical_name(
            record.get("name"), f"postSelectorOutputs.outputs[{index}]", "wayland"
        )
        for index, record in enumerate(after_records)
    )
    if before_names != ("WL-0", "WL-1") or after_names != ("WL-1", "WL-0"):
        raise OutputInventoryError(
            "post-selector output authority is not ordered WL-1 then WL-0"
        )
    priorities = tuple(record.get("priority") for record in after_records)
    if (
        any(isinstance(priority, bool) or not isinstance(priority, int)
            for priority in priorities)
        or priorities != (1, 2)
    ):
        raise OutputInventoryError("post-selector priorities are not primary then secondary")

    def generation(value: Any, location: str) -> int:
        if (
            not isinstance(value, str)
            or not value.isascii()
            or not value.isdecimal()
            or value != str(int(value, 10))
        ):
            raise OutputInventoryError(f"{location} must be a canonical generation")
        return int(value, 10)

    before_generation = generation(previous_generation, "generations.outputs")
    after_generation = generation(
        authority.get("outputGeneration"), "postSelectorOutputs.outputGeneration"
    )
    if after_generation <= before_generation:
        raise OutputInventoryError("post-selector output generation did not advance")

    before_by_name = {record["name"]: record for record in before_records}
    for record in after_records:
        prior = dict(before_by_name[record["name"]])
        current = dict(record)
        prior.pop("priority", None)
        current.pop("priority", None)
        if current != prior:
            raise OutputInventoryError(
                "post-selector output topology changed beyond ordered authority"
            )
    return dict(authority)
