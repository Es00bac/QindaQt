# SPDX-License-Identifier: GPL-3.0-or-later
"""Injected control-flow checks only; real process and signal APIs are poisoned."""
import importlib.util
import os
from pathlib import Path
import signal
import subprocess
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("recovery_owned", Path(__file__).with_name("run_recovery_owned.py"))
runner = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = runner
spec.loader.exec_module(runner)

class Fake:
    def __init__(self, observations):
        self.observations = iter(observations)
        self.calls = []
        self.cancelled = False
        self.now = 0
        self.send_errors = {}
        self.wait_error = None
        self.acquire_error = None
        self.cancel_acquire = False
        self.cancel_term = False
        self.ports = SimpleNamespace(acquire=self.acquire, observe=self.observe,
            send=self.send, clock=self.clock, sleep=self.sleep)
    def acquire(self, command):
        self.calls.append(("acquire", command))
        if self.acquire_error:
            raise self.acquire_error
        self.cancelled = self.cancel_acquire
        return SimpleNamespace(pid=12345, wait=self.wait)
    def observe(self, pid):
        self.calls.append(("observe", pid))
        value = next(self.observations, None)
        if isinstance(value, Exception):
            raise value
        return value
    def send(self, pid, number):
        self.calls.append(("send", pid, number))
        if number == signal.SIGTERM and self.cancel_term:
            self.cancelled = True
        if number in self.send_errors:
            raise self.send_errors[number]
    def clock(self):
        return self.now
    def sleep(self, amount):
        self.calls.append(("sleep", amount))
        self.now += amount
    def wait(self, timeout):
        self.calls.append(("reap", timeout))
        if self.wait_error:
            raise self.wait_error
        return 0
    def run(self):
        poison = AssertionError("real process/signal API forbidden in injected control")
        with patch.object(os, "waitid", side_effect=poison), patch.object(os, "killpg", side_effect=poison), \
             patch.object(subprocess, "Popen", side_effect=poison), patch.object(signal, "signal", side_effect=poison):
            return runner.execute(["fixture"], self.ports, lambda: self.cancelled)
    def signals(self):
        return [call[2] for call in self.calls if call[0] == "send"]

def exited(code=0):
    return SimpleNamespace(si_code=os.CLD_EXITED, si_status=code)

class RunnerControls(unittest.TestCase):
    def test_success(self):
        f = Fake([exited(), exited(), exited()])
        r = f.run()
        self.assertEqual(r.code, 0); self.assertFalse(r.primary); self.assertFalse(r.cleanup)
        self.assertEqual(f.signals(), [signal.SIGTERM, signal.SIGKILL])
        self.assertEqual(f.calls[-1], ("reap", 2))
    def test_nonzero_early_exit(self):
        f = Fake([exited(7), exited(7), exited(7)])
        self.assertEqual(f.run().code, 7)
    def test_loss_before_term_never_signals(self):
        f = Fake([exited(), ChildProcessError("lost")])
        r = f.run()
        self.assertEqual(f.signals(), [])
        self.assertEqual(r.code, 125)
        self.assertTrue(any("TERM admission" in e for e in r.cleanup))
        self.assertEqual(f.calls[-1], ("reap", 2))
    def test_loss_between_signals(self):
        f = Fake([exited(), exited(), ChildProcessError("lost")])
        r = f.run()
        self.assertEqual(f.signals(), [signal.SIGTERM])
        self.assertTrue(any("KILL admission" in e for e in r.cleanup))
    def test_wait_reservation_loss_no_signals(self):
        f = Fake([ChildProcessError("lost")])
        r = f.run()
        self.assertIn("ChildProcessError", r.primary)
        self.assertEqual(f.signals(), [])
    def test_term_error_keeps_primary_and_attempts_kill_reap(self):
        f = Fake([RuntimeError("primary"), None, None])
        f.send_errors[signal.SIGTERM] = PermissionError("term")
        r = f.run()
        self.assertIn("primary", r.primary); self.assertIn("TERM", r.cleanup[0])
        self.assertEqual(f.signals(), [signal.SIGTERM, signal.SIGKILL])
        self.assertEqual(f.calls[-1], ("reap", 2))
    def test_kill_and_reap_errors_separate(self):
        f = Fake([exited(), exited(), exited()])
        f.send_errors[signal.SIGKILL] = PermissionError("kill")
        f.wait_error = subprocess.TimeoutExpired("fixture", 2)
        r = f.run()
        self.assertTrue(any("KILL:" in e for e in r.cleanup))
        self.assertTrue(any("reap:" in e for e in r.cleanup))
        self.assertEqual(r.code, 125)
    def test_cancel_during_acquisition(self):
        f = Fake([None, None]); f.cancel_acquire = True
        r = f.run()
        self.assertEqual(r.primary, "cancelled"); self.assertEqual(r.code, 130)
        self.assertEqual(f.signals(), [signal.SIGTERM, signal.SIGKILL])
    def test_cancel_during_cleanup(self):
        f = Fake([exited(), exited(), exited()]); f.cancel_term = True
        r = f.run()
        self.assertEqual(r.code, 130)
        self.assertEqual(f.signals(), [signal.SIGTERM, signal.SIGKILL])
        self.assertEqual(f.calls[-1], ("reap", 2))
    def test_acquisition_error(self):
        f = Fake([]); f.acquire_error = RuntimeError("acquire")
        r = f.run()
        self.assertIn("acquire", r.primary); self.assertEqual(f.signals(), [])
    def test_timeout(self):
        f = Fake([])
        f.sleep = lambda amount: setattr(f, "now", f.now + 51)
        f.ports.sleep = f.sleep
        r = f.run()
        self.assertEqual(r.primary, "timeout"); self.assertEqual(r.code, 124)
        self.assertEqual(f.signals(), [signal.SIGTERM, signal.SIGKILL])
    def test_lookup_refusal_does_not_skip_reap(self):
        f = Fake([exited(), exited(), exited()])
        f.send_errors[signal.SIGTERM] = ProcessLookupError("absent")
        f.run()
        self.assertEqual(f.calls[-1], ("reap", 2))

if __name__ == "__main__":
    unittest.main()
