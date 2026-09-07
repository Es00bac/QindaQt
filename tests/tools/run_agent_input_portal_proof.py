#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fake RemoteDesktop portal proofs for the agent-input session lifecycle.

Run inside dbus-run-session with one mode argument:
  basic                   full lifecycle, verifies all Notify* calls
  denial                  Start denied; verifies Close sent, exit code 1
  no_events_before_start  write stdin before Start response; verify no Notify
  notify_failure          Notify* D-Bus error; verifies Close sent, exit code 1
  revoke                  portal emits Session Closed; verifies clean exit 0

AGENT-GUARD: This script must run on a private bus only. It registers the
real org.freedesktop.portal.Desktop service name; running on the host bus
would shadow the installed portal for all applications in that session.
"""
from __future__ import annotations

import sys
import threading
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import dbus
import dbus.mainloop.glib
from gi.repository import GLib

from agent_input_fake_portal import (
    PORTAL_SERVICE,
    FakeRemoteDesktop,
    FailingNotifyPortal,
    start_tool,
    wait_for_ready,
    watch_tool,
)


def _serve(portal_cls=FakeRemoteDesktop, **kwargs) -> FakeRemoteDesktop:
    """Own the portal name on the private bus and return the fake service."""
    bus = dbus.SessionBus()
    bus.request_name(PORTAL_SERVICE)
    return portal_cls(bus, **kwargs)


def _fail(msg: str) -> int:
    print(f"FAIL: {msg}", file=sys.stderr)
    return 1


def _proof_basic() -> int:
    """Full lifecycle: handshake + all Notify* calls match."""
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    portal = _serve()
    loop = GLib.MainLoop()

    # Real UTF-8 bytes for U+4E16 on stdin: proves the reader/JSON/keysym
    # path end-to-end, not just the StdinReader split-decode unit row.
    unicode_event = '{"action":"text","text":"世"}\n'.encode("utf-8")
    events = (
        b'{"action":"move","dx":5.0,"dy":-3.0}\n'
        b'{"action":"press","button":"left"}\n'
        b'{"action":"release","button":"left"}\n'
        b'{"action":"scroll","dx":0.0,"dy":2.0}\n'
        b'{"action":"key_press","keysym":65}\n'
        b'{"action":"key_release","keysym":65}\n'
        b'{"action":"text","text":"hi","requestId":"basic-text"}\n'
        + unicode_event
        + b'{"action":"close"}\n'
    )
    proc = start_tool()

    def _runner() -> None:
        if wait_for_ready(proc):
            # Write everything, then close stdin: the final chunk must still
            # be delivered (EOF/HUP data retention).
            proc.stdin.write(events)
            proc.stdin.close()
        proc.wait(timeout=10)
        GLib.idle_add(loop.quit)

    t = threading.Thread(target=_runner, daemon=True)
    t.start()
    GLib.timeout_add(15_000, lambda: (loop.quit(), False)[1])
    loop.run()
    t.join(timeout=2)

    ack_output = proc.stdout.read()
    if b'{"requestId":"basic-text","ok":true}' not in ack_output:
        return _fail(f"missing successful portal-acceptance ack: {ack_output!r}")
    if portal.calls[:3] != ["CreateSession", "SelectDevices", "Start"]:
        return _fail(f"handshake wrong: {portal.calls[:3]}")
    expected_notify = [
        "NotifyPointerMotion(5.0,-3.0)",
        "NotifyPointerButton(272,1)",
        "NotifyPointerButton(272,0)",
        "NotifyPointerAxis(0.0,2.0)",
        "NotifyKeyboardKeysym(65,1)",
        "NotifyKeyboardKeysym(65,0)",
        "NotifyKeyboardKeysym(104,1)", "NotifyKeyboardKeysym(104,0)",
        "NotifyKeyboardKeysym(105,1)", "NotifyKeyboardKeysym(105,0)",
        # U+4E16 maps to keysym 0x01004E16 (0x01000000 | codepoint).
        f"NotifyKeyboardKeysym({0x01004E16},1)",
        f"NotifyKeyboardKeysym({0x01004E16},0)",
    ]
    if portal.calls[3:] != expected_notify:
        return _fail(f"notify mismatch\n  expected: {expected_notify}\n"
                     f"  actual:   {portal.calls[3:]}")
    print("OK: basic lifecycle passed", file=sys.stderr)
    return 0


def _proof_denial() -> int:
    """Start denied: tool must close session, exit code 1, no Notify*."""
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    portal = _serve(deny_start=True)
    loop = GLib.MainLoop()
    proc = start_tool()

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
        return _fail(f"Notify* reached portal despite denial: {notify_calls}")
    if portal.session is None or not portal.session.close_called:
        return _fail("Session.Close not called after denial")
    if proc.returncode != 1:
        return _fail(f"expected exit code 1, got {proc.returncode}")
    print("OK: denial cleaned up correctly", file=sys.stderr)
    return 0


def _proof_no_events_before_start() -> int:
    """Events written before Start completes must not reach Notify*."""
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    # Delay Start response 300ms to leave a window for pre-approval writes.
    portal = _serve(start_delay_ms=300)
    loop = GLib.MainLoop()
    proc = start_tool()

    def _runner() -> None:
        # Write a move event before Start fires (immediately, no READY wait).
        proc.stdin.write(b'{"action":"move","dx":1.0,"dy":0.0}\n')
        proc.stdin.flush()
        wait_for_ready(proc, timeout=5)
        proc.stdin.write(b'{"action":"close"}\n')
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
        return _fail("Start never reached portal")
    if first_notify_idx <= start_idx:
        return _fail(f"Notify* at index {first_notify_idx} precedes Start at "
                     f"{start_idx} in {portal.calls}")
    print("OK: no Notify* reached portal before Start succeeded", file=sys.stderr)
    return 0


def _proof_notify_failure() -> int:
    """A Notify* D-Bus error must close the session and exit nonzero."""
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    portal = _serve(FailingNotifyPortal)
    loop = GLib.MainLoop()
    proc = start_tool()

    def _interact() -> None:
        # stdin stays OPEN so only the failure path can end the process.
        if wait_for_ready(proc, timeout=5):
            proc.stdin.write(b'{"action":"move","dx":1.0,"dy":2.0,"requestId":"failing-move"}\n')
            proc.stdin.flush()

    threading.Thread(target=_interact, daemon=True).start()
    watch_tool(proc, loop)
    GLib.timeout_add(15_000, lambda: (loop.quit(), False)[1])
    loop.run()

    attempts = [c for c in portal.calls if c.startswith("Notify")]
    if attempts != ["NotifyPointerMotion(FAILED)"]:
        return _fail(f"expected exactly one failing Notify, got {attempts}")
    if portal.session is None or not portal.session.close_called:
        return _fail("Session.Close not called after notify failure")
    if proc.returncode != 1:
        return _fail(f"expected exit code 1, got {proc.returncode}")
    stderr_tail = proc.stderr.read()
    if b"ERROR:" not in stderr_tail:
        return _fail(f"no ERROR report on stderr: {stderr_tail!r}")
    ack_output = proc.stdout.read()
    if b'{"requestId":"failing-move","ok":false,' not in ack_output:
        return _fail(f"missing failed portal-acceptance ack: {ack_output!r}")
    print("OK: notify failure closed session and exited nonzero", file=sys.stderr)
    return 0


def _proof_revoke() -> int:
    """External Session Closed must end the tool cleanly without stdin EOF."""
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    portal = _serve()
    loop = GLib.MainLoop()
    proc = start_tool()

    def _interact() -> None:
        delivered = False
        if wait_for_ready(proc, timeout=5):
            proc.stdin.write(b'{"action":"move","dx":7.0,"dy":0.0}\n')
            proc.stdin.flush()
            deadline = time.time() + 5
            while time.time() < deadline:
                if any(c.startswith("NotifyPointerMotion(") for c in portal.calls):
                    delivered = True
                    break
                time.sleep(0.05)
        if delivered:
            def _emit_closed() -> bool:
                portal.session.Closed({})
                return False
            GLib.idle_add(_emit_closed)

    threading.Thread(target=_interact, daemon=True).start()
    watch_tool(proc, loop)
    GLib.timeout_add(15_000, lambda: (loop.quit(), False)[1])
    loop.run()

    if not any(c.startswith("NotifyPointerMotion(") for c in portal.calls):
        return _fail("session was not live (no Notify) before revocation")
    if proc.returncode != 0:
        return _fail(f"revoked tool should exit 0, got {proc.returncode}")
    stderr_tail = proc.stderr.read()
    if b"ERROR:" in stderr_tail:
        return _fail(f"revocation produced an error: {stderr_tail!r}")
    print("OK: external Session Closed ended the tool cleanly", file=sys.stderr)
    return 0


_MODES = {
    "basic": _proof_basic,
    "denial": _proof_denial,
    "no_events_before_start": _proof_no_events_before_start,
    "notify_failure": _proof_notify_failure,
    "revoke": _proof_revoke,
}

if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "basic"
    fn = _MODES.get(mode)
    if fn is None:
        print(f"unknown mode {mode!r}; choices: {list(_MODES)}", file=sys.stderr)
        raise SystemExit(2)
    raise SystemExit(fn())
