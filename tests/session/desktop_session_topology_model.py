# SPDX-License-Identifier: GPL-3.0-or-later
"""Immutable topology expectations for contained desktop qualification."""

from __future__ import annotations

from dataclasses import asdict, dataclass
from typing import Any, Mapping, Sequence

from desktop_session_matrix import DesktopMatrixScenario


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
    width: int
    height: int
    scale: float


@dataclass(frozen=True)
class DockExpectation:
    scope: str
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


@dataclass(frozen=True)
class MatrixPresentationExpectation:
    scenario_id: str
    profile_id: str
    theme_id: str
    requested_scale: float


@dataclass(frozen=True)
class MatrixOutputExpectation:
    ordinal: int
    pixel_width: int
    pixel_height: int
    logical_x: int
    logical_y: int
    logical_width: int
    logical_height: int
    # The nested backend publishes the child surface in logical coordinates at
    # protocol scale 1. Fractional raster scale is owned and proved by the
    # private parent compositor and capture dimensions.
    scale: float
    render_scale: float


@dataclass(frozen=True)
class MatrixBootTopology:
    """S3 topology without changing the accepted S1/S2 evidence schema."""

    schema_version: int
    topology_id: str
    outputs: tuple[MatrixOutputExpectation, ...]
    presentation: MatrixPresentationExpectation
    processes: tuple[ProcessExpectation, ...]
    services: tuple[ServiceExpectation, ...]
    applications: tuple[ApplicationExpectation, ...]
    dock: DockExpectation

    def document(self) -> dict[str, Any]:
        return asdict(self)


DesktopTopology = BootTopology | MatrixBootTopology


def desktop_1080p_topology() -> BootTopology:
    """Return the immutable S1 process/service/surface contract."""

    return BootTopology(
        schema_version=1,
        topology_id="qindaqt.desktop.virtual.1080p.v1",
        output=OutputExpectation(1920, 1080, 1.0),
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
        # AGENT-CONTRACT: S1 consumes compositor-owned production dock records;
        # ordinary window or visibility inventories are not substitutes.
        dock=DockExpectation("dock", 1),
    )


def interactive_1080p_topology() -> BootTopology:
    """Return the immutable S2 parent-Wayland interaction/capture contract."""

    base = desktop_1080p_topology()
    return BootTopology(
        schema_version=1,
        topology_id="qindaqt.desktop.windowed.1080p.interactive.v1",
        output=base.output,
        processes=(
            ProcessExpectation("parent-compositor", "weston", None),
            *base.processes,
        ),
        services=base.services,
        applications=base.applications,
        dock=base.dock,
    )


def interactive_matrix_topology(
    scenario: DesktopMatrixScenario,
) -> MatrixBootTopology:
    """Bind an approved matrix row to its exact runtime topology."""

    base = desktop_1080p_topology()
    return MatrixBootTopology(
        schema_version=1,
        topology_id=f"qindaqt.desktop.windowed.matrix.{scenario.scenario_id}.v1",
        outputs=tuple(
            MatrixOutputExpectation(
                ordinal=output.ordinal,
                pixel_width=output.pixel_width,
                pixel_height=output.pixel_height,
                logical_x=output.logical_x,
                logical_y=output.logical_y,
                logical_width=output.logical_width,
                logical_height=output.logical_height,
                scale=1.0,
                render_scale=output.scale,
            )
            for output in scenario.outputs
        ),
        presentation=MatrixPresentationExpectation(
            scenario.scenario_id,
            scenario.profile_id,
            scenario.theme_id,
            scenario.virtual.scale,
        ),
        processes=(
            *(
                (ProcessExpectation("parent-private-bus", "dbus-daemon", None),)
                if scenario.virtual.scale != 1.0 else ()
            ),
            ProcessExpectation(
                "parent-compositor",
                "kwin_wayland" if scenario.virtual.scale != 1.0 else "weston",
                None,
            ),
            *base.processes,
        ),
        services=base.services,
        applications=base.applications,
        dock=base.dock,
    )


def is_interactive_topology(topology: DesktopTopology) -> bool:
    return topology.topology_id.startswith("qindaqt.desktop.windowed.")


def observed_applications(
    windows: Sequence[Any], topology: DesktopTopology | None = None
) -> list[dict[str, Any]]:
    """Retain exact compositor identity for the required application windows."""

    result = []
    contract = topology or desktop_1080p_topology()
    for expected in contract.applications:
        matches = [
            item for item in windows
            if isinstance(item, Mapping)
            and item.get("applicationId") == expected.app_id
            and expected.window_title_contains in str(item.get("title", ""))
        ]
        if len(matches) != 1:
            raise ValueError(f"mapped test application was missing: {expected.app_id}")
        match = matches[0]
        window_id = match.get("id")
        if not isinstance(window_id, str) or not window_id:
            raise ValueError(
                f"mapped test application has no window ID: {expected.app_id}"
            )
        result.append({
            "appId": match["applicationId"],
            "processRole": expected.process_role,
            "windowId": window_id,
            "windowTitle": match.get("title"),
            "mapped": True,
        })
    return result
