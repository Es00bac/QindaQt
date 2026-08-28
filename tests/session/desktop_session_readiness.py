# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded full-topology polling and durable probe observations."""

from __future__ import annotations

import json
import select
import subprocess
import time
from typing import Any, Callable, Mapping, TextIO

from desktop_session_topology import (
    BootTopology,
    TopologyContractError,
    desktop_1080p_topology,
    observed_applications,
    validate_topology_readiness,
)


MARKER = "QINDAQT_DESKTOP_SESSION_PROBE="
READINESS_SECONDS = 15.0
PROBE_LIFETIME_SECONDS = 1.0
REQUIRED_METHODS = (
    "outputs",
    "shellVisibility",
    "inputCapabilities",
    "developmentShellSurfaces",
    "windows",
)


class ReadinessDeadlineExpired(RuntimeError):
    """One fixed probe lifetime or the enclosing readiness budget expired."""


def require_probe_lifetime(outer_remaining: float) -> float:
    """Reserve one full fixed probe lifetime without extending the outer cap."""

    if outer_remaining < PROBE_LIFETIME_SECONDS:
        raise ReadinessDeadlineExpired("no complete probe lifetime remains")
    return PROBE_LIFETIME_SECONDS


def remaining_probe_lifetime(
    deadline: float, *, monotonic: Callable[[], float] = time.monotonic
) -> float:
    remaining = deadline - monotonic()
    if remaining <= 0:
        raise ReadinessDeadlineExpired("probe exceeded its fixed lifetime")
    return remaining


def _parse_probe(line: str) -> dict[str, Any]:
    if not line.startswith(MARKER):
        raise RuntimeError("desktop probe marker was missing")
    document = json.loads(line.removeprefix(MARKER))
    if not isinstance(document, dict) or document.get("schemaVersion") != 1:
        raise RuntimeError("desktop probe returned an invalid document")
    return document


def parse_and_archive_probe(line: str, log: TextIO) -> dict[str, Any]:
    """Archive the exact observation before accepting or rejecting its schema."""

    log.write(line if line.endswith("\n") else line + "\n")
    log.flush()
    return _parse_probe(line)


def read_probe_document(
    probe: subprocess.Popen[str],
    deadline: float,
    *,
    monotonic: Callable[[], float] = time.monotonic,
) -> Mapping[str, Any]:
    if probe.stdout is None:
        raise RuntimeError("desktop probe stdout was unavailable")
    timeout = remaining_probe_lifetime(deadline, monotonic=monotonic)
    readable, _, _ = select.select([probe.stdout], [], [], timeout)
    if not readable:
        raise ReadinessDeadlineExpired("probe response exceeded its fixed lifetime")
    line = probe.stdout.readline()
    if not line:
        raise RuntimeError("desktop probe exited without an evidence marker")
    log = getattr(probe, "_qindaqt_log", None)
    if log is None:
        raise RuntimeError("desktop probe log was unavailable")
    return parse_and_archive_probe(line, log)


def _snapshot_pending(
    probe: Mapping[str, Any], topology: BootTopology | None = None
) -> str | None:
    topology = topology or desktop_1080p_topology()
    expected_services = {item.name for item in topology.services}
    raw_services = probe.get("services")
    if not isinstance(raw_services, list):
        raise RuntimeError("probe service evidence was malformed")
    services: dict[str, Mapping[str, Any]] = {}
    for raw in raw_services:
        if not isinstance(raw, Mapping) or not isinstance(raw.get("name"), str):
            raise RuntimeError("probe service evidence contained a malformed record")
        name = str(raw["name"])
        if name in services or name not in expected_services:
            raise RuntimeError("probe service evidence had duplicate or unexpected names")
        services[name] = raw
    service_pending: str | None = None
    if set(services) != expected_services:
        service_pending = "required service ownership is incomplete"
    else:
        for name, record in services.items():
            status = record.get("status")
            if status == "unavailable":
                service_pending = f"service {name} is not owned yet"
                continue
            if (
                status != "owned"
                or not isinstance(record.get("owner"), str)
                or not str(record["owner"]).startswith(":")
            ):
                raise RuntimeError(f"service {name} returned invalid ownership evidence")
            try:
                pid = int(str(record.get("pid", "0")), 10)
            except ValueError as error:
                raise RuntimeError(f"service {name} returned a malformed PID") from error
            if pid <= 1:
                raise RuntimeError(f"service {name} returned an invalid PID")
    values = {key: probe.get(key) for key in REQUIRED_METHODS}
    if not all(isinstance(value, Mapping) for value in values.values()):
        raise RuntimeError("probe method evidence was malformed")
    # AGENT-GUARD: Service ownership is the prerequisite for method evidence.
    # A cold-boot probe must remain a retryable observation instead of turning
    # expected not-ready method placeholders into a terminal runtime failure.
    if service_pending is not None:
        return service_pending
    for key, value in values.items():
        if value.get("status") != "ok":
            raise RuntimeError(f"public D-Bus method {key} returned an error")
    output = values["outputs"]
    visibility = values["shellVisibility"]
    try:
        applications = observed_applications(
            list(values["windows"].get("windows", [])), topology
        )
    except TopologyContractError as error:
        return str(error)
    candidate = {
        "outputs": output.get("outputs", []),
        "visibilityOutputs": visibility.get("outputs", []),
        "generations": {
            "outputs": output.get("outputGeneration"),
            "shellVisibility": visibility.get("outputGeneration"),
        },
        "inputDevices": values["inputCapabilities"].get("devices", []),
        "dockSurfaces": values["developmentShellSurfaces"].get("surfaces", []),
        "applications": applications,
    }
    try:
        validate_topology_readiness(candidate, topology)
    except TopologyContractError as error:
        return str(error)
    return None


def await_complete_snapshot(
    sample: Callable[[float], Mapping[str, Any]],
    *,
    topology: BootTopology | None = None,
    seconds: float = READINESS_SECONDS,
    monotonic: Callable[[], float] = time.monotonic,
    sleep: Callable[[float], None] = time.sleep,
) -> Mapping[str, Any]:
    """Poll until all public topology inputs are ready in the same snapshot."""

    deadline = monotonic() + seconds
    last_pending = "no snapshot was sampled"
    while True:
        remaining = deadline - monotonic()
        if remaining <= 0:
            raise RuntimeError(f"desktop topology readiness timed out: {last_pending}")
        try:
            document = sample(remaining)
        except ReadinessDeadlineExpired as error:
            raise RuntimeError(
                f"desktop topology readiness timed out: {last_pending}; {error}"
            ) from None
        if monotonic() > deadline:
            raise RuntimeError(f"desktop topology readiness timed out: {last_pending}")
        pending = _snapshot_pending(document, topology)
        if pending is None:
            return document
        last_pending = pending
        sleep(min(0.05, max(0.0, deadline - monotonic())))
