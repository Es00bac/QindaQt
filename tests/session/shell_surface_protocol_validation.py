# SPDX-License-Identifier: GPL-3.0-or-later
"""Validate committed layer-shell mapping evidence from the nested probe."""

from __future__ import annotations

from typing import Any, Protocol


EXPECTED_TOP_ZONE = 30
EXPECTED_BOTTOM_ZONE = 54
EXPECTED_RESERVATION = EXPECTED_TOP_ZONE + EXPECTED_BOTTOM_ZONE
EXPECTED_SURFACE_COUNT = 2
EXPECTED_SCOPE = "dock"
LAYER_TOP = 2
ANCHOR_TOP = 1
ANCHOR_BOTTOM = 2
ANCHOR_LEFT = 4
ANCHOR_RIGHT = 8
TOP_ANCHORS = ANCHOR_TOP | ANCHOR_LEFT | ANCHOR_RIGHT
BOTTOM_ANCHORS = ANCHOR_BOTTOM | ANCHOR_LEFT


class LogicalOutputSpec(Protocol):
    logical_width: int


def _object(value: Any, location: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise RuntimeError(f"{location} must be a JSON object")
    return value


def _object_list(value: Any, location: str) -> list[dict[str, Any]]:
    if not isinstance(value, list):
        raise RuntimeError(f"{location} must be an array")
    return [_object(item, f"{location}[{index}]") for index, item in enumerate(value)]


def _integer(value: Any, location: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool):
        raise RuntimeError(f"{location} must be an integer")
    return value


def _object_id(value: Any, location: str) -> str:
    if (
        not isinstance(value, str)
        or not value.isdecimal()
        or not 0 < int(value) <= 0xFFFFFFFF
    ):
        raise RuntimeError(f"{location} must be a non-zero decimal Wayland object ID")
    return value


def _serial(value: Any, location: str) -> str:
    if (
        not isinstance(value, str)
        or not value.isdecimal()
        or int(value) > 0xFFFFFFFF
    ):
        raise RuntimeError(f"{location} must be an unsigned 32-bit Wayland serial")
    return value


def _size(value: Any, location: str) -> tuple[int, int]:
    size = _object(value, location)
    return (
        _integer(size.get("width"), f"{location}.width"),
        _integer(size.get("height"), f"{location}.height"),
    )


def _boolean(value: Any, location: str) -> bool:
    if not isinstance(value, bool):
        raise RuntimeError(f"{location} must be a boolean")
    return value


def _role_state(value: Any, location: str) -> dict[str, Any]:
    state = _object(value, location)
    return {
        "layer": _integer(state.get("layer"), f"{location}.layer"),
        "anchors": _integer(state.get("anchors"), f"{location}.anchors"),
        "exclusiveEdge": _integer(
            state.get("exclusiveEdge"), f"{location}.exclusiveEdge"
        ),
        "exclusiveZone": _integer(
            state.get("exclusiveZone"), f"{location}.exclusiveZone"
        ),
        "desiredSize": _size(state.get("desiredSize"), f"{location}.desiredSize"),
    }


def _configurations(
    surface: dict[str, Any], location: str
) -> dict[str, dict[str, Any]]:
    values = _object_list(surface.get("configurations"), f"{location}.configurations")
    result: dict[str, dict[str, Any]] = {}
    for index, value in enumerate(values):
        item_location = f"{location}.configurations[{index}]"
        serial = _serial(value.get("serial"), f"{item_location}.serial")
        if serial in result:
            raise RuntimeError(f"{location} repeated configure serial {serial}")
        acknowledge_order = value.get("acknowledgeOrder")
        if acknowledge_order is not None:
            acknowledge_order = _integer(
                acknowledge_order, f"{item_location}.acknowledgeOrder"
            )
        result[serial] = {
            "size": (
                _integer(value.get("width"), f"{item_location}.width"),
                _integer(value.get("height"), f"{item_location}.height"),
            ),
            "committedEpoch": _integer(
                value.get("committedEpoch"), f"{item_location}.committedEpoch"
            ),
            "committedState": _role_state(
                value.get("committedState"), f"{item_location}.committedState"
            ),
            "configureOrder": _integer(
                value.get("configureOrder"), f"{item_location}.configureOrder"
            ),
            "acknowledgeOrder": acknowledge_order,
        }
    return result


def _require_expected_state(
    state: dict[str, Any], contract: dict[str, Any], location: str
) -> None:
    for field in ("layer", "anchors", "exclusiveEdge", "exclusiveZone", "desiredSize"):
        if state[field] != contract[field]:
            raise RuntimeError(
                f"{location}.{field} was {state[field]!r}, expected {contract[field]!r}"
            )


def _protocol_surfaces(protocol: dict[str, Any], location: str) -> list[dict[str, Any]]:
    for flag in ("inputTruncated", "identityAmbiguous", "protocolAmbiguous"):
        if _boolean(protocol.get(flag), f"{location}.{flag}"):
            raise RuntimeError(f"{location} is unusable because {flag} is true")
    surfaces = _object_list(protocol.get("surfaces"), f"{location}.surfaces")
    if len(surfaces) != EXPECTED_SURFACE_COUNT:
        raise RuntimeError(f"{location} observed {len(surfaces)} roles instead of two")
    return surfaces


def _surface_identity(
    surface: dict[str, Any], location: str
) -> tuple[str, str, str]:
    return (
        _object_id(surface.get("roleId"), f"{location}.roleId"),
        _object_id(surface.get("waylandSurfaceId"), f"{location}.waylandSurfaceId"),
        _object_id(surface.get("outputId"), f"{location}.outputId"),
    )


def _surface_contracts(spec: LogicalOutputSpec) -> dict[int, dict[str, Any]]:
    shelf_width = (spec.logical_width * 52 + 50) // 100
    return {
        ANCHOR_TOP: {
            "layer": LAYER_TOP,
            "anchors": TOP_ANCHORS,
            "exclusiveEdge": ANCHOR_TOP,
            "exclusiveZone": EXPECTED_TOP_ZONE,
            "desiredSize": (0, EXPECTED_TOP_ZONE),
            "configured": (spec.logical_width, EXPECTED_TOP_ZONE),
        },
        ANCHOR_BOTTOM: {
            "layer": LAYER_TOP,
            "anchors": BOTTOM_ANCHORS,
            "exclusiveEdge": ANCHOR_BOTTOM,
            "exclusiveZone": EXPECTED_BOTTOM_ZONE,
            "desiredSize": (shelf_width, EXPECTED_BOTTOM_ZONE),
            "configured": (shelf_width, EXPECTED_BOTTOM_ZONE),
        },
    }


def _validate_mapped_epoch(
    surface: dict[str, Any], committed_epoch: int, contract: dict[str, Any], location: str
) -> None:
    configurations = _configurations(surface, location)
    mapping = _object(
        surface.get("activeBufferMapping"), f"{location}.activeBufferMapping"
    )
    mapping_epoch = _integer(
        mapping.get("commitEpoch"), f"{location}.activeBufferMapping.commitEpoch"
    )
    if not 0 < mapping_epoch <= committed_epoch:
        raise RuntimeError(f"{location} has an invalid mapped commit epoch")
    _object_id(mapping.get("bufferId"), f"{location}.activeBufferMapping.bufferId")
    attach_order = _integer(
        mapping.get("attachOrder"), f"{location}.activeBufferMapping.attachOrder"
    )
    commit_order = _integer(
        mapping.get("commitOrder"), f"{location}.activeBufferMapping.commitOrder"
    )
    configure_serial = _serial(
        mapping.get("configureSerial"), f"{location}.activeBufferMapping.configureSerial"
    )
    if configure_serial not in configurations:
        raise RuntimeError(f"{location} mapped an unknown configure serial")
    configuration = configurations[configure_serial]
    configure_epoch = _integer(
        mapping.get("configureCommittedEpoch"),
        f"{location}.activeBufferMapping.configureCommittedEpoch",
    )
    if not 0 < configure_epoch < mapping_epoch:
        raise RuntimeError(f"{location} configure did not precede buffer mapping")
    if configuration["committedEpoch"] != configure_epoch:
        raise RuntimeError(f"{location} mapped a mismatched configure epoch")
    acknowledge_order = configuration["acknowledgeOrder"]
    if acknowledge_order is None or not (
        0 < configuration["configureOrder"]
        < acknowledge_order
        < attach_order
        < commit_order
    ):
        raise RuntimeError(
            f"{location} did not prove configure < acknowledge < attach < commit"
        )
    if configuration["size"] != contract["configured"]:
        raise RuntimeError(
            f"{location} mapped configure size {configuration['size']}, "
            f"expected {contract['configured']}"
        )
    _require_expected_state(
        configuration["committedState"], contract, f"{location}.mappedConfigure.committedState"
    )
    mapped_state = _role_state(
        mapping.get("committedState"), f"{location}.activeBufferMapping.committedState"
    )
    _require_expected_state(
        mapped_state, contract, f"{location}.activeBufferMapping.committedState"
    )


def validate_active_protocol(
    protocol: dict[str, Any], spec: LogicalOutputSpec, location: str
) -> set[tuple[str, str, str]]:
    surfaces = _protocol_surfaces(protocol, location)
    identities: set[tuple[str, str, str]] = set()
    surfaces_by_edge: dict[
        int, tuple[dict[str, Any], dict[str, Any], str, int]
    ] = {}
    for index, surface in enumerate(surfaces):
        surface_location = f"{location}.surfaces[{index}]"
        identity = _surface_identity(surface, surface_location)
        if identity in identities:
            raise RuntimeError("layer role identities must be unique")
        identities.add(identity)
        if _integer(surface.get("requestCount"), f"{surface_location}.requestCount") != 1:
            raise RuntimeError(f"{surface_location} reused one protocol role ID")
        if surface.get("scope") != EXPECTED_SCOPE:
            raise RuntimeError(f"{surface_location} did not use the QindaQt dock scope")
        initial_layer = _integer(
            surface.get("initialLayer"), f"{surface_location}.initialLayer"
        )
        if _boolean(surface.get("roleDestroyed"), f"{surface_location}.roleDestroyed"):
            raise RuntimeError(f"{surface_location} was destroyed before the active snapshot")
        if _boolean(surface.get("surfaceDestroyed"), f"{surface_location}.surfaceDestroyed"):
            raise RuntimeError(f"{surface_location} backing surface was already destroyed")
        if not _boolean(surface.get("mapped"), f"{surface_location}.mapped"):
            raise RuntimeError(f"{surface_location} had no active mapped buffer")
        _role_state(surface.get("pendingState"), f"{surface_location}.pendingState")
        committed_epoch = _integer(
            surface.get("committedEpoch"), f"{surface_location}.committedEpoch"
        )
        if committed_epoch <= 0:
            raise RuntimeError(f"{surface_location} had no committed role state")
        committed_state = _role_state(
            surface.get("committedState"), f"{surface_location}.committedState"
        )
        edge = committed_state["exclusiveEdge"]
        if edge in surfaces_by_edge:
            raise RuntimeError(f"multiple layer surfaces claimed exclusive edge {edge}")
        surfaces_by_edge[edge] = (
            surface,
            committed_state,
            surface_location,
            initial_layer,
        )

    role_ids = {identity[0] for identity in identities}
    surface_ids = {identity[1] for identity in identities}
    output_ids = {identity[2] for identity in identities}
    if len(output_ids) != 1:
        raise RuntimeError(f"layer surfaces targeted different outputs: {sorted(output_ids)}")
    if output_ids.intersection(role_ids | surface_ids) or role_ids.intersection(surface_ids):
        raise RuntimeError("live Wayland objects reused an identity")
    contracts = _surface_contracts(spec)
    if set(surfaces_by_edge) != set(contracts):
        raise RuntimeError(
            "expected one top and one bottom exclusive edge, observed "
            f"{sorted(surfaces_by_edge)}"
        )
    for edge, contract in contracts.items():
        surface, committed_state, surface_location, initial_layer = surfaces_by_edge[edge]
        if initial_layer != contract["layer"]:
            raise RuntimeError(
                f"{surface_location}.initialLayer was {initial_layer}, "
                f"expected {contract['layer']}"
            )
        _require_expected_state(
            committed_state, contract, f"{surface_location}.committedState"
        )
        _validate_mapped_epoch(
            surface,
            _integer(surface.get("committedEpoch"), f"{surface_location}.committedEpoch"),
            contract,
            surface_location,
        )
    return identities


def validate_final_trace(
    protocol: dict[str, Any], active_identities: set[tuple[str, str, str]]
) -> None:
    surfaces = _protocol_surfaces(protocol, "layerProtocol")
    final_identities: set[tuple[str, str, str]] = set()
    for index, surface in enumerate(surfaces):
        location = f"layerProtocol.surfaces[{index}]"
        if _integer(surface.get("requestCount"), f"{location}.requestCount") != 1:
            raise RuntimeError(f"{location} reused a role identity during teardown")
        _boolean(surface.get("roleDestroyed"), f"{location}.roleDestroyed")
        _boolean(surface.get("surfaceDestroyed"), f"{location}.surfaceDestroyed")
        final_identities.add(_surface_identity(surface, location))
    if final_identities != active_identities:
        raise RuntimeError("layer role identities changed after the active snapshot")
