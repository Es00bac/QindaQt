#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fake RemoteDesktop portal proof for the agent-input session lifecycle.

Run this script inside a private dbus-run-session. It registers a fake
org.freedesktop.portal.Desktop service, exercises the agent-input tool with
a controlled event sequence, and exits 0 if every step of the portal handshake
and each expected Notify* call was received in order.

AGENT-GUARD: This script must remain self-contained within a private bus.
Never run it on the real session bus; it registers the portal service name
used by the desktop, which would shadow the installed portal for all applications.
"""
from __future__ import annotations

import os
import subprocess
import sys
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


class FakeSession(dbus.service.Object):
    @dbus.service.method(_SESS_IFACE)
    def Close(self):
        pass


class FakeRemoteDesktop(dbus.service.Object):
    """Minimal RemoteDesktop portal implementation for lifecycle proofs."""

    def __init__(self, bus: dbus.SessionBus) -> None:
        dbus.service.Object.__init__(self, bus, PORTAL_PATH)
        self._bus = bus
        self.calls: list[str] = []

    @dbus.service.method(_RD_IFACE, in_signature="a{sv}", out_signature="o",
                         sender_keyword="sender")
    def CreateSession(self, options: dict, sender: str = "") -> dbus.ObjectPath:
        self.calls.append("CreateSession")
        safe = sender[1:].replace(".", "_") if sender.startswith(":") else sender
        handle_token = str(options.get("handle_token", "h"))
        sess_token = str(options.get("session_handle_token", "s"))
        req_path = f"{PORTAL_PATH}/request/{safe}/{handle_token}"
        sess_path = f"{PORTAL_PATH}/session/{safe}/{sess_token}"
        FakeSession(self._bus, sess_path)
        req = dbus.service.Object(self._bus, req_path)

        @staticmethod
        def _emit() -> bool:
            req.PropertiesChanged  # noqa: B018 (keep ref alive)
            self._bus.get_object(PORTAL_SERVICE, req_path).emit_signal(
                req_path, "org.freedesktop.portal.Request", "Response",
                _REQ_SIG, dbus.UInt32(0),
                {"session_handle": dbus.ObjectPath(sess_path)},
            )
            return False

        GLib.idle_add(lambda: _emit_response(
            self._bus, req_path, 0,
            {"session_handle": dbus.ObjectPath(sess_path)},
        ))
        return dbus.ObjectPath(req_path)

    @dbus.service.method(_RD_IFACE, in_signature="oa{sv}", out_signature="o",
                         sender_keyword="sender")
    def SelectDevices(self, session_handle, options: dict,
                      sender: str = "") -> dbus.ObjectPath:
        self.calls.append("SelectDevices")
        safe = sender[1:].replace(".", "_") if sender.startswith(":") else sender
        handle_token = str(options.get("handle_token", "h"))
        req_path = f"{PORTAL_PATH}/request/{safe}/{handle_token}"
        dbus.service.Object(self._bus, req_path)
        GLib.idle_add(lambda: _emit_response(self._bus, req_path, 0, {}))
        return dbus.ObjectPath(req_path)

    @dbus.service.method(_RD_IFACE, in_signature="osa{sv}", out_signature="o",
                         sender_keyword="sender")
    def Start(self, session_handle, parent_window: str, options: dict,
              sender: str = "") -> dbus.ObjectPath:
        self.calls.append("Start")
        safe = sender[1:].replace(".", "_") if sender.startswith(":") else sender
        handle_token = str(options.get("handle_token", "h"))
        req_path = f"{PORTAL_PATH}/request/{safe}/{handle_token}"
        dbus.service.Object(self._bus, req_path)
        GLib.idle_add(lambda: _emit_response(
            self._bus, req_path, 0, {"devices": dbus.UInt32(3)},
        ))
        return dbus.ObjectPath(req_path)

    @dbus.service.method(_RD_IFACE, in_signature="oa{sv}dd")
    def NotifyPointerMotion(self, session, options, dx, dy):
        self.calls.append(f"NotifyPointerMotion({dx},{dy})")

    @dbus.service.method(_RD_IFACE, in_signature="oa{sv}udd")
    def NotifyPointerMotionAbsolute(self, session, options, stream, x, y):
        self.calls.append(f"NotifyPointerMotionAbsolute({stream},{x},{y})")

    @dbus.service.method(_RD_IFACE, in_signature="oa{sv}iu")
    def NotifyPointerButton(self, session, options, button, state):
        self.calls.append(f"NotifyPointerButton({button},{state})")

    @dbus.service.method(_RD_IFACE, in_signature="oa{sv}dd")
    def NotifyPointerAxis(self, session, options, dx, dy):
        self.calls.append(f"NotifyPointerAxis({dx},{dy})")

    @dbus.service.method(_RD_IFACE, in_signature="oa{sv}iu")
    def NotifyKeyboardKeysym(self, session, options, keysym, state):
        self.calls.append(f"NotifyKeyboardKeysym({keysym},{state})")

    @dbus.service.method(_RD_IFACE, in_signature="oa{sv}iu")
    def NotifyKeyboardKeycode(self, session, options, keycode, state):
        self.calls.append(f"NotifyKeyboardKeycode({keycode},{state})")


def _emit_response(bus: dbus.SessionBus, path: str,
                   code: int, results: dict[str, Any]) -> bool:
    try:
        msg = dbus.lowlevel.SignalMessage(path, "org.freedesktop.portal.Request",
                                         "Response")
        msg.append(dbus.UInt32(code), results,
                   signature=dbus.Signature(_REQ_SIG))
        bus.send_message(msg)
    except Exception as exc:
        print(f"WARN: Response signal failed: {exc}", file=sys.stderr)
    return False


def _run_proof() -> int:
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SessionBus()
    bus.request_name(PORTAL_SERVICE)

    portal = FakeRemoteDesktop(bus)
    loop = GLib.MainLoop()

    # Events that exercise every Notify* method, then close.
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

    env = dict(os.environ)
    env["PYTHONPATH"] = str(TOOLS_DIR)

    proc = subprocess.Popen(
        [sys.executable, str(TOOLS_DIR / "agent-input")],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        env=env,
    )

    def _send_and_wait() -> bool:
        # Wait for READY on stderr, then send events.
        for chunk in proc.stderr:
            if b"READY" in chunk:
                break
        proc.stdin.write(events)
        proc.stdin.close()
        proc.wait(timeout=10)
        loop.quit()
        return False

    import threading
    t = threading.Thread(target=_send_and_wait, daemon=True)
    t.start()

    GLib.timeout_add(15_000, lambda: (loop.quit(), False)[1])
    loop.run()
    t.join(timeout=2)

    expected_prefix = ["CreateSession", "SelectDevices", "Start"]
    if portal.calls[:3] != expected_prefix:
        print(f"FAIL: handshake sequence wrong: {portal.calls[:3]}", file=sys.stderr)
        return 1

    expected_notify = [
        "NotifyPointerMotion(5.0,-3.0)",
        "NotifyPointerButton(272,1)",
        "NotifyPointerButton(272,0)",
        "NotifyPointerAxis(0.0,2.0)",
        "NotifyKeyboardKeysym(65,1)",
        "NotifyKeyboardKeysym(65,0)",
        # "hi" → h=104 press/release, i=105 press/release
        "NotifyKeyboardKeysym(104,1)",
        "NotifyKeyboardKeysym(104,0)",
        "NotifyKeyboardKeysym(105,1)",
        "NotifyKeyboardKeysym(105,0)",
    ]
    actual_notify = portal.calls[3:]
    if actual_notify != expected_notify:
        print(f"FAIL: notify sequence mismatch\n"
              f"  expected: {expected_notify}\n"
              f"  actual:   {actual_notify}", file=sys.stderr)
        return 1

    print("OK: portal handshake and all Notify* calls matched", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(_run_proof())
