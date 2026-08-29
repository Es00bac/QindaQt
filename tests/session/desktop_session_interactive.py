# SPDX-License-Identifier: GPL-3.0-or-later
"""Additive S2 containment, private-seat interaction, and capture evidence."""

from __future__ import annotations

import re
from typing import Any, Mapping

from desktop_session_topology import (
    DesktopTopology,
    MatrixBootTopology,
    TopologyContractError,
    interactive_1080p_topology,
    validate_boot_evidence,
)
from desktop_session_matrix import physical_content_region


def _mapping(value: Any, location: str) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        raise TopologyContractError(f"{location} must be an object")
    return value


def _canonical_process_id(value: Any) -> int:
    process_id = (
        int(value, 10)
        if isinstance(value, str) and value.isascii() and value.isdecimal()
        else 0
    )
    if process_id <= 0 or value != str(process_id):
        raise TopologyContractError("surface processId must be canonical")
    return process_id


def _canonical_digest(value: Any) -> bool:
    return (
        isinstance(value, str)
        and len(value) == 64
        and all(character in "0123456789abcdef" for character in value)
    )


def _validate_matrix_captures(
    evidence: Mapping[str, Any], topology: MatrixBootTopology,
    surface: Mapping[str, Any], geometry: Mapping[str, Any],
) -> None:
    presentation = _mapping(
        evidence.get("matrixPresentation"), "evidence.matrixPresentation"
    )
    expected = topology.presentation
    if presentation != {
        "scenarioId": expected.scenario_id,
        "profileId": expected.profile_id,
        "themeId": expected.theme_id,
        "requestedScale": expected.requested_scale,
        "parentArguments": (
            [
                "--virtual", "--width", str(topology.outputs[0].logical_width),
                "--height", str(topology.outputs[0].logical_height),
                "--scale", str(expected.requested_scale), "--output-count", "1",
                "--socket", "qindaqt-parent-wayland", "--no-lockscreen",
                "--no-global-shortcuts",
            ]
            if expected.requested_scale != 1.0 else None
        ),
        "sessionArguments": [
            "--profile", expected.profile_id, "--theme", expected.theme_id
        ],
        "editorArguments": ["--theme", expected.theme_id],
    }:
        raise TopologyContractError("matrix presentation selection is malformed")
    captures = evidence.get("matrixCaptures")
    if not isinstance(captures, list) or len(captures) != 1:
        raise TopologyContractError("matrix capture cardinality is malformed")
    by_name: dict[str, Mapping[str, Any]] = {}
    for index, raw in enumerate(captures):
        capture = _mapping(raw, f"matrixCaptures[{index}]")
        name = capture.get("outputName")
        if not isinstance(name, str) or name in by_name:
            raise TopologyContractError("matrix capture output names must be unique")
        by_name[name] = capture
    interacted_name = surface.get("outputName")
    if set(by_name) != {interacted_name}:
        raise TopologyContractError("matrix capture does not bind the interacted output")
    outputs_by_name = {
        f"WL-{output.ordinal}": output for output in topology.outputs
    }
    for output_name, capture in by_name.items():
        output = outputs_by_name[output_name]
        expected_keys = {
            "tool", "path", "sha256", "byteCount", "width", "height",
            "sampledDistinctColors", "outputName",
        }
        expected_keys.add("contentRegion")
        if (
            set(capture) != expected_keys
            or capture.get("tool")
            != (
                "kwin-virtual-shm"
                if output.render_scale != 1.0 else "weston-screenshooter"
            )
            or capture.get("path")
            != f"desktop-matrix-{expected.scenario_id}-output-{output.ordinal}.png"
            or not _canonical_digest(capture.get("sha256"))
            or isinstance(capture.get("byteCount"), bool)
            or not isinstance(capture.get("byteCount"), int)
            or capture["byteCount"] <= 1024
            or capture.get("width") != output.pixel_width
            or capture.get("height") != output.pixel_height
            or isinstance(capture.get("sampledDistinctColors"), bool)
            or not isinstance(capture.get("sampledDistinctColors"), int)
            or capture["sampledDistinctColors"] < 16
        ):
            raise TopologyContractError("matrix captured desktop evidence is malformed")
        region = _mapping(capture.get("contentRegion"), "capture.contentRegion")
        physical = physical_content_region(
            geometry, output, physical_scale=output.render_scale
        )
        if (
            set(region) != {
                "x", "y", "width", "height", "sampledDistinctColors", "sha256"
            }
            or any(region.get(key) != value for key, value in physical.items())
            or isinstance(region.get("sampledDistinctColors"), bool)
            or not isinstance(region.get("sampledDistinctColors"), int)
            or region["sampledDistinctColors"] < 16
            or not _canonical_digest(region.get("sha256"))
        ):
            raise TopologyContractError(
                "matrix capture is not bound to the interacted physical region"
            )


def validate_interactive_evidence(
    document: Any, topology: DesktopTopology | None = None
) -> None:
    """Validate the additive S2 private-seat and captured-frame evidence."""

    topology = topology or interactive_1080p_topology()
    validate_boot_evidence(document, topology)
    evidence = _mapping(document, "evidence")
    processes = _mapping(evidence.get("processes"), "evidence.processes")
    shell = _mapping(processes.get("shell"), "processes.shell")
    outputs = evidence["outputs"]
    output_names = {
        item.get("name") for item in outputs if isinstance(item, Mapping)
    }

    containment = _mapping(evidence.get("containment"), "evidence.containment")
    parent_socket = containment.get("parentWaylandSocket")
    child_socket = containment.get("childWaylandSocket")
    expected_parent = (
        "kwin-virtual-qpaint"
        if isinstance(topology, MatrixBootTopology)
        and any(output.render_scale != 1.0 for output in topology.outputs)
        else "weston-headless-pixman"
    )
    if (
        containment.get("parentBackend") != expected_parent
        or containment.get("qindaqtBackend") != "kwin-windowed-qpaint"
        or parent_socket != "qindaqt-parent-wayland"
        or not isinstance(child_socket, str)
        or re.fullmatch(r"qindaqt-[0-9a-f]{12}", child_socket) is None
        or child_socket == parent_socket
    ):
        raise TopologyContractError("interactive containment endpoints are malformed")

    interaction = _mapping(evidence.get("interaction"), "evidence.interaction")
    if set(interaction) != {
        "action", "deviceId", "eventCount", "preInjectionActiveSurfaceCount", "surface",
    }:
        raise TopologyContractError("interaction has an unexpected field set")
    surface = _mapping(interaction.get("surface"), "interaction.surface")
    geometry = _mapping(surface.get("geometry"), "interaction.surface.geometry")
    secondary_output = (
        isinstance(topology, MatrixBootTopology) and len(topology.outputs) == 2
    )
    expected_interaction_output = "WL-1" if secondary_output else "WL-0"
    expected_event_count = 5 if secondary_output else 4
    if (
        interaction.get("action") != "open-notification-center"
        or interaction.get("deviceId") != "qindaqt-development-input"
        or interaction.get("eventCount") != expected_event_count
        or interaction.get("preInjectionActiveSurfaceCount") != 0
        or surface.get("scope") != "notification-center"
        or surface.get("mapped") is not True
        or surface.get("committed") is not True
        or surface.get("active") is not True
        or geometry.get("width") != 440
        or geometry.get("height") != 640
        or _canonical_process_id(surface.get("processId")) != shell.get("pid")
        or surface.get("outputName") != expected_interaction_output
        or surface.get("desiredOutputName") != surface.get("outputName")
    ):
        raise TopologyContractError("private-seat interaction evidence is malformed")

    if isinstance(topology, MatrixBootTopology):
        _validate_matrix_captures(evidence, topology, surface, geometry)
        return

    capture = _mapping(evidence.get("capture"), "evidence.capture")
    digest = capture.get("sha256")
    region = _mapping(capture.get("contentRegion"), "capture.contentRegion")
    region_digest = region.get("sha256")
    if (
        set(capture) != {
            "tool", "path", "sha256", "byteCount", "width", "height",
            "sampledDistinctColors", "contentRegion",
        }
        or capture.get("tool") != "weston-screenshooter"
        or capture.get("path") != "desktop-1080p.png"
        or not _canonical_digest(digest)
        or isinstance(capture.get("byteCount"), bool)
        or not isinstance(capture.get("byteCount"), int)
        or capture["byteCount"] <= 1024
        or capture.get("width") != 1920 or capture.get("height") != 1080
        or isinstance(capture.get("sampledDistinctColors"), bool)
        or not isinstance(capture.get("sampledDistinctColors"), int)
        or capture["sampledDistinctColors"] < 16
        or set(region) != {
            "x", "y", "width", "height", "sampledDistinctColors", "sha256",
        }
        or any(region.get(key) != geometry.get(key) for key in ("x", "y", "width", "height"))
        or isinstance(region.get("sampledDistinctColors"), bool)
        or not isinstance(region.get("sampledDistinctColors"), int)
        or region["sampledDistinctColors"] < 16
        or not _canonical_digest(region_digest)
    ):
        raise TopologyContractError("captured desktop evidence is malformed")
