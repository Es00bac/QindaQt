# SPDX-License-Identifier: GPL-3.0-or-later
"""Real-portal delivery and PTY readback for the Terminal PTY proof.

Owns the KDE service-cache fixture, the portal backend/frontend lifecycle,
the approved RemoteDesktop sessions, the sentinel delivery, and the
byte-exact readback that is the actual proof.

Two delivery routes run per case, and the split is deliberate:

- Gabbee's real ``AgentInputTextSink`` delivers the **sentinel itself**. This
  is the integration being proven: Gabbee's own production sink, its own
  helper lifecycle, and its own ACK semantics put the text into the PTY.
- A separately approved direct helper session supplies only what that sink
  structurally cannot: the pointer click that focuses the Terminal, and the
  Return and Ctrl-D strokes that frame the ``cat`` redirection. Gabbee's
  sink answers ``deliver_key`` with a failure by design — it is a text sink —
  so those strokes have no sink route to take.

AGENT-GUARD: if the sentinel is ever moved back onto the direct helper this
stops being a Gabbee integration proof. The sink's DeliveryResult is
recorded as evidence but is still not the proof; the readback is.
"""

from __future__ import annotations

import base64
import json
import os
import shutil
import subprocess
import time
from pathlib import Path

from gabbee_terminal_sentinel import (
    expected_sentinel_bytes,
    generate_sentinel,
    sentinel_output_filename,
)

CHOOSER_SUBSTRING = "terminal"
CHOOSER_APP = "org.qindaqt.Terminal"
KEYSYM_RETURN = 0xFF0D
KEYSYM_CONTROL_L = 0xFFE3
KEYSYM_LOWER_D = 0x64
EVIDENCE_ROOT = Path("/var/lib/qindaqt-evidence")


def refresh_service_cache(environment: dict, evidence: dict) -> None:
    """Build the KDE service cache so KWin can resolve the portal backend.

    AGENT-GUARD: KWin grants privileged Wayland interfaces (here
    ``zkde_screencast_unstable_v1``, which xdg-desktop-portal-kde requires
    inside CreateSession) only to a client whose desktop file it can resolve
    through KApplicationTrader. Without an XDG applications menu and a built
    sycoca cache the lookup returns zero entries even though
    /usr/share/applications/org.freedesktop.impl.portal.desktop.kde.desktop
    exists, the interface is withheld, and every RemoteDesktop session is
    denied. Mirrors the accepted qualification hook's fixture.
    """

    menus = Path(environment["XDG_CONFIG_HOME"]) / "menus"
    menus.mkdir(parents=True, exist_ok=True)
    (menus / "applications.menu").write_text(
        '<!DOCTYPE Menu PUBLIC "-//freedesktop//DTD Menu 1.0//EN" '
        '"http://www.freedesktop.org/standards/menu-spec/1.0/menu.dtd">'
        "<Menu><Name>Applications</Name><DefaultAppDirs/><Include><All/></Include></Menu>",
        encoding="utf-8",
    )
    cache_tool = shutil.which("kbuildsycoca6", path=environment.get("PATH"))
    if cache_tool is None:
        evidence["serviceCacheRefresh"] = {"error": "kbuildsycoca6 not found on PATH"}
        return
    cache = subprocess.run(
        [cache_tool, "--noincremental"], env=dict(environment),
        capture_output=True, text=True, timeout=30,
    )
    evidence["serviceCacheRefresh"] = {
        "returnCode": cache.returncode, "stderr": cache.stderr[-1500:],
    }


def remote_desktop_delivery(
    app_environment: dict, state, document, terminal_window, editor_window
) -> int:
    from desktop_session_process import spawn_logged_process
    from gabbee_probe_support import PhaseResult
    import gabbee_terminal_portal as portal

    EVIDENCE_ROOT.mkdir(parents=True, exist_ok=True)
    backend_env = dict(app_environment)
    backend_env["XDG_CURRENT_DESKTOP"] = "KDE"
    backend_env["XDG_SESSION_DESKTOP"] = "KDE"
    backend_env["QT_ACCESSIBILITY"] = "1"
    backend_env["QT_LINUX_ACCESSIBILITY_ALWAYS_ON"] = "1"
    # AGENT-NOTE: get the backend's own reason for a CreateSession denial
    # instead of inferring it from the one-line screencast warning alone.
    backend_env["QT_LOGGING_RULES"] = "xdp-kde-remotedesktop.debug=true"
    frontend_env = dict(app_environment)
    frontend_env["QT_ACCESSIBILITY"] = "1"
    frontend_env["QT_LINUX_ACCESSIBILITY_ALWAYS_ON"] = "1"
    frontend_env["LIBGL_ALWAYS_SOFTWARE"] = "1"

    # AGENT-NOTE: PipeWire is already running (started before the compositor
    # in gabbee_terminal_boot), so KWin's screencast plugin connected on its
    # first auto-load attempt. Reloading the plugin here is a defensive
    # no-op, not the fix.
    approval_evidence: dict = {}
    refresh_service_cache(frontend_env, approval_evidence)
    portal.reload_screencast_plugin(frontend_env, approval_evidence)

    a11y = spawn_logged_process(
        "at-spi-bus-launcher", ["/usr/libexec/at-spi-bus-launcher", "--launch-immediately", "--a11y=1"],
        frontend_env,
    )
    state.track(a11y, [Path("/usr/libexec/at-spi-bus-launcher")])
    time.sleep(0.25)

    start_portal_pair(backend_env, frontend_env, state)

    standalone = portal_session_delivery(
        app_environment, frontend_env, state, document, terminal_window,
        "standalone", approval_evidence,
        restart=lambda: start_portal_pair(backend_env, frontend_env, state),
    )
    if standalone != 0:
        return standalone

    # -- grouped member: the Terminal becomes a real container member beside
    # the Editor, then the whole real-portal delivery is repeated with a
    # second fresh sentinel so a grouped member is proven, not inferred.
    dock_evidence: dict = {}
    try:
        dock = portal.dock_windows(
            app_environment, editor_window["id"], terminal_window["id"]
        )
    except portal.TerminalPortalError as error:
        document.add(PhaseResult("grouped-member-dock", False, str(error), dock_evidence))
        return 1
    dock_evidence["dockResponse"] = dock
    docked = dock.get("status") == "docked"
    document.add(PhaseResult(
        "grouped-member-dock", docked,
        "Terminal grouped with the Editor through Compositor1.DockWindows"
        if docked else f"DockWindows did not group the members: {dock}",
        dock_evidence,
    ))
    if not docked:
        return 1
    time.sleep(1.0)  # let the container settle before re-reading geometry
    grouped_window = portal.wait_for_window(app_environment, CHOOSER_APP)
    grouped_evidence: dict = {"groupedWindow": grouped_window}
    return portal_session_delivery(
        app_environment, frontend_env, state, document, grouped_window,
        "group-member", grouped_evidence,
        restart=lambda: start_portal_pair(backend_env, frontend_env, state),
    )


def start_portal_pair(backend_env: dict, frontend_env: dict, state) -> None:
    """(Re)start the real KDE backend and the portal frontend, in that order.

    AGENT-GUARD: xdg-desktop-portal-kde probes zkde_screencast_unstable_v1
    exactly once, at startup, and latches the result for its whole lifetime —
    a backend that starts before KWin advertises the global denies every
    later CreateSession with no retry of its own. Restarting the backend is
    therefore the only way to recover, and the frontend is restarted with it
    so it re-resolves the new backend owner.
    """

    from desktop_session_process import spawn_logged_process
    import gabbee_terminal_portal as portal

    backend = spawn_logged_process(
        "xdg-desktop-portal-kde", ["/usr/libexec/xdg-desktop-portal-kde", "--replace"], backend_env,
    )
    state.track(backend, [Path("/usr/libexec/xdg-desktop-portal-kde")])
    portal.wait_for_remote_desktop_backend(backend_env, backend.pid)

    frontend = spawn_logged_process(
        "xdg-desktop-portal", ["/usr/libexec/xdg-desktop-portal", "--replace"], frontend_env,
    )
    state.track(frontend, [Path("/usr/libexec/xdg-desktop-portal")])
    portal.wait_for_remote_desktop_frontend(frontend_env, frontend.pid)


def portal_session_delivery(
    app_environment: dict, frontend_env: dict, state, document, window,
    label: str, approval_evidence: dict, restart=None, attempts: int = 3,
) -> int:
    """One complete case: framing helper session, sink delivery, readback."""

    from gabbee_probe_support import PhaseResult
    import gabbee_terminal_portal as portal

    approval_evidence.setdefault("portalSessionAttempts", [])
    helper = None
    origin = None
    for attempt in range(1, attempts + 1):
        helper = subprocess.Popen(
            ["/usr/bin/qindaqt-agent-input", "--devices", "pointer,keyboard"],
            env=frontend_env, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True,
        )
        state.track(helper, [Path("/usr/bin/qindaqt-agent-input")])
        try:
            origin = portal.approve_remote_desktop_dialog(
                app_environment, approval_evidence, CHOOSER_SUBSTRING, helper,
            )
            break
        except portal.TerminalPortalError as error:
            approval_evidence["portalSessionAttempts"].append(
                {"attempt": attempt, "error": str(error)[:300]}
            )
            if attempt == attempts or restart is None:
                document.add(
                    PhaseResult(f"portal-approval-{label}", False, str(error), approval_evidence)
                )
                return 1
            # The backend latched an unavailable screencast global; restart it
            # and give the compositor a moment before the next attempt.
            time.sleep(2.0)
            restart()
    helper_ready = portal.wait_helper_ready(helper, approval_evidence)
    document.add(PhaseResult(
        f"portal-approval-{label}", helper_ready,
        "real KDE RemoteDesktop dialog approved and framing helper published READY"
        if helper_ready else "framing helper did not publish READY after approval",
        approval_evidence,
    ))
    if not helper_ready or helper is None or helper.stdin is None:
        return 1

    return deliver_and_read_back(
        app_environment, frontend_env, window, helper, origin, document, label
    )


def _send(helper: subprocess.Popen[str], event: dict) -> None:
    assert helper.stdin is not None
    helper.stdin.write(json.dumps(event) + "\n")
    helper.stdin.flush()


def _close_helper(helper: subprocess.Popen[str]) -> None:
    try:
        _send(helper, {"action": "close"})
        if helper.stdin is not None:
            helper.stdin.close()
        helper.wait(timeout=15)
    except (BrokenPipeError, OSError, subprocess.TimeoutExpired):
        helper.terminate()


def deliver_and_read_back(
    app_environment, frontend_env, terminal_window, helper, origin, document, label: str,
) -> int:
    from gabbee_probe_support import PhaseResult
    import gabbee_terminal_portal as portal
    import gabbee_terminal_sink as sink_module

    run_id = app_environment.get("QINDAQT_SESSION_RUN_ID", "terminal-pty")
    # AGENT-GUARD: each case gets its own fresh sentinel and output file, so a
    # grouped-member pass can never be satisfied by the standalone artifact.
    scoped = f"{run_id}-{label}"
    sentinel = generate_sentinel(scoped)
    output_path = EVIDENCE_ROOT / sentinel_output_filename(scoped)

    target = portal.window_center(terminal_window["geometry"])
    dx, dy = target[0] - origin[0], target[1] - origin[1]
    evidence: dict = {"sentinel": sentinel, "outputPath": str(output_path)}

    # -- framing only: focus the Terminal and open the redirection. The
    # sentinel is deliberately NOT sent on this session.
    try:
        _send(helper, {"action": "move", "dx": dx, "dy": dy})
        _send(helper, {"action": "press", "button": "left"})
        _send(helper, {"action": "release", "button": "left"})
        time.sleep(0.3)
        _send(helper, {"action": "text", "text": f"cat > {output_path}"})
        _send(helper, {"action": "key_press", "keysym": KEYSYM_RETURN})
        _send(helper, {"action": "key_release", "keysym": KEYSYM_RETURN})
        time.sleep(0.5)
    except (BrokenPipeError, OSError) as error:
        evidence["framingError"] = str(error)
        document.add(PhaseResult(
            f"delivery-{label}", False, f"framing helper failed before sink delivery: {error}",
            evidence,
        ))
        return 1

    # -- the proof's payload: Gabbee's real sink types the sentinel.
    sink_ok, sink_evidence = _deliver_sentinel_through_gabbee(
        sentinel, app_environment, frontend_env, sink_module, portal
    )
    evidence["gabbeeSink"] = sink_evidence
    document.add(PhaseResult(
        f"gabbee-sink-delivery-{label}", sink_ok,
        "Gabbee's real AgentInputTextSink acknowledged the sentinel through the portal"
        if sink_ok else "Gabbee's real AgentInputTextSink did not acknowledge the sentinel",
        sink_evidence,
    ))
    if not sink_ok:
        _close_helper(helper)
        return 1

    # -- framing again: flush the line and close the redirection.
    try:
        _send(helper, {"action": "key_press", "keysym": KEYSYM_RETURN})
        _send(helper, {"action": "key_release", "keysym": KEYSYM_RETURN})
        time.sleep(0.3)
        _send(helper, {"action": "key_press", "keysym": KEYSYM_CONTROL_L})
        _send(helper, {"action": "key_press", "keysym": KEYSYM_LOWER_D})
        _send(helper, {"action": "key_release", "keysym": KEYSYM_LOWER_D})
        _send(helper, {"action": "key_release", "keysym": KEYSYM_CONTROL_L})
        _close_helper(helper)
    except (BrokenPipeError, OSError, subprocess.TimeoutExpired) as error:
        evidence["framingError"] = str(error)
        document.add(PhaseResult(
            f"delivery-{label}", False, f"framing helper failed after sink delivery: {error}",
            evidence,
        ))
        return 1
    evidence["framingHelperReturnCode"] = helper.returncode
    # AGENT-GUARD: an ACK or exit code proves the portal accepted the events,
    # never that the Terminal received them. This phase is not the proof.
    document.add(PhaseResult(
        f"delivery-{label}", helper.returncode == 0,
        "framing strokes delivered and the sink's sentinel acknowledged through the portal",
        evidence,
    ))
    if helper.returncode != 0:
        return 1

    return read_back(sentinel, output_path, document, label)


def _deliver_sentinel_through_gabbee(
    sentinel: str, app_environment: dict, frontend_env: dict, sink_module, portal
) -> tuple[bool, dict]:
    """Drive Gabbee's real sink for one sentinel, approving its own dialog.

    AGENT-NOTE: the sink spawns its helper with the interpreter's own
    environment, so the portal-facing variables are exported into os.environ
    first. They are the same values already used for every other portal
    client in this run.
    """

    evidence: dict = {}
    for key in ("WAYLAND_DISPLAY", "DBUS_SESSION_BUS_ADDRESS", "XDG_RUNTIME_DIR",
                "QT_ACCESSIBILITY", "QT_LINUX_ACCESSIBILITY_ALWAYS_ON",
                "LIBGL_ALWAYS_SOFTWARE", "XDG_CONFIG_HOME", "XDG_DATA_DIRS", "PATH"):
        if key in frontend_env:
            os.environ[key] = frontend_env[key]
    os.environ[sink_module.SINK_COMMAND_ENVIRONMENT] = sink_module.DEFAULT_SINK_COMMAND
    evidence["sinkCommand"] = sink_module.sink_command()

    try:
        sink = sink_module.build_sink()
    except sink_module.TerminalSinkError as error:
        evidence["error"] = str(error)
        return False, evidence
    evidence["sinkClass"] = f"{type(sink).__module__}.{type(sink).__qualname__}"
    # The sink is a text sink; record its own refusal of key chords so the
    # framing session's separate existence is justified by evidence.
    key_result = sink.deliver_key("Return")
    evidence["sinkKeyChordRefusal"] = sink_module.delivery_result_evidence(key_result)

    approval_evidence: dict = {}

    def approve():
        return portal.approve_remote_desktop_dialog(
            app_environment, approval_evidence, CHOOSER_SUBSTRING, None,
        )

    try:
        result, error, _approval = sink_module.deliver_with_concurrent_approval(
            sink, sentinel, approve
        )
    except portal.TerminalPortalError as approval_error:
        evidence["approvalError"] = str(approval_error)
        evidence["sinkApproval"] = approval_evidence
        sink.close()
        return False, evidence
    evidence["sinkApproval"] = approval_evidence
    if error is not None or result is None:
        evidence["error"] = error or "sink returned no DeliveryResult"
        sink.close()
        return False, evidence
    evidence["deliveryResult"] = sink_module.delivery_result_evidence(result)
    sink.close()
    return bool(getattr(result, "ok", False)), evidence


def read_back(sentinel: str, output_path: Path, document, label: str) -> int:
    from gabbee_probe_support import PhaseResult

    expected = expected_sentinel_bytes(sentinel)
    deadline = time.monotonic() + 15.0
    actual: bytes | None = None
    while time.monotonic() < deadline:
        if output_path.is_file():
            actual = output_path.read_bytes()
            if actual == expected or time.monotonic() + 0.5 > deadline:
                break
        time.sleep(0.2)
    matched = actual == expected
    document.add(PhaseResult(
        f"readback-{label}", matched,
        "byte-exact sentinel readback from the shell-written file"
        if matched else "readback bytes did not match the exact sentinel sent",
        {
            "expectedBytesBase64": base64.b64encode(expected).decode("ascii"),
            "actualBytesBase64": base64.b64encode(actual).decode("ascii") if actual is not None else None,
            "outputFileExists": output_path.is_file(),
        },
    ))
    # AGENT-GUARD: this phase's ok flag is the proof this whole module exists
    # to produce. A sink ACK or helper exit code must never substitute for
    # this byte comparison against the file the Terminal's own shell wrote.
    return 0 if matched else 1
