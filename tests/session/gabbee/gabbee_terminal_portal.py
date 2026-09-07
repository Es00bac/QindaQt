# SPDX-License-Identifier: GPL-3.0-or-later
"""Real-portal RemoteDesktop helpers reused by the Terminal PTY delivery proof.

This module adapts the accepted "real portal" pattern from the visual harness
qualification hook (``.cache/welcome-visual/agent_input_qindaqt_qualification_hook.py``,
copied from a repo-external private harness and not importable from here): a
private PipeWire core is started on the run's own runtime, KWin's public
screencast plugin is reloaded against it, the installed KDE
``xdg-desktop-portal-kde`` backend is started on the private bus with process
identity ``KDE`` while the ``xdg-desktop-portal`` frontend and every other
process keep the QindaQt identity, and the private
``org.qindaqt.Compositor1.InjectTestInput`` development method is used only to
click the real KDE portal *approval dialog* (never to deliver the proof
payload itself). After approval, the installed ``qindaqt-agent-input`` helper
is the sole source of pointer/keyboard events, delivered through the genuine
RemoteDesktop portal session.

AGENT-CONTRACT: nothing here touches a host D-Bus address, host Wayland
socket, or host input device. Every helper takes an explicit environment
mapping bound to the private run.
"""

from __future__ import annotations

import json
import re
import subprocess
import time
from pathlib import Path
from typing import Any, Mapping

BUS = "org.qindaqt.Compositor"
OBJECT = "/org/qindaqt/Compositor"
IFACE = "org.qindaqt.Compositor1"
POLL_SECONDS = 0.15
DEFAULT_TIMEOUT_SECONDS = 20.0
# The portal's window/device chooser is rendered by the KDE backend process;
# its dialog frame is one of this application's live compositor windows.
PORTAL_BACKEND_APPLICATION_ID = "org.freedesktop.impl.portal.desktop.kde"


class TerminalPortalError(RuntimeError):
    pass


def _call(environment: Mapping[str, str], method: str, *args: str) -> Any:
    result = subprocess.run(
        ["/usr/bin/gdbus", "call", "--session", "--dest", BUS,
         "--object-path", OBJECT, "--method", f"{IFACE}.{method}", *args],
        env=dict(environment), capture_output=True, text=True, timeout=5,
    )
    if result.returncode:
        raise TerminalPortalError(f"{method} failed: {result.stderr.strip()}")
    values = re.findall(r"0x([0-9a-fA-F]{2})", result.stdout)
    if not values:
        raise TerminalPortalError(f"{method} returned no JSON byte array: {result.stdout!r}")
    return json.loads(bytes(int(value, 16) for value in values).decode("utf-8"))


def _events(environment: Mapping[str, str], events: list[dict[str, Any]]) -> None:
    payload = json.dumps({"schemaVersion": 1, "events": events}).encode("utf-8")
    _call(environment, "InjectTestInput", "[" + ",".join(str(byte) for byte in payload) + "]")


def click_dev_input(environment: Mapping[str, str], x: float, y: float) -> None:
    """Click through the private dev-only injector; approval dialogs only."""

    _events(environment, [
        {"type": "pointer-absolute", "x": round(x, 1), "y": round(y, 1)},
        {"type": "button", "button": "left", "pressed": True},
        {"type": "button", "button": "left", "pressed": False},
    ])


def windows(environment: Mapping[str, str]) -> list[dict[str, Any]]:
    return list(_call(environment, "Windows").get("windows", []))


def window_center(rect: Mapping[str, Any]) -> tuple[float, float]:
    return (float(rect["x"]) + float(rect["width"]) / 2,
            float(rect["y"]) + float(rect["height"]) / 2)


def wait_for_window(
    environment: Mapping[str, str], desktop_id: str, *, timeout: float = DEFAULT_TIMEOUT_SECONDS
) -> dict[str, Any]:
    deadline = time.monotonic() + timeout
    last: list[dict[str, Any]] = []
    last_error: TerminalPortalError | None = None
    while time.monotonic() < deadline:
        try:
            last = windows(environment)
        except TerminalPortalError as error:
            # AGENT-GUARD: the compositor's Wayland socket appears before it
            # finishes registering org.qindaqt.Compositor on the session bus;
            # a transient ServiceUnknown here is expected startup timing, not
            # a fatal condition, so keep polling instead of propagating.
            last_error = error
            time.sleep(POLL_SECONDS)
            continue
        matches = [row for row in last if row.get("applicationId") == desktop_id]
        if len(matches) == 1:
            return matches[0]
        time.sleep(POLL_SECONDS)
    if last_error is not None and not last:
        raise TerminalPortalError(
            f"compositor Windows() never became reachable: {last_error}"
        )
    raise TerminalPortalError(f"expected exactly one {desktop_id!r} window, got {last!r}")


def _wait_portal_interface(
    environment: Mapping[str, str], destination: str, required_interface: str, expected_pid: int,
    *, timeout: float = DEFAULT_TIMEOUT_SECONDS,
) -> None:
    """Wait until a private portal process has registered its advertised API."""

    deadline = time.monotonic() + timeout
    last_error = "not queried"
    while time.monotonic() < deadline:
        owner = subprocess.run(
            ["/usr/bin/gdbus", "call", "--session", "--dest", "org.freedesktop.DBus",
             "--object-path", "/org/freedesktop/DBus", "--method",
             "org.freedesktop.DBus.GetConnectionUnixProcessID", destination],
            env=dict(environment), capture_output=True, text=True, timeout=3,
        )
        match = re.search(r"uint32 (\d+)", owner.stdout)
        if not match or int(match.group(1)) != expected_pid:
            last_error = f"expected owner PID {expected_pid}, got {owner.stdout.strip()!r}"
            time.sleep(POLL_SECONDS)
            continue
        result = subprocess.run(
            ["/usr/bin/gdbus", "introspect", "--session", "--dest", destination,
             "--object-path", "/org/freedesktop/portal/desktop"],
            env=dict(environment), capture_output=True, text=True, timeout=3,
        )
        if result.returncode == 0 and required_interface in result.stdout:
            return
        last_error = (result.stderr or result.stdout).strip()
        time.sleep(POLL_SECONDS)
    raise TerminalPortalError(
        f"private portal {destination} did not expose {required_interface}; "
        f"last introspection error={last_error!r}"
    )


def wait_for_remote_desktop_backend(environment: Mapping[str, str], backend_pid: int) -> None:
    _wait_portal_interface(
        environment, PORTAL_BACKEND_APPLICATION_ID,
        "org.freedesktop.impl.portal.RemoteDesktop", backend_pid,
    )


def wait_for_remote_desktop_frontend(environment: Mapping[str, str], frontend_pid: int) -> None:
    _wait_portal_interface(
        environment, "org.freedesktop.portal.Desktop",
        "org.freedesktop.portal.RemoteDesktop", frontend_pid,
    )


def wait_for_private_pipewire(
    environment: Mapping[str, str], process: subprocess.Popen[str], evidence: dict[str, Any],
    *, timeout: float = 12.0,
) -> None:
    """Wait for the private PipeWire core KWin's screencast plugin needs."""

    runtime = Path(environment["XDG_RUNTIME_DIR"])
    socket = runtime / "pipewire-0"
    deadline = time.monotonic() + timeout
    last_error = "socket has not appeared"
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise TerminalPortalError(
                f"private PipeWire exited before readiness: returncode={process.returncode}"
            )
        if socket.exists():
            probe = subprocess.run(
                ["/usr/bin/pw-cli", "-r", "pipewire-0", "info", "0"],
                env=dict(environment), capture_output=True, text=True, timeout=3,
            )
            if probe.returncode == 0:
                evidence["pipewireSocket"] = str(socket)
                evidence["pipewireReady"] = True
                return
            last_error = (probe.stderr or probe.stdout).strip()
        time.sleep(POLL_SECONDS)
    raise TerminalPortalError(f"private PipeWire did not become ready: {last_error!r}")


def _kwin_plugin_call(
    environment: Mapping[str, str], method: str, plugin: str | None = None
) -> subprocess.CompletedProcess[str]:
    args = ["/usr/bin/gdbus", "call", "--session", "--dest", "org.kde.KWin",
            "--object-path", "/Plugins", "--method", f"org.kde.KWin.Plugins.{method}"]
    if plugin is not None:
        args.append(plugin)
    return subprocess.run(args, env=dict(environment), capture_output=True, text=True, timeout=5)


def reload_screencast_plugin(environment: Mapping[str, str], evidence: dict[str, Any]) -> None:
    """Restart KWin's public screencast plugin after PipeWire is available."""

    loaded = subprocess.run(
        ["/usr/bin/gdbus", "call", "--session", "--dest", "org.kde.KWin",
         "--object-path", "/Plugins", "--method",
         "org.freedesktop.DBus.Properties.Get", "org.kde.KWin.Plugins", "LoadedPlugins"],
        env=dict(environment), capture_output=True, text=True, timeout=5,
    )
    evidence["kwinPluginsBeforeReload"] = loaded.stdout.strip()
    if "screencast" in loaded.stdout:
        unload = _kwin_plugin_call(environment, "UnloadPlugin", "screencast")
        if unload.returncode:
            raise TerminalPortalError(
                "KWin public screencast plugin unload failed: " + unload.stderr.strip()
            )
    load = _kwin_plugin_call(environment, "LoadPlugin", "screencast")
    if load.returncode or "true" not in load.stdout.lower():
        raise TerminalPortalError(
            "KWin public screencast plugin reload failed: " + load.stderr.strip()
        )
    loaded_after = subprocess.run(
        ["/usr/bin/gdbus", "call", "--session", "--dest", "org.kde.KWin",
         "--object-path", "/Plugins", "--method",
         "org.freedesktop.DBus.Properties.Get", "org.kde.KWin.Plugins", "LoadedPlugins"],
        env=dict(environment), capture_output=True, text=True, timeout=5,
    )
    evidence["kwinPluginsAfterReload"] = loaded_after.stdout.strip()
    if "screencast" not in loaded_after.stdout:
        raise TerminalPortalError("KWin did not report screencast loaded after reload")


def atspi_nodes() -> list[Any]:
    """Return the current AT-SPI tree of the private accessibility broker."""

    try:
        import gi
        gi.require_version("Atspi", "2.0")
        from gi.repository import Atspi, GLib
        Atspi.init()
        context = GLib.MainContext.default()
        for _ in range(100):
            if not context.pending():
                break
            context.iteration(False)
        desktop = Atspi.get_desktop(0)
    except Exception as error:  # pragma: no cover - depends on the private image
        raise TerminalPortalError(f"private AT-SPI tree unavailable: {error}") from error
    nodes: list[Any] = []
    seen: set[int] = set()

    def visit(node: Any) -> None:
        identity = id(node)
        if identity in seen:
            return
        seen.add(identity)
        nodes.append(node)
        try:
            count = int(node.get_child_count())
        except Exception:
            return
        for index in range(count):
            try:
                child = node.get_child_at_index(index)
            except Exception:
                continue
            if child is not None:
                visit(child)

    visit(desktop)
    return nodes


def node_name(node: Any) -> str:
    try:
        return str(node.get_name() or "")
    except Exception:
        return ""


def node_role(node: Any) -> str:
    try:
        return str(node.get_role_name() or "").lower()
    except Exception:
        return ""


def node_rect(node: Any) -> tuple[float, float, float, float] | None:
    try:
        import gi
        gi.require_version("Atspi", "2.0")
        from gi.repository import Atspi
        rect = node.get_extents(Atspi.CoordType.SCREEN)
        if rect.width <= 0 or rect.height <= 0:
            return None
        return (float(rect.x), float(rect.y), float(rect.width), float(rect.height))
    except Exception:
        return None


def node_checked(node: Any) -> bool:
    try:
        import gi
        gi.require_version("Atspi", "2.0")
        from gi.repository import Atspi
        return bool(node.get_state_set().contains(Atspi.StateType.CHECKED))
    except Exception:
        return False


def chooser_matches(name: str, substring: str) -> bool:
    """Pure predicate for the portal chooser's window/app entry match."""

    return substring.casefold() in name.casefold()


def click_accessible(environment: Mapping[str, str], node: Any) -> tuple[float, float]:
    rect = node_rect(node)
    if rect is None:
        raise TerminalPortalError(f"portal control {node_name(node)!r} has no screen bounds")
    x, y, width, height = rect
    # Qt Wayland reports client-local SCREEN bounds. Recover the private
    # compositor origin from this node's frame and its public window geometry.
    parent = node
    for _ in range(30):
        if node_role(parent) in {"frame", "dialog"}:
            frame = node_rect(parent)
            matches = [row for row in windows(environment)
                       if row.get("applicationId") == PORTAL_BACKEND_APPLICATION_ID
                       and row.get("title") == node_name(parent)]
            if frame and frame[0] == 0 and frame[1] == 0 and len(matches) == 1:
                geometry = matches[0]["geometry"]
                border = (float(geometry["width"]) - frame[2]) / 2
                x += float(geometry["x"]) + border
                y += float(geometry["y"]) + float(geometry["height"]) - frame[3] - border
            break
        try:
            parent = parent.get_parent()
        except Exception:
            break
        if parent is None:
            break
    click_dev_input(environment, x + width / 2, y + height / 2)
    return (x + width / 2, y + height / 2)


def approve_remote_desktop_dialog(
    environment: Mapping[str, str], evidence: dict[str, Any], chooser_substring: str,
    helper: subprocess.Popen[str] | None = None, *, timeout: float = DEFAULT_TIMEOUT_SECONDS,
) -> tuple[float, float]:
    """Select the target window/app and approve the real KDE RemoteDesktop dialog.

    Mirrors the qualification hook's ``_approve_private_dialog`` with a
    configurable chooser label instead of the fixed "text editor" target.
    Never invokes an AT-SPI action on Accept/Allow/Share: those controls are
    clicked by their real screen bounds through the development-only input
    injector, exactly like a human approving the dialog.
    """

    last_names: list[str] = []
    deadline = time.monotonic() + timeout
    pointer_origin: tuple[float, float] | None = None
    evidence.setdefault("privateDevInput", [])
    while time.monotonic() < deadline:
        if helper is not None and helper.poll() is not None:
            evidence["helperExitedDuringApproval"] = {
                "returnCode": helper.returncode,
                "stdout": helper.stdout.read() if helper.stdout is not None else "",
                "stderr": helper.stderr.read() if helper.stderr is not None else "",
            }
            raise TerminalPortalError(
                "agent-input exited before approval controls appeared; "
                f"returncode={helper.returncode}"
            )
        nodes = atspi_nodes()
        last_names = [node_name(node) for node in nodes if node_name(node)]
        choices = [
            node for node in nodes
            if chooser_matches(node_name(node), chooser_substring)
            and any(token in node_role(node) for token in ("list", "radio", "check", "combo"))
        ]
        target_selected = False
        for node in choices:
            if node_checked(node):
                target_selected = True
                break
            pointer_origin = click_accessible(environment, node)
            target_selected = True
            evidence["privateDevInput"].append({"action": "select-target", "name": node_name(node)})
            break
        for node in nodes:
            name = node_name(node).lower()
            role = node_role(node)
            if any(token in name for token in ("keyboard", "pointer", "remote control")) and "check" in role:
                if not node_checked(node):
                    click_accessible(environment, node)
                    evidence["privateDevInput"].append({"action": "select-device", "name": node_name(node)})
        buttons = [
            node for node in nodes
            if node_role(node) in {"push button", "button"}
            and node_name(node).strip().lower() in {"allow", "share", "start", "ok", "approve"}
        ]
        if buttons and (not choices or target_selected):
            pointer_origin = click_accessible(environment, buttons[-1])
            evidence["privateDevInput"].append({"action": "approve", "name": node_name(buttons[-1])})
            return pointer_origin
        time.sleep(POLL_SECONDS)
    raise TerminalPortalError(
        "private KDE portal approval controls were not discoverable through AT-SPI; "
        f"observed names={sorted(set(last_names))!r}"
    )


def dock_windows(
    environment: Mapping[str, str], target_id: str, incoming_id: str
) -> dict[str, Any]:
    """Group two windows through the compositor's development docking API.

    AGENT-NOTE: the same public development call the Gabbee interop probe uses
    (Compositor1.DockWindows). It only arranges existing windows; it delivers
    no input and grants no privilege.
    """

    return _call(
        environment, "DockWindows",
        target_id.strip("{}"), incoming_id.strip("{}"),
        "horizontal", "second", "0.5",
    )


def wait_helper_ready(helper: subprocess.Popen[str], evidence: dict[str, Any]) -> bool:
    """Consume the agent-input READY handshake without blocking on an empty pipe."""

    import select

    stream = helper.stderr
    if stream is None:
        raise TerminalPortalError("agent-input stderr was not available for READY handshake")
    deadline = time.monotonic() + 8.0
    lines: list[str] = []
    while time.monotonic() < deadline:
        remaining = max(0.0, deadline - time.monotonic())
        readable, _, _ = select.select([stream], [], [], min(0.25, remaining))
        if not readable:
            if helper.poll() is not None:
                break
            continue
        line = stream.readline()
        if not line:
            break
        lines.append(line)
        if "READY" in line:
            evidence["helperStartupStderr"] = "".join(lines)
            evidence["helperReadyLine"] = line.rstrip("\n")
            return True
    evidence["helperStartupStderr"] = "".join(lines)
    return False
