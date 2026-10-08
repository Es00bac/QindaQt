"""Only injected subprocesses; no guest, bus or process is started."""
import subprocess
import unittest
from unittest.mock import patch
import windows


class ReadinessTests(unittest.TestCase):
    def invoke(self, remaining=190, boot=False, effect=None):
        with patch.object(windows.time, "monotonic", return_value=100), patch.object(
                windows.subprocess, "run", side_effect=effect) as run:
            if effect is None:
                run.return_value.stdout = "1\n"
            value = windows.run_command(("prop", "get", "sys.boot_completed"),
                                        100 + remaining, boot_readiness=boot)
            return value, run.call_args

    def test_boot_uses_existing_remaining_budget(self):
        value, call = self.invoke(190, True)
        self.assertEqual(value, "1")
        self.assertEqual(call.kwargs["timeout"], 190)

    def test_other_calls_keep_ten_second_bound(self):
        _, call = self.invoke(190)
        self.assertEqual(call.kwargs["timeout"], 10)

    def test_last_budget_bounds_every_command(self):
        for boot in (False, True):
            _, call = self.invoke(2.5, boot)
            self.assertEqual(call.kwargs["timeout"], 2.5)

    def test_expired_budget_never_dispatches(self):
        with patch.object(windows.time, "monotonic", return_value=100), patch.object(
                windows.subprocess, "run") as run:
            with self.assertRaisesRegex(RuntimeError, "window-proof-deadline"):
                windows.run_command(("prop", "get", "sys.boot_completed"), 100,
                                    boot_readiness=True)
            run.assert_not_called()

    def test_timeout_preserved_once_without_retry(self):
        error = subprocess.TimeoutExpired(["waydroid"], 190, output=b"boot", stderr=b"wait")
        with patch.object(windows.time, "monotonic", return_value=100), patch.object(
                windows.subprocess, "run", side_effect=error) as run:
            with self.assertRaises(subprocess.TimeoutExpired) as caught:
                windows.run_command(("prop", "get", "sys.boot_completed"), 290,
                                    boot_readiness=True)
            self.assertIs(caught.exception, error)
            self.assertEqual(run.call_count, 1)

    def test_fixed_launches_use_shared_remaining_budget_once(self):
        for app in windows.APPS:
            with patch.object(windows.time, "monotonic", return_value=100), patch.object(
                    windows.subprocess, "run") as run:
                run.return_value.stdout = ""
                windows.run_command(("app", "launch", app), 235)
                self.assertEqual(run.call_args.kwargs["timeout"], 135)
                self.assertEqual(run.call_count, 1)

    def test_unknown_or_extended_launch_retains_ten_seconds(self):
        for args in (("app", "launch", "foreign.app"),
                     ("app", "launch", windows.APPS[0], "--extra"),
                     ("app", "remove", windows.APPS[0])):
            with patch.object(windows.time, "monotonic", return_value=100), patch.object(
                    windows.subprocess, "run") as run:
                run.return_value.stdout = ""
                windows.run_command(args, 290)
                self.assertEqual(run.call_args.kwargs["timeout"], 10)

    def test_fixed_launch_expired_budget_never_dispatches(self):
        with patch.object(windows.time, "monotonic", return_value=100), patch.object(
                windows.subprocess, "run") as run:
            with self.assertRaisesRegex(RuntimeError, "window-proof-deadline"):
                windows.run_command(("app", "launch", windows.APPS[1]), 100)
            run.assert_not_called()

    def test_launch_timeout_preserves_error_without_retry(self):
        error = subprocess.TimeoutExpired(["waydroid"], 135)
        with patch.object(windows.time, "monotonic", return_value=100), patch.object(
                windows.subprocess, "run", side_effect=error) as run:
            with self.assertRaises(subprocess.TimeoutExpired) as caught:
                windows.run_command(("app", "launch", windows.APPS[1]), 235)
            self.assertIs(caught.exception, error)
            self.assertEqual(run.call_count, 1)

    def test_diagnostics_are_bounded_and_decode_bytes(self):
        self.assertEqual(windows.bounded_output(None), "")
        self.assertEqual(len(windows.bounded_output(b"x" * 3000)), 2048)
        self.assertIn("\ufffd", windows.bounded_output(b"\xff"))


if __name__ == "__main__":
    unittest.main()
