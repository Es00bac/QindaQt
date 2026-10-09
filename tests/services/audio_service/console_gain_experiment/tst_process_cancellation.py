# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual runner with injected Popen/wait/kill/clock; no child or OS signal."""
import signal
import subprocess
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch
import process_evidence as owning
from dispatch_evidence import DispatchBudget


class Clock:
    def __init__(self): self.now = 0.0
    def __call__(self): return self.now


class ProcessCancellation(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="audio-pure-runner-")
        self.root = Path(self.temporary.name)
        self.clock = Clock(); self.budget = DispatchBudget(10, 5, clock=self.clock)
        self.process = Mock(pid=4711); self.process.wait.return_value = 0
        self.spawn = self.enterContext(patch.object(owning.subprocess, "Popen", return_value=self.process))
        self.poll = self.enterContext(patch.object(owning.os, "waitid", return_value=object()))
        self.kill = self.enterContext(patch.object(owning.os, "killpg"))
        self.enterContext(patch.object(owning.time, "monotonic", self.clock))
        self.enterContext(patch.object(owning.time, "sleep", return_value=None))

    def tearDown(self): self.temporary.cleanup()

    def run_phase(self, name="compile", *, injected=True):
        return owning.run_logged(["fixture", name], {"PATH": "/fixture"}, self.root,
            self.root, name, 10, cancellation=self.budget.attempt_cancelled if injected else None)

    def settled(self):
        self.kill.assert_called_once_with(4711, signal.SIGKILL)
        self.process.wait.assert_called_once_with(timeout=5)

    def test_default_exit_preserves_original_result_fields(self):
        row = self.run_phase(injected=False)
        self.assertEqual(set(row), {"name", "argv", "exit", "timedOut", "ownedProcessGroup",
                                   "leaderReservedThroughCleanup", "log", "sha256"})
        self.assertEqual(row["exit"], 0); self.assertFalse(row["timedOut"])
        self.settled()

    def test_default_timeout_keeps_term_grace_kill_reap(self):
        with patch.object(owning, "observe_exit", side_effect=[False, True]):
            row = self.run_phase(injected=False)
        self.assertTrue(row["timedOut"])
        self.assertEqual([call.args for call in self.kill.call_args_list],
                         [(4711, signal.SIGTERM), (4711, signal.SIGKILL)])
        self.process.wait.assert_called_once_with(timeout=5)

    def test_prelaunch_request_does_not_acquire(self):
        self.budget.request(signal.SIGTERM)
        with self.assertRaises(owning.ProcessCancelled): self.run_phase()
        self.spawn.assert_not_called(); self.kill.assert_not_called(); self.process.wait.assert_not_called()

    def test_prelaunch_deadline_does_not_acquire(self):
        self.clock.now = 10
        with self.assertRaises(owning.ProcessCancelled): self.run_phase()
        self.spawn.assert_not_called(); self.kill.assert_not_called()

    def test_postacquisition_request_still_kills_and_reaps(self):
        def acquire(*args, **kwargs):
            self.budget.request(signal.SIGTERM)
            return self.process
        self.spawn.side_effect = acquire
        with self.assertRaises(owning.ProcessCancelled): self.run_phase()
        self.settled()

    def test_poll_request_still_kills_and_reaps(self):
        count = 0
        def poll(*args):
            nonlocal count
            count += 1
            if count == 1:
                self.budget.request(signal.SIGTERM); return None
            return object()
        self.poll.side_effect = poll
        with self.assertRaises(owning.ProcessCancelled): self.run_phase()
        self.settled()

    def test_observed_exit_request_cannot_report_success(self):
        def poll(*args):
            self.budget.request(signal.SIGTERM); return object()
        self.poll.side_effect = poll
        with self.assertRaises(owning.ProcessCancelled): self.run_phase()
        self.settled()

    def test_group_cleanup_request_does_not_interrupt_reap(self):
        self.kill.side_effect = lambda *args: self.budget.request(signal.SIGTERM)
        with self.assertRaises(owning.ProcessCancelled): self.run_phase()
        self.settled()

    def test_wait_request_does_not_interrupt_reap(self):
        def wait(*args, **kwargs):
            self.budget.request(signal.SIGTERM); return 0
        self.process.wait.side_effect = wait
        with self.assertRaises(owning.ProcessCancelled): self.run_phase()
        self.settled()

    def test_deadline_during_group_cleanup_refuses_after_reap(self):
        def kill(*args): self.clock.now = 11
        self.kill.side_effect = kill
        with self.assertRaises(owning.ProcessCancelled): self.run_phase()
        self.settled()

    def test_deadline_during_wait_refuses_after_reap(self):
        def wait(*args, **kwargs): self.clock.now = 11; return 0
        self.process.wait.side_effect = wait
        with self.assertRaises(owning.ProcessCancelled): self.run_phase()
        self.settled()

    def test_cancelled_first_phase_never_dispatches_later_phase(self):
        self.process.wait.side_effect = lambda **kwargs: self.budget.request(signal.SIGTERM) or 0
        with self.assertRaises(owning.ProcessCancelled):
            for name in ["compile", "test", "image"]: self.run_phase(name)
        self.spawn.assert_called_once()
        self.assertEqual(self.spawn.call_args.args[0], ["fixture", "compile"])
        self.settled()

    def test_primary_poll_and_both_cleanup_errors_remain_distinct(self):
        primary = OSError("primary poll")
        kill = PermissionError("cleanup kill")
        wait = subprocess.TimeoutExpired("fake", 5)
        self.poll.side_effect = [primary, object()]
        self.kill.side_effect = kill; self.process.wait.side_effect = wait
        with self.assertRaises(OSError) as caught: self.run_phase()
        self.assertIs(caught.exception, primary)
        self.assertEqual(primary.cleanup_errors, (kill, wait))
        self.assertEqual(len(primary.__notes__), 2)
        self.process.wait.assert_called_once()

    def test_cleanup_kill_failure_does_not_skip_wait(self):
        error = PermissionError("kill denied")
        self.kill.side_effect = error
        with self.assertRaises(PermissionError) as caught: self.run_phase()
        self.assertIs(caught.exception, error); self.process.wait.assert_called_once()

    def test_cleanup_wait_failure_cannot_return_success(self):
        error = subprocess.TimeoutExpired("fake", 5); self.process.wait.side_effect = error
        with self.assertRaises(subprocess.TimeoutExpired) as caught: self.run_phase()
        self.assertIs(caught.exception, error)

    def test_lost_reservation_in_poll_never_signals_group(self):
        self.poll.side_effect = ChildProcessError("lost")
        with self.assertRaisesRegex(RuntimeError, "unexpectedly reaped"): self.run_phase()
        self.kill.assert_not_called(); self.process.wait.assert_not_called()

    def test_lost_reservation_before_cleanup_never_signals_group(self):
        self.poll.side_effect = [object(), ChildProcessError("lost")]
        with self.assertRaises(ChildProcessError): self.run_phase()
        self.kill.assert_not_called(); self.process.wait.assert_not_called()

    def test_failed_acquisition_preserves_error_without_cleanup_on_cached_pid(self):
        error = OSError("acquire failed"); self.spawn.side_effect = error
        with self.assertRaises(OSError) as caught: self.run_phase()
        self.assertIs(caught.exception, error)
        self.kill.assert_not_called(); self.process.wait.assert_not_called()

    def test_already_gone_group_still_reaps_reserved_leader(self):
        self.kill.side_effect = ProcessLookupError()
        self.assertEqual(self.run_phase()["exit"], 0)
        self.process.wait.assert_called_once()

    def test_primary_failure_survives_log_close_error(self):
        primary = OSError("poll"); close = OSError("log close")
        output = Mock(); output.close.side_effect = close
        self.poll.side_effect = [primary, object()]
        with patch.object(Path, "open", return_value=output):
            with self.assertRaises(OSError) as caught: self.run_phase()
        self.assertIs(caught.exception, primary)
        self.assertEqual(primary.cleanup_errors, (close,))
        self.settled()

    def test_request_during_log_hash_cannot_report_success(self):
        def read(path):
            self.budget.request(signal.SIGTERM)
            return b"synthetic log"
        with patch.object(Path, "read_bytes", read):
            with self.assertRaises(owning.ProcessCancelled): self.run_phase()
        self.settled()


if __name__ == "__main__": unittest.main()
