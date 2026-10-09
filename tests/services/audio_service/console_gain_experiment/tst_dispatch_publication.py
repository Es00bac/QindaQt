# SPDX-License-Identifier: GPL-3.0-or-later
"""Injected evidence callbacks/clock; no child, OS signal or package access."""
import signal
import unittest
from dispatch_evidence import DispatchBudget, finish_dispatch
from process_evidence import ProcessCancelled


class Clock:
    def __init__(self): self.now = 0.0
    def __call__(self): return self.now


class DispatchPublication(unittest.TestCase):
    def setUp(self):
        self.clock = Clock(); self.budget = DispatchBudget(10, 5, clock=self.clock)
        self.clock.now = 2
        self.stored = []
        self.final = lambda: None
        self.index = lambda: {"raw": "synthetic"}
        self.write = lambda row: self.stored.append(row)

    def finish(self, original=None):
        return finish_dispatch(self.budget, {"success": True}, original,
                               self.final, self.index, self.write)

    def denied(self):
        terminal, error = self.finish()
        self.assertFalse(terminal["success"]); self.assertIsNotNone(error)
        self.assertFalse(self.stored[0]["success"])
        return terminal, error

    def test_success_duration_includes_actual_final_result_write(self):
        self.final = lambda: setattr(self.clock, "now", 3)
        self.index = lambda: setattr(self.clock, "now", 4)
        def write(row): self.stored.append(row); self.clock.now = 6
        self.write = write
        terminal, error = self.finish()
        self.assertIsNone(error); self.assertTrue(terminal["success"])
        self.assertEqual(terminal["completedSeconds"], 6)
        self.assertEqual(terminal["attemptedBuildSeconds"], 2)
        self.assertEqual(terminal["cleanupEvidenceSeconds"], 4)
        self.assertFalse(self.stored[0]["success"])
        self.assertTrue(self.stored[0]["successCandidate"])
        self.assertTrue(terminal["requiresManagerOwnedSettlement"])

    def test_deadline_crossed_during_final_pin_work(self):
        self.final = lambda: setattr(self.clock, "now", 7)
        terminal, _ = self.denied()
        self.assertEqual(terminal["completedSeconds"], 7)
        self.assertEqual(terminal["cancelReason"], "evidence-deadline")

    def test_deadline_crossed_during_raw_index(self):
        self.index = lambda: setattr(self.clock, "now", 8)
        terminal, _ = self.denied()
        self.assertEqual(terminal["completedSeconds"], 8)

    def test_deadline_crossed_during_result_write_cannot_leave_success_file(self):
        def write(row): self.stored.append(row); self.clock.now = 9
        self.write = write
        terminal, error = self.denied()
        self.assertIsInstance(error, ProcessCancelled)
        self.assertEqual(terminal["completedSeconds"], 9)
        self.assertTrue(self.stored[0]["successCandidate"])

    def test_latched_signal_during_final_pins_denies(self):
        self.final = lambda: self.budget.request(signal.SIGTERM)
        _, error = self.denied(); self.assertIsInstance(error, ProcessCancelled)

    def test_latched_signal_during_index_denies(self):
        self.index = lambda: self.budget.request(signal.SIGINT)
        _, error = self.denied(); self.assertIsInstance(error, ProcessCancelled)

    def test_latched_signal_during_result_write_denies(self):
        def write(row): self.stored.append(row); self.budget.request(signal.SIGTERM)
        self.write = write
        _, error = self.denied(); self.assertIsInstance(error, ProcessCancelled)

    def test_primary_build_error_survives_all_final_errors(self):
        original = OSError("first build")
        errors = [ValueError("pins"), ValueError("index"), OSError("write")]
        def fail(error): raise error
        self.final = lambda: fail(errors[0]); self.index = lambda: fail(errors[1])
        self.write = lambda row: fail(errors[2])
        terminal, caught = self.finish(original)
        self.assertIs(caught, original)
        self.assertEqual(original.evidence_errors, tuple(errors))
        self.assertFalse(terminal["success"]); self.assertEqual(len(original.__notes__), 3)

    def test_first_final_error_survives_secondary_index_and_write_errors(self):
        errors = [ValueError("pins"), ValueError("index"), OSError("write")]
        def fail(error): raise error
        self.final = lambda: fail(errors[0]); self.index = lambda: fail(errors[1])
        self.write = lambda row: fail(errors[2])
        terminal, caught = self.finish()
        self.assertIs(caught, errors[0])
        self.assertEqual(caught.evidence_errors, tuple(errors[1:]))
        self.assertFalse(terminal["success"])

    def test_write_error_refuses_without_erasing_it(self):
        error = OSError("write")
        def write(row): raise error
        self.write = write
        terminal, caught = self.finish()
        self.assertIs(caught, error); self.assertFalse(terminal["success"])

    def test_evidence_predicate_requires_attempt_retirement(self):
        with self.assertRaisesRegex(RuntimeError, "settle"):
            self.budget.evidence_cancelled()

    def test_finish_attempt_cannot_extend_evidence_budget(self):
        self.budget.finish_attempt()
        deadline = self.budget.evidence_deadline
        self.clock.now = 4
        self.budget.finish_attempt()
        self.assertEqual(self.budget.evidence_deadline, deadline)
        self.clock.now = deadline
        self.assertTrue(self.budget.evidence_cancelled())

    def test_late_attempt_refuses_all_success_even_if_evidence_is_fast(self):
        self.clock.now = 11
        terminal, _ = self.denied()
        self.assertEqual(terminal["cancelReason"], "attempt-deadline")
        self.assertEqual(terminal["attemptedBuildSeconds"], 11)

    def test_no_final_callbacks_after_preexisting_cancel(self):
        calls = []
        self.budget.request(signal.SIGTERM)
        self.final = lambda: calls.append("final")
        self.index = lambda: calls.append("index")
        self.denied()
        self.assertEqual(calls, [])


if __name__ == "__main__": unittest.main()
