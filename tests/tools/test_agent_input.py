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


def _run_proof(mode: str, timeout: int = 30) -> subprocess.CompletedProcess:
    env = dict(os.environ)
    env["PYTHONPATH"] = str(TOOLS_DIR)
    return subprocess.run(
        [_DBUS_SESSION, "--", sys.executable, str(PROOF_SCRIPT), mode],
        capture_output=True, text=True, timeout=timeout, env=env,
    )


class AgentInputBackendReadinessTest(unittest.TestCase):
    """Verify the installed KDE portal backend advertises RemoteDesktop.

    AGENT-CONTRACT: This test reads the installed kde.portal file at test time.
    If it ever stops advertising RemoteDesktop the portal conf entry
    RemoteDesktop=kde;gtk;lxqt needs re-review (ADR-0059 / qindaqt-portals.conf).
    """

    def test_kde_portal_advertises_remote_desktop(self) -> None:
        self.assertTrue(KDE_PORTAL.exists(),
                        "kde.portal not installed; xdg-desktop-portal-kde required")
        content = KDE_PORTAL.read_text(encoding="utf-8")
        self.assertIn("org.freedesktop.impl.portal.RemoteDesktop", content,
                      "KDE portal no longer advertises RemoteDesktop; "
                      "review qindaqt-portals.conf RemoteDesktop= line")


class AgentInputModuleTest(unittest.TestCase):
    """Unit tests for agent_input package components."""

    # --- events.py ---

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

    def test_dispatch_unknown_button_raises_event_error(self) -> None:
        from agent_input.events import EventError, dispatch_event
        from agent_input.portal_session import ApprovedInputSession
        from unittest.mock import MagicMock
        sess = MagicMock(spec=ApprovedInputSession)
        sess.notify_pointer_button.side_effect = ValueError("unknown button 'zap'")
        with self.assertRaises(EventError):
            dispatch_event({"action": "press", "button": "zap"}, sess)

    # --- portal_session.py ---

    def test_device_flags_are_non_overlapping(self) -> None:
        from agent_input.portal_session import DEVICE_KEYBOARD, DEVICE_POINTER
        self.assertEqual(DEVICE_KEYBOARD & DEVICE_POINTER, 0)
        self.assertNotEqual(DEVICE_KEYBOARD, 0)
        self.assertNotEqual(DEVICE_POINTER, 0)

    def test_notify_pointer_button_unknown_raises(self) -> None:
        from agent_input.portal_session import ApprovedInputSession
        from unittest.mock import MagicMock
        sess = ApprovedInputSession.__new__(ApprovedInputSession)
        sess._approved = True
        sess._session = "/dummy"
        sess._portal_obj = MagicMock()
        sess._bus = MagicMock()
        with self.assertRaises(ValueError):
            sess.notify_pointer_button("zap", True)

    def test_approved_gate_blocks_calls_before_start(self) -> None:
        from agent_input.portal_session import ApprovedInputSession
        from unittest.mock import MagicMock
        sess = ApprovedInputSession.__new__(ApprovedInputSession)
        sess._approved = False
        sess._session = "/dummy"
        sess._portal_obj = MagicMock()
        sess._bus = MagicMock()
        sess.notify_pointer_motion(1.0, 2.0)
        # No portal call should have been made.
        sess._portal_obj.assert_not_called()

    # --- stdin_reader.py ---

    def test_stdin_reader_complete_line(self) -> None:
        from agent_input.stdin_reader import StdinReader
        r = StdinReader()
        lines = r.feed(b'{"action":"move"}\n')
        self.assertEqual(lines, ['{"action":"move"}'])
        self.assertEqual(r.flush(), [])

    def test_stdin_reader_residual_line(self) -> None:
        from agent_input.stdin_reader import StdinReader
        r = StdinReader()
        self.assertEqual(r.feed(b'{"action"'), [])
        lines = r.feed(b':"move"}\n')
        self.assertEqual(lines, ['{"action":"move"}'])

    def test_stdin_reader_unicode_split(self) -> None:
        """Multi-byte UTF-8 codepoint split across two reads is decoded correctly."""
        from agent_input.stdin_reader import StdinReader
        r = StdinReader()
        # U+00E9 (é) encodes as 0xC3 0xA9
        first_byte = "é".encode("utf-8")[:1]   # 0xC3
        second_byte = "é".encode("utf-8")[1:]  # 0xA9
        self.assertEqual(r.feed(first_byte), [])
        lines = r.feed(second_byte + b"\n")
        self.assertEqual(lines, ["é"])

    def test_stdin_reader_eof_with_final_event(self) -> None:
        """Data followed immediately by EOF (flush) is not lost."""
        from agent_input.stdin_reader import StdinReader
        r = StdinReader()
        self.assertEqual(r.feed(b'{"action":"close"}'), [])
        lines = r.flush()
        self.assertEqual(lines, ['{"action":"close"}'])

    def test_stdin_reader_multiple_lines(self) -> None:
        from agent_input.stdin_reader import StdinReader
        r = StdinReader()
        lines = r.feed(b'line1\nline2\nline3\n')
        self.assertEqual(lines, ["line1", "line2", "line3"])


@unittest.skipUnless(_DBUS_SESSION, "dbus-run-session not available")
class AgentInputPortalProofTest(unittest.TestCase):
    """Fake-portal lifecycle proofs on a private bus."""

    def test_basic_lifecycle(self) -> None:
        """Full handshake and all Notify* dispatches verified."""
        r = _run_proof("basic")
        self.assertEqual(r.returncode, 0,
                         f"basic proof failed:\n{r.stdout}\n{r.stderr}")
        self.assertIn("OK:", r.stderr)

    def test_denial_cleans_up(self) -> None:
        """Denied Start: Session.Close sent, no Notify*, exit code 1."""
        r = _run_proof("denial")
        self.assertEqual(r.returncode, 0,
                         f"denial proof failed:\n{r.stdout}\n{r.stderr}")
        self.assertIn("OK:", r.stderr)

    def test_no_events_before_start(self) -> None:
        """Events written before Start response must not reach Notify*."""
        r = _run_proof("no_events_before_start")
        self.assertEqual(r.returncode, 0,
                         f"no_events_before_start proof failed:\n{r.stdout}\n{r.stderr}")
        self.assertIn("OK:", r.stderr)


if __name__ == "__main__":
    unittest.main()
