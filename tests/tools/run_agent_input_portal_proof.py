#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fake RemoteDesktop portal proofs for the agent-input session lifecycle.

Run inside dbus-run-session with one mode argument:
  basic                   full lifecycle, verifies all Notify* calls
  denial                  Start denied; verifies Close sent, exit code 1
  no_events_before_start  write stdin before Start response; verify no Notify

AGENT-GUARD: This script must run on a private bus only. It registers the
real org.freedesktop.portal.Desktop service name; running on the host bus
would shadow the installed portal for all applications in that session.
"""
from __future__ import annotations

import os
import subprocess
import sys
import threading
from pathlib import Path
from typing import Any

import dbus
import dbus.mainloop.glib
import dbus.service
from gi.repository import GLib

TOOLS_DIR = Path(__file__).resolve().parents[2] / "tools"
PORTAL_SERVICE = "org.freedesktop.portal.Desktop"
PORTAL_PATH = "/org/freedesktop/portal/desktop"
_RD_IFACE = "org.freedesktop.portal.RemoteDesktop"
_SESS_IFACE = "org.freedesktop.portal.Session"
_REQ_SIG = "ua{sv}"


# ---------------------------------------------------------------------------
# Fake service objects
# ---------------------------------------------------------------------------

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

    Behaviour is set by subclasses or by the deny_start flag.
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


# ---------------------------------------------------------------------------
# Process helper
# ---------------------------------------------------------------------------

def _start_tool(extra_env: dict | None = None) -> subprocess.Popen:
    env = dict(os.environ)
    env["PYTHONPATH"] = str(TOOLS_DIR)
    if extra_env:
        env.update(extra_env)
    return subprocess.Popen(
        [sys.executable, str(TOOLS_DIR / "agent-input")],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        env=env,
    )


def _wait_for_ready(proc: subprocess.Popen, timeout: float = 10.0) -> bool:
    """Read stderr lines until READY appears or EOF. Returns True on READY."""
    import select
    deadline = __import__("time").time() + timeout
    buf = b""
    while __import__("time").time() < deadline:
        remaining = deadline - __import__("time").time()
        r, _, _ = select.select([proc.stderr], [], [], min(remaining, 0.5))
        if r:
            chunk = os.read(proc.stderr.fileno(), 4096)
            if not chunk:
                return False
            buf += chunk
            if b"READY" in buf:
                return True
    return False


# ---------------------------------------------------------------------------
# Proof modes
# ---------------------------------------------------------------------------

def _proof_basic() -> int:
    """Full lifecycle: handshake + all Notify* calls match."""
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SessionBus()
    bus.request_name(PORTAL_SERVICE)
    portal = FakeRemoteDesktop(bus)
    loop = GLib.MainLoop()

    events = (
        b'{"action":"move","dx":5.0,"dy":-3.0}\n'
        b'{"action":"press","button":"left"}\n'
        b'{"action":"release","button":"left"}\n'
        b'{"action":"scroll","dx":0.0,"dy":2.0}\n'
        b'{"action":"key_press","keysym":65}\n'
        b'{"action":"key_release","keysym":65}\n'
        b'{"action":"text","text":"hi"}\n'
        b'{"action":"close"}\n'
    )
    proc = _start_tool()

    def _runner() -> None:
        if _wait_for_ready(proc):
            proc.stdin.write(events)
            proc.stdin.close()
        proc.wait(timeout=10)
        GLib.idle_add(loop.quit)

    t = threading.Thread(target=_runner, daemon=True)
    t.start()
    GLib.timeout_add(15_000, lambda: (loop.quit(), False)[1])
    loop.run()
    t.join(timeout=2)

    expected_handshake = ["CreateSession", "SelectDevices", "Start"]
    if portal.calls[:3] != expected_handshake:
        print(f"FAIL: handshake wrong: {portal.calls[:3]}", file=sys.stderr)
        return 1
    expected_notify = [
        "NotifyPointerMotion(5.0,-3.0)",
        "NotifyPointerButton(272,1)",
        "NotifyPointerButton(272,0)",
        "NotifyPointerAxis(0.0,2.0)",
        "NotifyKeyboardKeysym(65,1)",
        "NotifyKeyboardKeysym(65,0)",
        "NotifyKeyboardKeysym(104,1)", "NotifyKeyboardKeysym(104,0)",
        "NotifyKeyboardKeysym(105,1)", "NotifyKeyboardKeysym(105,0)",
    ]
    actual_notify = portal.calls[3:]
    if actual_notify != expected_notify:
        print(f"FAIL: notify mismatch\n  expected: {expected_notify}\n"
              f"  actual:   {actual_notify}", file=sys.stderr)
        return 1
    print("OK: basic lifecycle passed", file=sys.stderr)
    return 0


def _proof_denial() -> int:
    """Start denied: tool must close session, exit code 1, no Notify*."""
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SessionBus()
    bus.request_name(PORTAL_SERVICE)
    portal = FakeRemoteDesktop(bus, deny_start=True)
    loop = GLib.MainLoop()
    proc = _start_tool()

    def _runner() -> None:
        proc.wait(timeout=10)
        GLib.idle_add(loop.quit)

    t = threading.Thread(target=_runner, daemon=True)
    t.start()
    GLib.timeout_add(10_000, lambda: (loop.quit(), False)[1])
    loop.run()
    t.join(timeout=2)

    notify_calls = [c for c in portal.calls if c.startswith("Notify")]
    if notify_calls:
        print(f"FAIL: Notify* reached portal despite denial: {notify_calls}",
              file=sys.stderr)
        return 1
    if portal.session is None or not portal.session.close_called:
        print("FAIL: Session.Close not called after denial", file=sys.stderr)
        return 1
    if proc.returncode != 1:
        print(f"FAIL: expected exit code 1, got {proc.returncode}", file=sys.stderr)
        return 1
    print("OK: denial cleaned up correctly", file=sys.stderr)
    return 0


def _proof_no_events_before_start() -> int:
    """Events written before Start completes must not reach Notify*."""
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SessionBus()
    bus.request_name(PORTAL_SERVICE)
    # Delay Start response 300ms to leave a window for pre-approval writes.
    portal = FakeRemoteDesktop(bus, start_delay_ms=300)
    loop = GLib.MainLoop()
    proc = _start_tool()

    pre_start_events = b'{"action":"move","dx":1.0,"dy":0.0}\n'
    post_start_events = b'{"action":"close"}\n'

    notify_at_start: list[int] = []

    def _runner() -> None:
        # Write a move event before Start fires (immediately, no READY wait).
        proc.stdin.write(pre_start_events)
        proc.stdin.flush()
        # Wait for READY (Start approved, delayed 300ms).
        if _wait_for_ready(proc, timeout=5):
            notify_at_start.append(len(portal.calls))
        proc.stdin.write(post_start_events)
        proc.stdin.close()
        proc.wait(timeout=10)
        GLib.idle_add(loop.quit)

    t = threading.Thread(target=_runner, daemon=True)
    t.start()
    GLib.timeout_add(10_000, lambda: (loop.quit(), False)[1])
    loop.run()
    t.join(timeout=2)

    # All Notify* calls must come after Start in portal.calls.
    notify_calls = [c for c in portal.calls if c.startswith("Notify")]
    start_idx = portal.calls.index("Start") if "Start" in portal.calls else -1
    first_notify_idx = (portal.calls.index(notify_calls[0])
                        if notify_calls else len(portal.calls))
    if start_idx < 0:
        print("FAIL: Start never reached portal", file=sys.stderr)
        return 1
    if first_notify_idx <= start_idx:
        print(f"FAIL: Notify* at index {first_notify_idx} precedes Start at "
              f"{start_idx} in {portal.calls}", file=sys.stderr)
        return 1
    print("OK: no Notify* reached portal before Start succeeded", file=sys.stderr)
    return 0


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

_MODES = {
    "basic": _proof_basic,
    "denial": _proof_denial,
    "no_events_before_start": _proof_no_events_before_start,
}

if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "basic"
    fn = _MODES.get(mode)
    if fn is None:
        print(f"unknown mode {mode!r}; choices: {list(_MODES)}", file=sys.stderr)
        raise SystemExit(2)
    raise SystemExit(fn())
