# SPDX-License-Identifier: GPL-3.0-or-later
"""Additive S2 containment, private-seat interaction, and capture evidence."""

from __future__ import annotations

import re
from typing import Any, Mapping

from desktop_session_topology import (
    TopologyContractError,
    interactive_1080p_topology,
    validate_boot_evidence,
)


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


def validate_interactive_evidence(document: Any) -> None:
    """Validate the additive S2 private-seat and captured-frame evidence."""

    topology = interactive_1080p_topology()
    validate_boot_evidence(document, topology)
    evidence = _mapping(document, "evidence")
    processes = _mapping(evidence.get("processes"), "evidence.processes")
    shell = _mapping(processes.get("shell"), "processes.shell")
    outputs = evidence["outputs"]
    output_name = outputs[0]["name"]

    containment = _mapping(evidence.get("containment"), "evidence.containment")
    parent_socket = containment.get("parentWaylandSocket")
    child_socket = containment.get("childWaylandSocket")
    if (
        containment.get("parentBackend") != "weston-headless-pixman"
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
    if (
        interaction.get("action") != "open-notification-center"
        or interaction.get("deviceId") != "qindaqt-development-input"
        or interaction.get("eventCount") != 4
        or interaction.get("preInjectionActiveSurfaceCount") != 0
        or surface.get("scope") != "notification-center"
        or surface.get("mapped") is not True
        or surface.get("committed") is not True
        or surface.get("active") is not True
        or geometry.get("width") != 440
        or geometry.get("height") != 640
        or _canonical_process_id(surface.get("processId")) != shell.get("pid")
        or surface.get("outputName") != output_name
        or surface.get("desiredOutputName") != output_name
    ):
        raise TopologyContractError("private-seat interaction evidence is malformed")

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
        or not isinstance(digest, str) or len(digest) != 64
        or any(character not in "0123456789abcdef" for character in digest)
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
        or not isinstance(region_digest, str) or len(region_digest) != 64
        or any(character not in "0123456789abcdef" for character in region_digest)
    ):
        raise TopologyContractError("captured desktop evidence is malformed")
