# SPDX-License-Identifier: GPL-3.0-or-later
"""Deterministic contract for the first contained QindaQt desktop boot."""

from __future__ import annotations

from dataclasses import asdict, dataclass
from typing import Any, Mapping, Sequence


class TopologyContractError(ValueError):
    """Boot evidence does not describe the complete accepted topology."""


@dataclass(frozen=True)
class ProcessExpectation:
    role: str
    executable: str
    parent_role: str | None


@dataclass(frozen=True)
class ServiceExpectation:
    name: str
    process_role: str


@dataclass(frozen=True)
class ApplicationExpectation:
    app_id: str
    process_role: str
    window_title_contains: str


@dataclass(frozen=True)
class OutputExpectation:
    name: str
    width: int
    height: int
    scale: float


@dataclass(frozen=True)
class DockExpectation:
    scope: str
    output_name: str
    minimum_count: int


@dataclass(frozen=True)
class BootTopology:
    schema_version: int
    topology_id: str
    output: OutputExpectation
    processes: tuple[ProcessExpectation, ...]
    services: tuple[ServiceExpectation, ...]
    applications: tuple[ApplicationExpectation, ...]
    dock: DockExpectation

    def document(self) -> dict[str, Any]:
        return asdict(self)


def desktop_1080p_topology() -> BootTopology:
    """Return the immutable S1 process/service/surface contract.

    Executable values are package-relative basenames. Runtime path resolution is
    owned by ``desktop_session_stage`` and must not be inferred from ``PATH``.
    """

    return BootTopology(
        schema_version=1,
        topology_id="qindaqt.desktop.virtual.1080p.v1",
        output=OutputExpectation("Virtual-1", 1920, 1080, 1.0),
        processes=(
            ProcessExpectation("private-bus", "dbus-daemon", None),
            ProcessExpectation("compositor", "kwin_wayland", None),
            ProcessExpectation("session", "qindaqt-session", "compositor"),
            ProcessExpectation(
                "notification", "qindaqt-notification-host", "session"
            ),
            ProcessExpectation("shell", "qindaqt-shell", "session"),
            ProcessExpectation(
                "settings-service", "qindaqt-settings-service", None
            ),
            ProcessExpectation("audio-service", "qindaqt-audio-service", None),
            ProcessExpectation("settings-app", "qindaqt-settings", None),
            ProcessExpectation("editor-app", "qindaqt-editor", None),
            ProcessExpectation(
                "session-probe", "qindaqt-desktop-session-probe", None
            ),
        ),
        services=(
            ServiceExpectation("org.qindaqt.Compositor", "compositor"),
            ServiceExpectation("org.qindaqt.Settings1", "settings-service"),
            ServiceExpectation("org.qindaqt.Audio1", "audio-service"),
            ServiceExpectation("org.freedesktop.Notifications", "notification"),
        ),
        applications=(
            ApplicationExpectation("org.qindaqt.Settings", "settings-app", "Settings"),
            ApplicationExpectation(
                "org.qindaqt.TextEditor", "editor-app", "QindaQt Text Editor"
            ),
        ),
        # AGENT-CONTRACT: Notification Live broadens its development-only,
        # compositor-owned surface inventory to the production shell's `dock`
        # scope. S1 must fail, not infer panel mapping from ordinary windows.
        dock=DockExpectation("dock", "Virtual-1", 1),
    )


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
    evidence: Mapping[str, Any], topology: BootTopology
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
    evidence: Mapping[str, Any], topology: BootTopology, pids: Mapping[str, int]
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


def _validate_output(evidence: Mapping[str, Any], topology: BootTopology) -> None:
    outputs = _sequence(evidence.get("outputs"), "evidence.outputs")
    if len(outputs) != 1:
        raise TopologyContractError("S1 requires exactly one output")
    output = _mapping(outputs[0], "outputs[0]")
    geometry = _mapping(output.get("geometry"), "outputs[0].geometry")
    expected = topology.output
    if (
        output.get("name") != expected.name
        or geometry.get("x") != 0
        or geometry.get("y") != 0
        or geometry.get("width") != expected.width
        or geometry.get("height") != expected.height
        or output.get("scale") != expected.scale
    ):
        raise TopologyContractError("the output is not exact 1920x1080@1 Virtual-1")
    generations = _mapping(evidence.get("generations"), "evidence.generations")
    output_generation = _canonical_generation(
        generations.get("outputs"), "generations.outputs"
    )
    visibility_generation = _canonical_generation(
        generations.get("shellVisibility"), "generations.shellVisibility"
    )
    if output_generation != visibility_generation:
        raise TopologyContractError("output and shell visibility generations differ")


def _validate_input_and_dock(evidence: Mapping[str, Any], topology: BootTopology) -> None:
    devices = _sequence(evidence.get("inputDevices"), "evidence.inputDevices")
    development = [
        item
        for item in devices
        if isinstance(item, Mapping)
        and item.get("name") == "QindaQt Development Input"
        and item.get("keyboard") is True
        and item.get("pointer") is True
    ]
    if len(development) != 1:
        raise TopologyContractError("exactly one combined development input is required")
    surfaces = _sequence(evidence.get("dockSurfaces"), "evidence.dockSurfaces")
    matched = [
        item
        for item in surfaces
        if isinstance(item, Mapping)
        and item.get("scope") == topology.dock.scope
        and item.get("outputName") == topology.dock.output_name
        and item.get("desiredOutputName") == topology.dock.output_name
        and item.get("mapped") is True
        and item.get("committed") is True
    ]
    if len(matched) < topology.dock.minimum_count:
        raise TopologyContractError("no mapped and committed production dock surface was proven")


def _validate_applications(evidence: Mapping[str, Any], topology: BootTopology) -> None:
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
        if record.get("mapped") is not True or not isinstance(title, str):
            raise TopologyContractError(f"application {expected.app_id} is not mapped")
        if expected.window_title_contains not in title:
            raise TopologyContractError(f"application {expected.app_id} title is unexpected")


def validate_boot_evidence(document: Any) -> None:
    """Validate one complete evidence object; partial success is never accepted."""

    topology = desktop_1080p_topology()
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
    _validate_output(evidence, topology)
    _validate_input_and_dock(evidence, topology)
    _validate_applications(evidence, topology)
    cleanup = _mapping(evidence.get("cleanup"), "evidence.cleanup")
    if cleanup.get("bounded") is not True or cleanup.get("survivorPids") != []:
        raise TopologyContractError("cleanup was not bounded and survivor-free")
