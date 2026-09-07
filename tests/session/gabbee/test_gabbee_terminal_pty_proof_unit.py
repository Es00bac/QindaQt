# SPDX-License-Identifier: GPL-3.0-or-later
"""Deterministic tests for the Terminal PTY proof's host-safe logic.

Covers the sentinel byte contract, the portal chooser predicate, and the
adapter that drives Gabbee's real ``AgentInputTextSink``. The sandboxed
proof itself (run_gabbee_terminal_pty_proof.py) needs the private runtime
lane and is exercised separately; nothing here starts a portal, a
compositor, or a helper process.

AGENT-NOTE: the sink adapter is tested against a local stub that stands in
for Gabbee's sink *interface*, which is how the adapter's own concurrency
and error mapping get covered without a lane. That stub is never used by
the proof itself — SinkContractTests pins the real class's behaviour so the
stub cannot drift away from it unnoticed.
"""

from __future__ import annotations

import sys
import threading
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from gabbee_terminal_portal import chooser_matches  # noqa: E402
from gabbee_terminal_sentinel import (  # noqa: E402
    expected_sentinel_bytes,
    generate_sentinel,
    sentinel_output_filename,
)
import gabbee_terminal_sink as sink_module  # noqa: E402


class SentinelTests(unittest.TestCase):
    def test_sentinel_is_unique_per_call(self) -> None:
        self.assertNotEqual(generate_sentinel("run-a"), generate_sentinel("run-a"))

    def test_sentinel_contains_run_id_and_non_ascii(self) -> None:
        sentinel = generate_sentinel("deadbeef")
        self.assertIn("deadbeef", sentinel)
        self.assertFalse(sentinel.isascii())

    def test_sentinel_round_trips_through_utf8(self) -> None:
        sentinel = generate_sentinel("run-b")
        self.assertEqual(sentinel.encode("utf-8").decode("utf-8"), sentinel)


class ExpectedBytesTests(unittest.TestCase):
    def test_expected_bytes_append_exactly_one_newline(self) -> None:
        self.assertEqual(expected_sentinel_bytes("hello ✓"), "hello ✓\n".encode("utf-8"))

    def test_expected_bytes_reject_a_missing_trailing_newline(self) -> None:
        self.assertNotEqual(expected_sentinel_bytes("hello"), b"hello")

    def test_expected_bytes_reject_extra_trailing_bytes(self) -> None:
        self.assertNotEqual(expected_sentinel_bytes("hello"), b"hello\nextra")


class OutputFilenameTests(unittest.TestCase):
    def test_filename_embeds_the_run_id(self) -> None:
        name = sentinel_output_filename("cafef00d")
        self.assertIn("cafef00d", name)
        self.assertTrue(name.endswith(".txt"))

    def test_filenames_differ_across_runs(self) -> None:
        self.assertNotEqual(sentinel_output_filename("run-1"), sentinel_output_filename("run-2"))

    def test_each_case_label_gets_its_own_file(self) -> None:
        """A grouped-member pass must not be satisfiable by the standalone file."""

        self.assertNotEqual(
            sentinel_output_filename("run-1-standalone"),
            sentinel_output_filename("run-1-group-member"),
        )


class ChooserMatchTests(unittest.TestCase):
    def test_matches_case_insensitively(self) -> None:
        self.assertTrue(chooser_matches("Session 1 — QindaQt Terminal", "terminal"))
        self.assertTrue(chooser_matches("SESSION 1 — QINDAQT TERMINAL", "terminal"))

    def test_rejects_unrelated_names(self) -> None:
        self.assertFalse(chooser_matches("Whole Screen", "terminal"))
        self.assertFalse(chooser_matches("Untitled — QindaQt Text Editor", "terminal"))


class _StubResult:
    """Shape-compatible stand-in for Gabbee's DeliveryResult."""

    def __init__(self, ok=True, method="agent-input", detail="", uncertain=False,
                 target_verified=False) -> None:
        self.ok = ok
        self.method = method
        self.detail = detail
        self.uncertain = uncertain
        self.target_verified = target_verified


class _StubSink:
    """Stands in for the sink interface: deliver() blocks until approved."""

    def __init__(self, result=None, raises=None, never_returns=False) -> None:
        self.approved = threading.Event()
        self.delivered: list[str] = []
        self._result = result or _StubResult()
        self._raises = raises
        self._never_returns = never_returns
        self.closed = False

    def deliver(self, text: str):
        # Mirrors the real sink: blocks until the portal dialog is approved.
        if not self.approved.wait(timeout=5):
            raise AssertionError("deliver() was not approved concurrently")
        if self._never_returns:
            threading.Event().wait(10)
        if self._raises is not None:
            raise self._raises
        self.delivered.append(text)
        return self._result

    def close(self) -> None:
        self.closed = True


class SinkCommandTests(unittest.TestCase):
    def test_explicit_command_wins(self) -> None:
        self.assertEqual(sink_module.sink_command("/custom/helper --devices keyboard"),
                         "/custom/helper --devices keyboard")

    def test_environment_is_used_when_no_explicit_command(self) -> None:
        original = sink_module.os.environ.get(sink_module.SINK_COMMAND_ENVIRONMENT)
        sink_module.os.environ[sink_module.SINK_COMMAND_ENVIRONMENT] = "/env/helper"
        try:
            self.assertEqual(sink_module.sink_command(), "/env/helper")
        finally:
            if original is None:
                sink_module.os.environ.pop(sink_module.SINK_COMMAND_ENVIRONMENT, None)
            else:
                sink_module.os.environ[sink_module.SINK_COMMAND_ENVIRONMENT] = original

    def test_default_command_requests_pointer_and_keyboard(self) -> None:
        self.assertIn("--devices", sink_module.DEFAULT_SINK_COMMAND)
        self.assertIn("keyboard", sink_module.DEFAULT_SINK_COMMAND)


class DeliveryResultEvidenceTests(unittest.TestCase):
    def test_successful_result_is_projected(self) -> None:
        evidence = sink_module.delivery_result_evidence(
            _StubResult(ok=True, detail="Acknowledged request abc.")
        )
        self.assertTrue(evidence["ok"])
        self.assertEqual(evidence["method"], "agent-input")
        self.assertFalse(evidence["uncertain"])

    def test_uncertain_result_is_carried_through(self) -> None:
        """An uncertain failure must stay distinguishable from a clean one."""

        evidence = sink_module.delivery_result_evidence(
            _StubResult(ok=False, uncertain=True, detail="may be partially delivered")
        )
        self.assertFalse(evidence["ok"])
        self.assertTrue(evidence["uncertain"])


class ConcurrentApprovalTests(unittest.TestCase):
    def test_approval_runs_while_deliver_blocks(self) -> None:
        sink = _StubSink()

        def approve():
            sink.approved.set()
            return (10.0, 20.0)

        result, error, approval = sink_module.deliver_with_concurrent_approval(
            sink, "payload ✓", approve
        )
        self.assertIsNone(error)
        self.assertTrue(result.ok)
        self.assertEqual(sink.delivered, ["payload ✓"])
        self.assertEqual(approval, (10.0, 20.0))

    def test_sink_exception_is_reported_not_raised(self) -> None:
        sink = _StubSink(raises=RuntimeError("helper vanished"))

        result, error, _ = sink_module.deliver_with_concurrent_approval(
            sink, "payload", lambda: sink.approved.set()
        )
        self.assertIsNone(result)
        self.assertIn("helper vanished", error)

    def test_hung_delivery_times_out_with_an_error(self) -> None:
        sink = _StubSink(never_returns=True)

        result, error, _ = sink_module.deliver_with_concurrent_approval(
            sink, "payload", lambda: sink.approved.set(), timeout=0.5
        )
        self.assertIsNone(result)
        self.assertIn("did not return", error)


class SinkContractTests(unittest.TestCase):
    """Pin the real Gabbee sink behaviour the proof's design depends on."""

    def setUp(self) -> None:
        try:
            self.sink_class = sink_module.import_real_sink()
        except sink_module.TerminalSinkError as error:
            self.skipTest(f"Gabbee checkout not on PYTHONPATH: {error}")

    def test_real_sink_exposes_the_lifecycle_the_adapter_drives(self) -> None:
        for name in ("deliver", "deliver_key", "close", "from_environment"):
            self.assertTrue(callable(getattr(self.sink_class, name, None)), name)

    def test_real_sink_refuses_key_chords(self) -> None:
        """The framing helper session exists only because of this refusal.

        If Gabbee ever gains a real key route this test fails, and the proof
        should deliver Return/Ctrl-D through the sink instead of the
        separately approved direct helper session.
        """

        sink = self.sink_class("/nonexistent/helper")
        result = sink.deliver_key("Return")
        self.assertFalse(result.ok)
        self.assertEqual(result.method, "agent-input")


if __name__ == "__main__":
    unittest.main()
