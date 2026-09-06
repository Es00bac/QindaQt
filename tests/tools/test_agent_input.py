# SPDX-License-Identifier: GPL-3.0-or-later
"""Tests for the agent-input RemoteDesktop portal tool."""
from __future__ import annotations

import os
import shutil
import subprocess
import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
TOOLS_DIR = REPO_ROOT / "tools"
PROOF_SCRIPT = Path(__file__).resolve().parent / "run_agent_input_portal_proof.py"
KDE_PORTAL = Path("/usr/share/xdg-desktop-portal/portals/kde.portal")
_DBUS_SESSION = shutil.which("dbus-run-session")


class AgentInputBackendReadinessTest(unittest.TestCase):
    """Verify the installed KDE portal backend advertises RemoteDesktop.

    AGENT-CONTRACT: This test reads the installed kde.portal file at test time.
    If it ever stops advertising RemoteDesktop the portal conf entry
    RemoteDesktop=kde;gtk;lxqt needs re-review (ADR-0059 / qindaqt-portals.conf).
    """

    def test_kde_portal_advertises_remote_desktop(self) -> None:
        self.assertTrue(KDE_PORTAL.exists(),
                        "kde.portal not installed; xdg-desktop-portal-kde is required")
        content = KDE_PORTAL.read_text(encoding="utf-8")
        self.assertIn("org.freedesktop.impl.portal.RemoteDesktop", content,
                      "KDE portal no longer advertises RemoteDesktop; "
                      "review qindaqt-portals.conf RemoteDesktop= line")


class AgentInputModuleTest(unittest.TestCase):
    """Unit tests for agent_input package components."""

    def test_parse_event_move(self) -> None:
        from agent_input.events import parse_event
        ev = parse_event('{"action":"move","dx":1.5,"dy":-2.0}')
        self.assertEqual(ev["action"], "move")
        self.assertAlmostEqual(ev["dx"], 1.5)

    def test_parse_event_blank_line(self) -> None:
        from agent_input.events import parse_event
        self.assertEqual(parse_event(""), {})
        self.assertEqual(parse_event("  \n"), {})

    def test_parse_event_invalid_json(self) -> None:
        from agent_input.events import EventError, parse_event
        with self.assertRaises(EventError):
            parse_event("{bad}")

    def test_parse_event_missing_action(self) -> None:
        from agent_input.events import EventError, parse_event
        with self.assertRaises(EventError):
            parse_event('{"dx":1}')

    def test_parse_event_text(self) -> None:
        from agent_input.events import parse_event
        ev = parse_event('{"action":"text","text":"hello"}')
        self.assertEqual(ev["text"], "hello")

    def test_dispatch_close_returns_false(self) -> None:
        from agent_input.events import dispatch_event
        self.assertFalse(dispatch_event({"action": "close"}, object()))

    def test_dispatch_unknown_action_returns_true(self) -> None:
        from agent_input.events import dispatch_event
        self.assertTrue(dispatch_event({"action": "future_action"}, object()))

    def test_device_flags_are_non_overlapping(self) -> None:
        from agent_input.portal_session import DEVICE_KEYBOARD, DEVICE_POINTER
        self.assertEqual(DEVICE_KEYBOARD & DEVICE_POINTER, 0)
        self.assertNotEqual(DEVICE_KEYBOARD, 0)
        self.assertNotEqual(DEVICE_POINTER, 0)


@unittest.skipUnless(_DBUS_SESSION, "dbus-run-session not available")
class AgentInputPortalProofTest(unittest.TestCase):
    """Fake-portal lifecycle proof on a private bus.

    Verifies the full CreateSession → SelectDevices → Start handshake and
    every Notify* dispatch using a fake org.freedesktop.portal.Desktop service.
    """

    def test_session_lifecycle_proof(self) -> None:
        env = dict(os.environ)
        env["PYTHONPATH"] = str(TOOLS_DIR)
        result = subprocess.run(
            [_DBUS_SESSION, "--", sys.executable, str(PROOF_SCRIPT)],
            capture_output=True, text=True, timeout=30, env=env,
        )
        self.assertEqual(
            result.returncode, 0,
            f"Portal proof failed (rc={result.returncode}):\n"
            f"stdout: {result.stdout}\nstderr: {result.stderr}",
        )
        self.assertIn("OK:", result.stderr, "Proof script did not print OK")


if __name__ == "__main__":
    unittest.main()
