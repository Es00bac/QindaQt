# SPDX-License-Identifier: GPL-3.0-or-later
"""Process launch boundary for the contained desktop runtime."""

from __future__ import annotations

import argparse
import shlex
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping

from desktop_session_host_tools import library_search_roots
from desktop_session_lifecycle import LIFECYCLE_TRACE_ENVIRONMENT
from desktop_session_matrix import DesktopMatrixScenario
from desktop_session_process import RuntimeState, spawn_logged_process, wait_for_path
from desktop_session_stage import ResolvedStage
from nested_session_scenario import VirtualOutputSpec, write_virtual_output_config


@dataclass(frozen=True)
class DesktopLaunch:
    app_environment: dict[str, str]
    parent_environment: dict[str, str] | None
    child_socket: str
    private_bus_process_id: int
    parent_bus_process_id: int | None
    parent_process_id: int | None
    compositor_process_id: int


def _virtual_spec(
    scenario: DesktopMatrixScenario | None,
) -> VirtualOutputSpec:
    return (
        scenario.virtual
        if scenario is not None
        else VirtualOutputSpec("single-1080p", 1, 1920, 1080, 1920, 1080, 1.0)
    )


def _configure_private_session(
    environment: Mapping[str, str], scenario: DesktopMatrixScenario | None
) -> str:
    config = Path(environment["XDG_CONFIG_HOME"])
    config.mkdir(parents=True, exist_ok=True)
    # AGENT-CONTRACT: Matrix KWin takes mode/scale from its CLI. Persisting the
    # virtual-output bootstrap would make output-store policy normalize the
    # nested Wayland connector; only S1/S2 retain that bootstrap fixture.
    if scenario is None:
        write_virtual_output_config(config, _virtual_spec(scenario))
    (config / "kscreenlockerrc").write_text(
        "[Daemon]\nAutolock=false\nLockOnResume=false\n", encoding="utf-8"
    )
    return f"qindaqt-{environment['QINDAQT_SESSION_RUN_ID'][:12]}"


def _traced_session_program(
    stage: ResolvedStage,
    environment: Mapping[str, str],
    scenario: DesktopMatrixScenario,
    wrapper: Path,
) -> Path:
    python = shlex.quote(sys.executable)
    tracer = shlex.quote(
        "/opt/qindaqt-source/tests/session/desktop_session_lifecycle.py"
    )
    trace = shlex.quote("/var/log/qindaqt-desktop/session-lifecycle.jsonl")
    child_wrappers: dict[str, Path] = {}
    for role in ("notification", "shell"):
        child_wrapper = Path(environment["XDG_RUNTIME_DIR"]) / f"qindaqt-{role}-trace"
        child_executable = shlex.quote(str(stage.executables[role]))
        child_wrapper.write_text(
            "#!/usr/bin/sh\n"
            f'exec {python} {tracer} --role {role} --executable '
            f'{child_executable} --trace {trace} -- "$@"\n',
            encoding="utf-8",
        )
        child_wrapper.chmod(0o700)
        child_wrappers[role] = child_wrapper
    executable = shlex.quote(str(stage.executables["session"]))
    profile = shlex.quote(scenario.profile_id)
    theme = shlex.quote(scenario.theme_id)
    notification = shlex.quote(str(child_wrappers["notification"]))
    shell = shlex.quote(str(child_wrappers["shell"]))
    wrapper.write_text(
        "#!/usr/bin/sh\n"
        f"exec {python} {tracer} --role session --executable {executable} "
        f"--trace {trace} --exec-only -- --profile {profile} --theme {theme} "
        f"--notification-host {notification} --shell {shell}\n",
        encoding="utf-8",
    )
    wrapper.chmod(0o700)
    return wrapper


def _session_program(
    stage: ResolvedStage,
    environment: Mapping[str, str],
    scenario: DesktopMatrixScenario | None,
) -> Path:
    if scenario is None:
        return stage.executables["session"]
    wrapper = Path(environment["XDG_RUNTIME_DIR"]) / "qindaqt-matrix-session"
    if environment.get(LIFECYCLE_TRACE_ENVIRONMENT) == "1":
        return _traced_session_program(stage, environment, scenario, wrapper)
    wrapper.write_text(
        # The empty root has no /bin compatibility symlink.
        "#!/usr/bin/sh\n"
        f"exec {shlex.quote(str(stage.executables['session']))} "
        f"--profile {shlex.quote(scenario.profile_id)} "
        f"--theme {shlex.quote(scenario.theme_id)}\n",
        encoding="utf-8",
    )
    wrapper.chmod(0o700)
    return wrapper


def _read_exact_process_arguments(
    process_id: int, executable: Path, expected_tail: list[str], role: str
) -> list[str]:
    try:
        raw = Path(f"/proc/{process_id}/cmdline").read_bytes()
    except OSError as error:
        raise RuntimeError(f"matrix {role} arguments are not observable") from error
    arguments = [item.decode("utf-8", "strict") for item in raw.split(b"\0") if item]
    if arguments != [str(executable), *expected_tail]:
        raise RuntimeError(f"live {role} process did not receive the matrix selection")
    return arguments[1:]


def _start_services(
    arguments: argparse.Namespace,
    stage: ResolvedStage,
    environment: dict[str, str],
    state: RuntimeState,
) -> int:
    runtime = Path(environment["XDG_RUNTIME_DIR"])
    bus = spawn_logged_process(
        "dbus-daemon",
        [str(arguments.dbus_daemon), "--session", "--nofork", "--nopidfile",
         f"--address={environment['DBUS_SESSION_BUS_ADDRESS']}"],
        environment,
    )
    state.track(bus, [arguments.dbus_daemon])
    wait_for_path(runtime / "bus", state, 5)
    for role in ("settings-service", "audio-service"):
        child = spawn_logged_process(role, [str(stage.executables[role])], environment)
        state.track(child, [stage.executables[role]])
    return bus.pid



def _parent_library_environment(
    arguments: argparse.Namespace, environment: Mapping[str, str]
) -> dict[str, str]:
    """Give the parent Weston (and its screenshooter) their own prefix libraries."""

    parent_environment = dict(environment)
    weston_libraries, _plugins, _qml = library_search_roots([Path(arguments.weston)])
    if weston_libraries:
        existing = parent_environment.get("LD_LIBRARY_PATH", "")
        parent_environment["LD_LIBRARY_PATH"] = ":".join(
            weston_libraries + ([existing] if existing else [])
        )
    return parent_environment

def _start_fractional_parent(
    arguments: argparse.Namespace,
    environment: dict[str, str],
    state: RuntimeState,
    virtual: VirtualOutputSpec,
    parent_socket: str,
) -> tuple[dict[str, str], int, int]:
    parent_environment = _parent_library_environment(arguments, environment)
    parent_bus_path = Path(environment["XDG_RUNTIME_DIR"]) / "qindaqt-parent-bus"
    parent_bus = spawn_logged_process(
        "parent-private-bus",
        [str(arguments.dbus_daemon), "--session", "--nofork", "--nopidfile",
         f"--address=unix:path={parent_bus_path}"],
        environment,
    )
    state.track(parent_bus, [arguments.dbus_daemon])
    wait_for_path(parent_bus_path, state, 5)
    # AGENT-GUARD: The raw parent owns this namespace-private bus, never the
    # QindaQt child or host bus, so it cannot claim the child's global actions.
    parent_environment.update({
        "DBUS_SESSION_BUS_ADDRESS": f"unix:path={parent_bus_path}",
        "QT_NO_XDG_DESKTOP_PORTAL": "1",
        "GTK_USE_PORTAL": "0",
    })
    parent = spawn_logged_process(
        "parent-compositor",
        [str(arguments.kwin_wayland), "--virtual",
         "--width", str(virtual.logical_width),
         "--height", str(virtual.logical_height),
         "--scale", str(virtual.scale), "--output-count", "1",
         "--socket", parent_socket, "--no-lockscreen", "--no-global-shortcuts"],
        parent_environment,
    )
    state.track(parent, [arguments.kwin_wayland])
    return parent_environment, parent_bus.pid, parent.pid


def _start_parent(
    arguments: argparse.Namespace,
    environment: dict[str, str],
    state: RuntimeState,
    scenario: DesktopMatrixScenario | None,
) -> tuple[dict[str, str] | None, int | None, int | None]:
    if not arguments.interactive:
        return None, None, None
    virtual = _virtual_spec(scenario)
    parent_socket = "qindaqt-parent-wayland"
    if scenario is not None and virtual.scale != 1.0:
        parent_environment, bus_pid, parent_pid = _start_fractional_parent(
            arguments, environment, state, virtual, parent_socket
        )
    else:
        parent_environment = _parent_library_environment(arguments, environment)
        parent = spawn_logged_process(
            "parent-compositor",
            [str(arguments.weston), "--backend=headless", "--renderer=pixman",
             "--shell=kiosk", "--debug", "--no-config", "--idle-time=0",
             "--fake-seat", f"--width={virtual.pixel_width}",
             f"--height={virtual.pixel_height}", "--scale=1",
             f"--socket={parent_socket}",
             "--log=/var/log/qindaqt-desktop/parent-compositor-weston.log"],
            parent_environment,
        )
        state.track(parent, [arguments.weston])
        bus_pid, parent_pid = None, parent.pid
    wait_for_path(Path(environment["XDG_RUNTIME_DIR"]) / parent_socket, state, 5)
    parent_environment["WAYLAND_DISPLAY"] = parent_socket
    return parent_environment, bus_pid, parent_pid


def _start_desktop(
    arguments: argparse.Namespace,
    stage: ResolvedStage,
    environment: dict[str, str],
    state: RuntimeState,
    scenario: DesktopMatrixScenario | None,
) -> DesktopLaunch:
    private_bus_pid = _start_services(arguments, stage, environment, state)
    socket_name = _configure_private_session(environment, scenario)
    virtual = _virtual_spec(scenario)
    parent_environment, parent_bus_pid, parent_pid = _start_parent(
        arguments, environment, state, scenario
    )
    compositor_environment = dict(environment)
    # AGENT-NOTE: inside the sandbox journald is unreachable, so Qt would drop
    # every qCritical from KWin, the session, and the shell; force stderr so the
    # compositor log carries the shell's own exit reason.
    compositor_environment["QT_FORCE_STDERR_LOGGING"] = "1"
    if parent_environment is not None:
        compositor_environment["WAYLAND_DISPLAY"] = "qindaqt-parent-wayland"
    scenario_path = (
        f"/opt/qindaqt-source/tests/scenarios/{scenario.scenario_id}.json"
        if scenario is not None
        else "/opt/qindaqt-source/tests/scenarios/single-1080p.json"
    )
    backend = "--windowed" if arguments.interactive else "--virtual"
    compositor = spawn_logged_process(
        "compositor",
        [str(stage.executables["launcher"]), "--plugin-root",
         str(stage.compositor_plugin.parents[2]),
         # AGENT-GUARD: name the configured KWin explicitly; the launcher's
         # PATH default could otherwise pick a different KWin release inside
         # the sandbox and reject the release-matched plugin.
         "--kwin", str(arguments.kwin_wayland), backend,
         "--width", str(virtual.logical_width),
         "--height", str(virtual.logical_height),
         "--scale", str(virtual.scale), "--output-count", str(virtual.output_count),
         "--socket", socket_name, "--test-scenario", scenario_path,
         "--session", str(_session_program(stage, environment, scenario))],
        compositor_environment,
    )
    state.track(compositor, [stage.executables["launcher"], arguments.kwin_wayland])
    wait_for_path(Path(environment["XDG_RUNTIME_DIR"]) / socket_name, state, 15)
    app_environment = dict(environment)
    app_environment["WAYLAND_DISPLAY"] = socket_name
    editor_arguments = ["--theme", scenario.theme_id] if scenario is not None else []
    commands = (
        ("settings-app", [str(stage.executables["settings-app"]), "--page", "notifications"]),
        ("editor-app", [str(stage.executables["editor-app"]), *editor_arguments]),
    )
    for role, command in commands:
        child = spawn_logged_process(role, command, app_environment)
        state.track(child, [stage.executables[role]])
    return DesktopLaunch(
        app_environment, parent_environment, socket_name, private_bus_pid,
        parent_bus_pid, parent_pid, compositor.pid,
    )
