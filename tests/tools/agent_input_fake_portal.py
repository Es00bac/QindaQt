# SPDX-License-Identifier: GPL-3.0-or-later
"""Fake org.freedesktop.portal.Desktop service for agent-input proofs.

AGENT-GUARD: These classes register the real portal service name. Only import
and instantiate them inside a private dbus-run-session bus; on the host bus
they would shadow the installed desktop portal for every application.
"""
from __future__ import annotations

import os
import select
import subprocess
import sys
import threading
import time
from pathlib import Path
from typing import Any

import dbus
import dbus.service
from gi.repository import GLib

TOOLS_DIR = Path(__file__).resolve().parents[2] / "tools"
PORTAL_SERVICE = "org.freedesktop.portal.Desktop"
PORTAL_PATH = "/org/freedesktop/portal/desktop"
_RD_IFACE = "org.freedesktop.portal.RemoteDesktop"
_SESS_IFACE = "org.freedesktop.portal.Session"
_REQ_SIG = "ua{sv}"


class FakeRequest(dbus.service.Object):
    @dbus.service.signal("org.freedesktop.portal.Request", signature=_REQ_SIG)
    def Response(self, code: int, results: dict) -> None:
        pass


class FakeSession(dbus.service.Object):
    def __init__(self, bus: dbus.SessionBus, path: str) -> None:
        dbus.service.Object.__init__(self, bus, path)
        self.close_called = False

    @dbus.service.method(_SESS_IFACE)
    def Close(self) -> None:
        self.close_called = True

    @dbus.service.signal(_SESS_IFACE, signature="a{sv}")
    def Closed(self, details: dict) -> None:
        pass


class FakeRemoteDesktop(dbus.service.Object):
    """Minimal RemoteDesktop portal implementation for lifecycle proofs.

    Behaviour is set by subclasses or by the deny_start / start_delay_ms flags.
    """

    def __init__(self, bus: dbus.SessionBus, deny_start: bool = False,
                 start_delay_ms: int = 0) -> None:
        dbus.service.Object.__init__(self, bus, PORTAL_PATH)
        self._bus = bus
        self._deny_start = deny_start
        self._start_delay_ms = start_delay_ms
        self.calls: list[str] = []
        self.session: FakeSession | None = None

    def _make_req(self, sender: str, token: str, code: int,
                  results: dict, delay_ms: int = 0) -> dbus.ObjectPath:
        safe = sender[1:].replace(".", "_") if sender.startswith(":") else sender
        req_path = f"{PORTAL_PATH}/request/{safe}/{token}"
        req = FakeRequest(self._bus, req_path)

        def emit() -> bool:
            req.Response(dbus.UInt32(code), results)
            return False

        if delay_ms > 0:
            GLib.timeout_add(delay_ms, emit)
        else:
            GLib.idle_add(emit)
        return dbus.ObjectPath(req_path)

    @dbus.service.method(_RD_IFACE, in_signature="a{sv}", out_signature="o",
                         sender_keyword="sender")
    def CreateSession(self, options: dict, sender: str = "") -> dbus.ObjectPath:
        self.calls.append("CreateSession")
        safe = sender[1:].replace(".", "_") if sender.startswith(":") else sender
        sess_token = str(options.get("session_handle_token", "s"))
        sess_path = f"{PORTAL_PATH}/session/{safe}/{sess_token}"
        self.session = FakeSession(self._bus, sess_path)
        return self._make_req(
            sender, str(options.get("handle_token", "h")), 0,
            {"session_handle": dbus.ObjectPath(sess_path, variant_level=1)},
        )

    @dbus.service.method(_RD_IFACE, in_signature="oa{sv}", out_signature="o",
                         sender_keyword="sender")
    def SelectDevices(self, session_handle: Any, options: dict,
                      sender: str = "") -> dbus.ObjectPath:
        self.calls.append("SelectDevices")
        return self._make_req(sender, str(options.get("handle_token", "h")), 0, {})

    @dbus.service.method(_RD_IFACE, in_signature="osa{sv}", out_signature="o",
                         sender_keyword="sender")
    def Start(self, session_handle: Any, parent_window: str, options: dict,
              sender: str = "") -> dbus.ObjectPath:
        self.calls.append("Start")
        code = 1 if self._deny_start else 0
        results: dict = {} if code else {"devices": dbus.UInt32(3, variant_level=1)}
        return self._make_req(sender, str(options.get("handle_token", "h")),
                              code, results, self._start_delay_ms)

    @dbus.service.method(_RD_IFACE, in_signature="oa{sv}dd")
    def NotifyPointerMotion(self, session: Any, options: Any,
                            dx: float, dy: float) -> None:
        self.calls.append(f"NotifyPointerMotion({float(dx)},{float(dy)})")

    @dbus.service.method(_RD_IFACE, in_signature="oa{sv}iu")
    def NotifyPointerButton(self, session: Any, options: Any,
                            button: int, state: int) -> None:
        self.calls.append(f"NotifyPointerButton({int(button)},{int(state)})")

    @dbus.service.method(_RD_IFACE, in_signature="oa{sv}dd")
    def NotifyPointerAxis(self, session: Any, options: Any,
                          dx: float, dy: float) -> None:
        self.calls.append(f"NotifyPointerAxis({float(dx)},{float(dy)})")

    @dbus.service.method(_RD_IFACE, in_signature="oa{sv}iu")
    def NotifyKeyboardKeysym(self, session: Any, options: Any,
                             keysym: int, state: int) -> None:
        self.calls.append(f"NotifyKeyboardKeysym({int(keysym)},{int(state)})")


class FailingNotifyPortal(FakeRemoteDesktop):
    """Portal whose NotifyPointerMotion always returns a D-Bus error."""

    @dbus.service.method(_RD_IFACE, in_signature="oa{sv}dd")
    def NotifyPointerMotion(self, session: Any, options: Any,
                            dx: float, dy: float) -> None:
        self.calls.append("NotifyPointerMotion(FAILED)")
        raise dbus.DBusException(
            "org.freedesktop.portal.RemoteDesktop.Error.Failed",
            "simulated notify transport failure")


def start_tool(extra_env: dict | None = None) -> subprocess.Popen:
    env = dict(os.environ)
    env["PYTHONPATH"] = str(TOOLS_DIR)
    if extra_env:
        env.update(extra_env)
    return subprocess.Popen(
        [sys.executable, str(TOOLS_DIR / "agent-input")],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        env=env,
    )


def wait_for_ready(proc: subprocess.Popen, timeout: float = 10.0) -> bool:
    """Read stderr until READY appears or EOF. Returns True on READY."""
    deadline = time.time() + timeout
    buf = b""
    while time.time() < deadline:
        remaining = deadline - time.time()
        r, _, _ = select.select([proc.stderr], [], [], min(remaining, 0.5))
        if r:
            chunk = os.read(proc.stderr.fileno(), 4096)
            if not chunk:
                return False
            buf += chunk
            if b"READY" in buf:
                return True
    return False


def watch_tool(proc: subprocess.Popen, loop: GLib.MainLoop,
               timeout: float = 10.0) -> None:
    """Watch for tool exit in a background thread, then stop the proof loop.

    The watcher never touches stdin; proofs that keep the pipe open rely on
    the tool's own session lifecycle to exit. A timeout kills the process so
    the proof fails with evidence instead of hanging.
    """
    def _runner() -> None:
        try:
            proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
        finally:
            proc.stdin.close()
        GLib.idle_add(loop.quit)

    t = threading.Thread(target=_runner, daemon=True)
    t.start()
