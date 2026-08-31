# SPDX-License-Identifier: GPL-3.0-or-later
"""Deterministic contract for the first contained QindaQt desktop boot."""

from __future__ import annotations

from pathlib import Path
from typing import Any, Mapping, Sequence

from desktop_session_topology_model import (
    ApplicationExpectation,
    BootTopology,
    DesktopTopology,
    DockExpectation,
    MatrixBootTopology,
    MatrixOutputExpectation,
    MatrixPresentationExpectation,
    OutputExpectation,
    ProcessExpectation,
    ServiceExpectation,
    desktop_1080p_topology,
    interactive_1080p_topology,
    interactive_matrix_topology,
    is_interactive_topology,
    observed_applications as _observed_applications,
)
from desktop_session_output import (
    OutputInventoryError,
    validate_matrix_output_inventory,
    validate_output_inventory,
)


class TopologyContractError(ValueError):
    """Boot evidence does not describe the complete accepted topology."""


def observed_applications(
    windows: Sequence[Any], topology: DesktopTopology | None = None
) -> list[dict[str, Any]]:
    try:
        return _observed_applications(windows, topology)
    except ValueError as error:
        raise TopologyContractError(str(error)) from None


def _mapping(value: Any, location: str) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        raise TopologyContractError(f"{location} must be an object")
    return value


def _sequence(value: Any, location: str) -> Sequence[Any]:
    if isinstance(value, (str, bytes)) or not isinstance(value, Sequence):
        raise TopologyContractError(f"{location} must be an array")
    return value


def _canonical_pid(value: Any, location: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value <= 1:
        raise TopologyContractError(f"{location} must be a process id greater than 1")
    return value


def _canonical_generation(value: Any, location: str) -> str:
    if (
        not isinstance(value, str)
        or not value.isascii()
        or not value.isdigit()
        or value.startswith("0")
    ):
        raise TopologyContractError(f"{location} must be a canonical nonzero decimal")
    return value


def _validate_processes(
    evidence: Mapping[str, Any], topology: DesktopTopology
) -> dict[str, int]:
    observed = _mapping(evidence.get("processes"), "evidence.processes")
    expected_roles = {item.role for item in topology.processes}
    if set(observed) != expected_roles:
        raise TopologyContractError("process roles do not exactly match the boot topology")
    pids: dict[str, int] = {}
    for expected in topology.processes:
        record = _mapping(observed[expected.role], f"processes.{expected.role}")
        if set(record) != {"pid", "executable", "parentRole"}:
            raise TopologyContractError(
                f"processes.{expected.role} has an unexpected field set"
            )
        pid = _canonical_pid(record["pid"], f"processes.{expected.role}.pid")
        if record["executable"] != expected.executable:
            raise TopologyContractError(
                f"processes.{expected.role} did not use {expected.executable}"
            )
        if record["parentRole"] != expected.parent_role:
            raise TopologyContractError(
                f"processes.{expected.role} has the wrong parent role"
            )
        pids[expected.role] = pid
    if len(set(pids.values())) != len(pids):
        raise TopologyContractError("process roles must identify distinct live processes")
    return pids


def _validate_services(
    evidence: Mapping[str, Any], topology: DesktopTopology, pids: Mapping[str, int]
) -> None:
    records = _sequence(evidence.get("services"), "evidence.services")
    by_name: dict[str, Mapping[str, Any]] = {}
    for index, raw in enumerate(records):
        record = _mapping(raw, f"services[{index}]")
        name = record.get("name")
        if not isinstance(name, str) or name in by_name:
            raise TopologyContractError("service names must be unique strings")
        by_name[name] = record
    if set(by_name) != {item.name for item in topology.services}:
        raise TopologyContractError("service names do not exactly match the boot topology")
    for expected in topology.services:
        record = by_name[expected.name]
        if set(record) != {"name", "owner", "pid", "executable"}:
            raise TopologyContractError(f"service {expected.name} has unexpected fields")
        if not isinstance(record["owner"], str) or not record["owner"].startswith(":"):
            raise TopologyContractError(f"service {expected.name} has no unique owner")
        if record["pid"] != pids[expected.process_role]:
            raise TopologyContractError(f"service {expected.name} owner PID is unbound")
        process = next(
            item for item in topology.processes if item.role == expected.process_role
        )
        if record["executable"] != process.executable:
            raise TopologyContractError(f"service {expected.name} owner executable is wrong")


def _validate_output(
    evidence: Mapping[str, Any], topology: DesktopTopology
) -> tuple[str, ...]:
    backend = "wayland" if is_interactive_topology(topology) else "virtual"
    try:
        if isinstance(topology, MatrixBootTopology):
            output_names = validate_matrix_output_inventory(
                evidence, expectations=topology.outputs, backend=backend
            )
        else:
            expected = topology.output
            output_names = (
                validate_output_inventory(
                    evidence, width=expected.width, height=expected.height,
                    scale=expected.scale, backend=backend,
                ),
            )
    except OutputInventoryError as error:
        raise TopologyContractError(str(error)) from None
    generations = _mapping(evidence.get("generations"), "evidence.generations")
    output_generation = _canonical_generation(
        generations.get("outputs"), "generations.outputs"
    )
    visibility_generation = _canonical_generation(
        generations.get("shellVisibility"), "generations.shellVisibility"
    )
    if output_generation != visibility_generation:
        raise TopologyContractError("output and shell visibility generations differ")
    return output_names


def _canonical_process_id(value: Any) -> int:
    process_id = int(value, 10) if isinstance(value, str) and value.isascii() and value.isdecimal() else 0
    if process_id <= 0 or value != str(process_id):
        raise TopologyContractError("dock processId must be a canonical positive decimal string")
    return process_id


def _validate_input_and_dock(
    evidence: Mapping[str, Any], topology: DesktopTopology,
    output_names: Sequence[str],
    shell_pid: int | None = None,
) -> None:
    devices = _sequence(evidence.get("inputDevices"), "evidence.inputDevices")
    development = [
        item for item in devices
        if isinstance(item, Mapping) and item.get("name") == "QindaQt Development Input"
    ]
    if len(development) != 1:
        raise TopologyContractError("exactly one combined development input is required")
    device = development[0]
    capabilities = _sequence(device.get("capabilities"), "inputDevices[0].capabilities")
    if (
        device.get("name") != "QindaQt Development Input"
        or device.get("enabled") is not True
        or len(capabilities) != 2
        or set(capabilities) != {"keyboard", "pointer"}
    ):
        raise TopologyContractError("exactly one combined development input is required")
    if is_interactive_topology(topology):
        forwarded = [item for item in devices if item is not device]
        identities = [item.get("id") for item in devices if isinstance(item, Mapping)]
        private_capabilities = {
            tuple(item.get("capabilities", []))
            for item in forwarded if isinstance(item, Mapping)
        }
        parent = next(
            item for item in topology.processes if item.role == "parent-compositor"
        )
        expected_forwarded = (
            {("keyboard",)} if parent.executable == "kwin_wayland"
            else {("keyboard",), ("pointer",)}
        )
        if (
            len(forwarded) != len(expected_forwarded)
            or private_capabilities != expected_forwarded
            or len(set(identities)) != len(devices)
            or any(
                not isinstance(item, Mapping)
                or item.get("name") != ""
                or item.get("enabled") is not True
                or item.get("busType") != 0
                or item.get("vendorId") != 0
                or item.get("productId") != 0
                or not isinstance(item.get("id"), str)
                or not str(item["id"]).startswith("input-")
                for item in forwarded
            )
        ):
            raise TopologyContractError(
                "interactive input devices do not match the exact private parent fake-seat"
            )
    elif len(devices) != 1:
        raise TopologyContractError("exactly one combined development input is required")
    dock_surfaces: list[Mapping[str, Any]] = []
    for item in _sequence(evidence.get("dockSurfaces"), "evidence.dockSurfaces"):
        if not isinstance(item, Mapping) or item.get("scope") != topology.dock.scope:
            continue
        process_id = _canonical_process_id(item.get("processId"))
        # AGENT-CONTRACT: Reject consumed dock records that contradict output inventory.
        output_name = item.get("outputName")
        if (
            output_name not in output_names
            or item.get("desiredOutputName") != output_name
        ):
            raise TopologyContractError("dock surface output identities differ")
        # AGENT-CONTRACT: Bind every consumed dock record to the separately
        # authenticated current shell; foreign/replaced client PIDs are not proof.
        if shell_pid is not None and process_id != shell_pid:
            raise TopologyContractError("dock processId does not match authenticated shell")
        dock_surfaces.append(item)
    matched = [
        item
        for item in dock_surfaces
        if item.get("mapped") is True
        and item.get("committed") is True
    ]
    if len(matched) < topology.dock.minimum_count:
        raise TopologyContractError("no mapped and committed production dock surface was proven")
    if isinstance(topology, MatrixBootTopology) and set(output_names) - {
        item.get("outputName") for item in matched
    }:
        raise TopologyContractError("every matrix output must have a mapped production dock")


def _validate_applications(evidence: Mapping[str, Any], topology: DesktopTopology) -> None:
    records = _sequence(evidence.get("applications"), "evidence.applications")
    by_id: dict[str, Mapping[str, Any]] = {}
    for index, raw in enumerate(records):
        record = _mapping(raw, f"applications[{index}]")
        app_id = record.get("appId")
        if not isinstance(app_id, str) or app_id in by_id:
            raise TopologyContractError("application IDs must be unique strings")
        by_id[app_id] = record
    if set(by_id) != {item.app_id for item in topology.applications}:
        raise TopologyContractError("test applications do not exactly match the topology")
    for expected in topology.applications:
        record = by_id[expected.app_id]
        title = record.get("windowTitle")
        if set(record) != {
            "appId", "processRole", "windowId", "windowTitle", "mapped"
        }:
            raise TopologyContractError(
                f"application {expected.app_id} has unexpected fields"
            )
        if (
            record.get("processRole") != expected.process_role
            or record.get("mapped") is not True
            or not isinstance(record.get("windowId"), str)
            or not record["windowId"]
            or not isinstance(title, str)
        ):
            raise TopologyContractError(f"application {expected.app_id} is not mapped")
        if expected.window_title_contains not in title:
            raise TopologyContractError(f"application {expected.app_id} title is unexpected")


def validate_topology_readiness(
    evidence: Mapping[str, Any], topology: DesktopTopology | None = None
) -> None:
    """Validate the simultaneous public inputs that can become ready asynchronously."""

    contract = topology or desktop_1080p_topology()
    output_names = _validate_output(evidence, contract)
    _validate_input_and_dock(evidence, contract, output_names)
    _validate_applications(evidence, contract)


def _validate_measurements(evidence: Mapping[str, Any]) -> None:
    measurements = _mapping(evidence.get("measurements"), "evidence.measurements")
    if set(measurements) != {"residentPssKiB", "ceilingKiB"}:
        raise TopologyContractError("measurements must contain the exact PSS field set")
    resident = measurements["residentPssKiB"]
    ceiling = measurements["ceilingKiB"]
    if (
        isinstance(resident, bool)
        or not isinstance(resident, int)
        or resident < 0
        or isinstance(ceiling, bool)
        or ceiling != 1_048_576
        or resident > ceiling
    ):
        raise TopologyContractError("resident PSS is malformed or exceeds 1,048,576 KiB")


def _validate_cleanup(
    evidence: Mapping[str, Any], topology: DesktopTopology, pids: Mapping[str, int]
) -> None:
    cleanup = _mapping(evidence.get("cleanup"), "evidence.cleanup")
    if (
        set(cleanup) != {"bounded", "survivorPids", "terminalPhases"}
        or cleanup.get("bounded") is not True
        or cleanup.get("survivorPids") != []
    ):
        raise TopologyContractError("cleanup was not bounded and survivor-free")
    records = _sequence(cleanup.get("terminalPhases"), "cleanup.terminalPhases")
    by_role: dict[str, Mapping[str, Any]] = {}
    for index, raw in enumerate(records):
        record = _mapping(raw, f"cleanup.terminalPhases[{index}]")
        role = record.get("role")
        if not isinstance(role, str) or role in by_role:
            raise TopologyContractError("cleanup roles must be unique strings")
        by_role[role] = record
    if set(by_role) != {item.role for item in topology.processes}:
        raise TopologyContractError("cleanup roles do not exactly match process roles")
    for expected in topology.processes:
        record = by_role[expected.role]
        if set(record) != {
            "role", "pid", "processGroup", "executablePath", "startTicks",
            "terminalPhase",
        }:
            raise TopologyContractError(f"cleanup role {expected.role} has unexpected fields")
        path = Path(str(record["executablePath"]))
        if (
            record["pid"] != pids[expected.role]
            or isinstance(record["processGroup"], bool)
            or not isinstance(record["processGroup"], int)
            or record["processGroup"] <= 1
            or not path.is_absolute()
            or path.name != expected.executable
            or isinstance(record["startTicks"], bool)
            or not isinstance(record["startTicks"], int)
            or record["startTicks"] <= 0
            or record["terminalPhase"] not in {"already-exited", "term", "kill"}
        ):
            raise TopologyContractError(
                f"cleanup role {expected.role} has unauthenticated terminal evidence"
            )


def validate_boot_evidence(
    document: Any, topology: DesktopTopology | None = None
) -> None:
    """Validate one complete evidence object; partial success is never accepted."""

    topology = topology or desktop_1080p_topology()
    evidence = _mapping(document, "evidence")
    if evidence.get("schemaVersion") != 1:
        raise TopologyContractError("evidence.schemaVersion must be 1")
    if evidence.get("topology") != topology.document():
        raise TopologyContractError("evidence embeds a different boot topology")
    containment = _mapping(evidence.get("containment"), "evidence.containment")
    if (
        containment.get("mode") != "bwrap-pid-network-ipc"
        or containment.get("hostDisplayReachable") is not False
        or containment.get("hostSessionBusReachable") is not False
        or containment.get("hostInputReachable") is not False
    ):
        raise TopologyContractError("containment did not fail closed")
    pids = _validate_processes(evidence, topology)
    _validate_services(evidence, topology, pids)
    output_names = _validate_output(evidence, topology)
    _validate_input_and_dock(evidence, topology, output_names, pids["shell"])
    _validate_applications(evidence, topology)
    _validate_measurements(evidence)
    _validate_cleanup(evidence, topology, pids)
