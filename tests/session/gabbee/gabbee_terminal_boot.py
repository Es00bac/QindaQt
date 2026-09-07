# SPDX-License-Identifier: GPL-3.0-or-later
"""Inner private-session boot for the Terminal PTY proof.

Brings up, in the exact order the real portal requires: the private session
bus, the private PipeWire core, the parent Weston compositor, the child
QindaQt compositor windowed on that parent, and the Terminal and Editor
applications. Two ordering constraints here are load-bearing and are stated
as guards at their call sites; changing the order silently reintroduces an
unconditional portal denial that reports itself only as a one-line warning
in the KDE backend's log.

Teardown is this module's responsibility too: the proof must leave no
private survivors, and a survivor turns the run red rather than being
reported as a warning.
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent

TERMINAL_APP = "org.qindaqt.Terminal"
EDITOR_APP = "org.qindaqt.TextEditor"


def run_inner() -> int:
    sys.path.insert(0, str(HERE.parent))
    from desktop_session_process import RuntimeState

    parser = argparse.ArgumentParser(description="inner Gabbee Terminal PTY proof session")
    parser.add_argument("--inner", action="store_true")
    parser.add_argument("--stage-root", type=Path, required=True)
    parser.add_argument("--bin-directory", required=True)
    parser.add_argument("--plugin-relative", required=True)
    parser.add_argument("--decoration-relative", required=True)
    parser.add_argument("--settings-service-directory", required=True)
    parser.add_argument("--audio-service-directory", required=True)
    parser.add_argument("--dbus-daemon", required=True)
    parser.add_argument("--kwin-wayland", required=True)
    parser.add_argument("--weston", required=True)
    parser.add_argument("--result-path", type=Path, required=True)
    arguments = parser.parse_args()

    from desktop_session_stage import resolve_stage

    stage = resolve_stage(
        arguments.stage_root, bin_directory=arguments.bin_directory,
        plugin_relative=arguments.plugin_relative, decoration_relative=arguments.decoration_relative,
        settings_service_directory=arguments.settings_service_directory,
        audio_service_directory=arguments.audio_service_directory,
    )
    environment = dict(os.environ)
    state = RuntimeState()
    exit_code = 1
    try:
        exit_code = _run_inner(arguments, stage, environment, state)
    finally:
        survivors = teardown(state)
        if survivors:
            print(f"private processes survived teardown: {survivors}", file=sys.stderr)
            exit_code = 1
    return exit_code


def teardown(state) -> list[int]:
    for process in reversed(state.processes):
        process.terminate()
    for process in state.processes:
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                pass
    return [process.pid for process in state.processes if process.poll() is None]


def _run_inner(arguments: argparse.Namespace, stage, environment: dict, state) -> int:
    from gabbee_probe_support import ResultDocument

    run_id = environment.get("QINDAQT_SESSION_RUN_ID", "terminal-pty")
    document = ResultDocument(run_id, "terminal-pty-proof")
    try:
        return _run_inner_phases(arguments, stage, environment, state, document)
    finally:
        document.write(arguments.result_path)
        copy_process_logs(arguments.result_path.parent)


def copy_process_logs(evidence_root: Path) -> None:
    """Copy each spawned process's own log next to the result document.

    AGENT-NOTE: spawn_logged_process writes to /var/log/qindaqt-desktop inside
    the sandbox; without this the outer runner's evidence copy only ever sees
    the JSON result and never the portal backend's own diagnostic output,
    which is where every portal denial explains itself.
    """

    log_src = Path("/var/log/qindaqt-desktop")
    if not log_src.is_dir():
        return
    evidence_root.mkdir(parents=True, exist_ok=True)
    for log_file in log_src.iterdir():
        if log_file.is_file() and log_file.stat().st_size < 256 * 1024:
            (evidence_root / f"process-log-{log_file.name}").write_bytes(log_file.read_bytes())


def _run_inner_phases(
    arguments: argparse.Namespace, stage, environment: dict, state, document,
) -> int:
    from desktop_session_launch import _configure_private_session, _virtual_spec
    from desktop_session_host_tools import library_search_roots
    from desktop_session_process import spawn_logged_process, wait_for_path
    from gabbee_probe_support import PhaseResult
    from gabbee_terminal_delivery import remote_desktop_delivery

    runtime = Path(environment["XDG_RUNTIME_DIR"])

    document.add(preflight_phase())

    _start_private_bus(arguments, stage, environment, state, runtime)

    # AGENT-GUARD: the private PipeWire core must run BEFORE the compositor;
    # KWin's screencast plugin connects exactly once at compositor startup.
    # _start_private_pipewire carries the full failure trace.
    _start_private_pipewire(environment, state)

    # -- parent Wayland compositor (private Weston, headless + pixman).
    # AGENT-GUARD: the child KWin must run --windowed on this parent socket,
    # not --virtual. The real xdg-desktop-portal-kde RemoteDesktop backend
    # denies CreateSession unless KWin advertises zkde_screencast_unstable_v1,
    # and the --virtual platform never does. This is the exact parent backend
    # the accepted interactive audit configuration uses; note it renders with
    # pixman (software), so this is not a GPU/GL requirement.
    socket_name = _configure_private_session(environment, None)
    virtual = _virtual_spec(None)
    parent_socket = "qindaqt-parent-wayland"
    parent_environment = dict(environment)
    weston_libraries, _plugins, _qml = library_search_roots([Path(arguments.weston)])
    if weston_libraries:
        existing = parent_environment.get("LD_LIBRARY_PATH", "")
        parent_environment["LD_LIBRARY_PATH"] = ":".join(
            weston_libraries + ([existing] if existing else [])
        )
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
    state.track(parent, [Path(arguments.weston)])
    wait_for_path(runtime / parent_socket, state, 15)

    # -- child compositor, windowed on the parent, plus the two applications
    compositor_environment = dict(environment)
    compositor_environment["WAYLAND_DISPLAY"] = parent_socket
    compositor_environment["QT_FORCE_STDERR_LOGGING"] = "1"
    # AGENT-NOTE: KWIN_COMPOSE=O2 was tried and reverted — the --virtual
    # platform logs "Configured compositor not supported by Platform.
    # Falling back to defaults" and silently keeps QPainter regardless, so
    # forcing GL2 is a no-op there. The parent-Weston route above is what
    # actually makes the screencast global appear.
    compositor = spawn_logged_process(
        "compositor",
        [str(stage.executables["launcher"]), "--plugin-root", str(stage.compositor_plugin.parents[2]),
         "--kwin", arguments.kwin_wayland, "--windowed",
         "--width", str(virtual.logical_width), "--height", str(virtual.logical_height),
         "--scale", str(virtual.scale), "--output-count", str(virtual.output_count),
         "--socket", socket_name,
         "--test-scenario", "/opt/qindaqt-source/tests/scenarios/single-1080p.json",
         "--session", str(stage.executables["session"])],
        compositor_environment,
    )
    state.track(compositor, [stage.executables["launcher"], Path(arguments.kwin_wayland)])
    wait_for_path(runtime / socket_name, state, 20)

    app_environment = dict(environment)
    app_environment["WAYLAND_DISPLAY"] = socket_name
    app_environment["QT_LINUX_ACCESSIBILITY_ALWAYS_ON"] = "1"
    if "SHELL" not in app_environment:
        app_environment["SHELL"] = "/usr/bin/bash"
    terminal_executable = Path("/opt/qindaqt-terminal") / arguments.bin_directory / "qindaqt-terminal"
    terminal_process = spawn_logged_process("terminal-app", [str(terminal_executable)], app_environment)
    state.track(terminal_process, [terminal_executable])
    # The editor is the second real member for the grouped-member case; the
    # proof never delivers into it, it only gives the container a peer.
    editor_executable = stage.executables["editor-app"]
    editor_process = spawn_logged_process("editor-app", [str(editor_executable)], app_environment)
    state.track(editor_process, [editor_executable])

    from gabbee_terminal_portal import wait_for_window

    terminal_window = wait_for_window(app_environment, TERMINAL_APP)
    editor_window = wait_for_window(app_environment, EDITOR_APP)
    document.add(PhaseResult(
        "terminal-focus", True, "Terminal window enumerated on the nested compositor",
        {"window": terminal_window, "editorWindow": editor_window},
    ))

    return remote_desktop_delivery(
        app_environment, state, document, terminal_window, editor_window
    )


def _start_private_bus(arguments: argparse.Namespace, stage, environment: dict, state, runtime: Path) -> None:
    from desktop_session_process import spawn_logged_process, wait_for_path
    from gabbee_probe_support import BUS_CONFIG_TEMPLATE

    # Probe-owned service dir stays empty: every process below is spawned and
    # named explicitly, never D-Bus activated, so activation cannot race a
    # stray host backend into this private bus.
    service_dir = runtime / "bus-services"
    service_dir.mkdir(parents=True, exist_ok=True)
    bus_config = runtime / "bus.conf"
    bus_config.write_text(
        BUS_CONFIG_TEMPLATE.format(address=f"unix:path={runtime / 'bus'}", service_dir=service_dir),
        encoding="utf-8",
    )
    environment["DBUS_SESSION_BUS_ADDRESS"] = f"unix:path={runtime / 'bus'}"
    bus = spawn_logged_process(
        "dbus-daemon", [arguments.dbus_daemon, "--config-file", str(bus_config), "--nofork", "--nopidfile"],
        environment,
    )
    state.track(bus, [arguments.dbus_daemon])
    wait_for_path(runtime / "bus", state, 10)
    for role in ("settings-service", "audio-service"):
        child = spawn_logged_process(role, [str(stage.executables[role])], environment)
        state.track(child, [stage.executables[role]])


def _start_private_pipewire(environment: dict, state) -> None:
    from desktop_session_process import spawn_logged_process
    from gabbee_terminal_portal import wait_for_private_pipewire

    # AGENT-GUARD: starting PipeWire after the compositor leaves its first
    # connection attempt permanently failed — the plugin's later D-Bus
    # Unload/LoadPlugin cycle re-registers the plugin but does not retry the
    # PipeWire connection, so the KDE portal backend never sees
    # zkde_screencast_unstable_v1 and denies CreateSession outright. Found by
    # tracing "kwin_screencast: Failed to connect PipeWire context" to the
    # very first lines of the compositor's own log, before PipeWire existed.
    pipewire_env = dict(environment)
    pipewire_env["PIPEWIRE_RUNTIME_DIR"] = pipewire_env["XDG_RUNTIME_DIR"]
    pipewire_env.pop("PIPEWIRE_REMOTE", None)
    pipewire_env["XDG_STATE_HOME"] = tempfile.mkdtemp(prefix="qindaqt-pipewire-state-")
    pipewire_env["XDG_CONFIG_HOME"] = tempfile.mkdtemp(prefix="qindaqt-pipewire-config-")
    Path(pipewire_env["XDG_STATE_HOME"]).mkdir(parents=True, exist_ok=True)
    Path(pipewire_env["XDG_CONFIG_HOME"]).mkdir(parents=True, exist_ok=True)
    pipewire_evidence: dict = {}
    pipewire = spawn_logged_process(
        "pipewire", ["/usr/bin/pipewire", "-c", "/usr/share/pipewire/pipewire.conf"], pipewire_env,
    )
    state.track(pipewire, [Path("/usr/bin/pipewire")])
    wait_for_private_pipewire(pipewire_env, pipewire, pipewire_evidence)


def preflight_phase():
    from gabbee_probe_support import PhaseResult
    from gabbee_terminal_sink import TerminalSinkError, import_real_sink

    facts = {
        "pipewireAvailable": Path("/usr/bin/pipewire").is_file(),
        "portalBackendAvailable": Path("/usr/libexec/xdg-desktop-portal-kde").is_file(),
        "portalFrontendAvailable": Path("/usr/libexec/xdg-desktop-portal").is_file(),
        "agentInputAvailable": Path("/usr/bin/qindaqt-agent-input").is_file(),
        "atspiBusLauncherAvailable": Path("/usr/libexec/at-spi-bus-launcher").is_file(),
    }
    # AGENT-GUARD: fail preflight, not the delivery phase, when Gabbee's real
    # sink is unreachable. A missing sink must never be silently replaced by
    # this repository's own helper wire protocol, which would turn the
    # integration proof back into a direct-helper proof.
    try:
        sink_class = import_real_sink()
        facts["gabbeeSinkImport"] = f"{sink_class.__module__}.{sink_class.__qualname__}"
        facts["gabbeeSinkImportable"] = True
    except TerminalSinkError as error:
        facts["gabbeeSinkImportable"] = False
        facts["gabbeeSinkImportError"] = str(error)
    ok = all(value for key, value in facts.items() if isinstance(value, bool))
    return PhaseResult(
        "preflight", ok,
        "real-portal binaries and Gabbee's real text sink are present"
        if ok else "a real-portal binary or Gabbee's real text sink is missing",
        facts,
    )
