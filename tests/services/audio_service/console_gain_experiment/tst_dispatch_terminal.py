# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual terminal boundary with injected sinks/clock; no child or OS signal."""
import unittest
from dispatch_evidence import DispatchBudget, emit_terminal
from process_evidence import ProcessCancelled


class Clock:
    def __init__(self): self.now = 0.0
    def __call__(self): return self.now


def refuse(error):
    raise error


class DispatchTerminal(unittest.TestCase):
    def setUp(self):
        self.clock = Clock()
        self.budget = DispatchBudget(10, 5, clock=self.clock)
        self.budget.finish_attempt()
        self.terminal = {"success": False}

    def test_prior_primary_and_cleanup_survive_broken_pipe_or_full_sink(self):
        for sink in (BrokenPipeError("pipe"), OSError("full sink")):
            with self.subTest(sink=type(sink).__name__):
                primary = RuntimeError("build")
                cleanup = PermissionError("kill")
                earlier = ValueError("final pins")
                primary.cleanup_errors = (cleanup,)
                primary.evidence_errors = (earlier,)
                with self.assertRaises(RuntimeError) as caught:
                    emit_terminal(self.terminal, primary, lambda row: refuse(sink),
                                  self.budget.evidence_cancelled)
                self.assertIs(caught.exception, primary)
                self.assertEqual(primary.cleanup_errors, (cleanup,))
                self.assertEqual(primary.evidence_errors, (earlier, sink))

    def test_prior_primary_survives_late_emission(self):
        primary = RuntimeError("build")
        def emit(row): self.clock.now = 6
        with self.assertRaises(RuntimeError) as caught:
            emit_terminal(self.terminal, primary, emit, self.budget.evidence_cancelled)
        self.assertIs(caught.exception, primary)
        self.assertIsInstance(primary.evidence_errors[0], ProcessCancelled)
        self.assertEqual(self.budget.request_reason, "evidence-deadline")

    def test_no_primary_keeps_first_sink_failure_before_late_refusal(self):
        sink = BrokenPipeError("first terminal failure")
        def emit(row):
            self.clock.now = 6
            raise sink
        with self.assertRaises(BrokenPipeError) as caught:
            emit_terminal(self.terminal, None, emit, self.budget.evidence_cancelled)
        self.assertIs(caught.exception, sink)
        self.assertIsInstance(sink.evidence_errors[0], ProcessCancelled)

    def test_no_primary_late_emission_refuses(self):
        def emit(row): self.clock.now = 6
        with self.assertRaises(ProcessCancelled) as caught:
            emit_terminal(self.terminal, None, emit, self.budget.evidence_cancelled)
        self.assertEqual(str(caught.exception), "terminal emission completed late or cancelled")
        self.assertEqual(caught.exception.evidence_errors, ())
        self.assertEqual(self.budget.request_reason, "evidence-deadline")

    def test_diagnostic_sink_failure_cannot_replace_either_primary(self):
        for earlier in (None, RuntimeError("build")):
            with self.subTest(hasBuildPrimary=earlier is not None):
                sink = BrokenPipeError("terminal")
                diagnostic = OSError("diagnostic sink")
                selected = earlier if earlier is not None else sink
                with self.assertRaises(type(selected)) as caught:
                    emit_terminal(self.terminal, earlier, lambda row: refuse(sink),
                                  self.budget.evidence_cancelled,
                                  diagnose=lambda text: refuse(diagnostic))
                self.assertIs(caught.exception, selected)
                expected = (sink, diagnostic) if earlier is not None else (diagnostic,)
                self.assertEqual(selected.evidence_errors, expected)


if __name__ == "__main__": unittest.main()
