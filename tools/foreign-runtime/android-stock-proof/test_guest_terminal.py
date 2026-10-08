"""Injected console/flush/drain controls; never operate a real TTY or child."""
from contextlib import ExitStack
import unittest
from unittest.mock import patch
import guest


class TerminalTests(unittest.TestCase):
    def setup_calls(self, stack, tty=True, name="/dev/console"):
        events = []
        stack.enter_context(patch.object(guest.sys.stdout, "fileno", return_value=1))
        stack.enter_context(patch.object(guest.os, "isatty", return_value=tty))
        stack.enter_context(patch.object(guest.os, "ttyname", return_value=name))
        stack.enter_context(patch.object(guest.termios, "tcgetattr",
                                        side_effect=lambda fd: events.append("attributes")))
        stack.enter_context(patch.object(guest.os, "sync",
                                        side_effect=lambda: events.append("sync")))
        output = stack.enter_context(patch("builtins.print",
                                          side_effect=lambda *a, **kw: events.append(
                                              ("print", a[0], kw.get("flush")))))
        drain = stack.enter_context(patch.object(guest.termios, "tcdrain",
                                                  side_effect=lambda fd: events.append("drain")))
        hold = stack.enter_context(patch.object(guest, "quarantine_terminal_failure",
                                                 side_effect=RuntimeError("held-until-host-deadline")))
        return events, output, drain, hold

    def test_sync_then_print_flush_then_drain(self):
        with ExitStack() as stack:
            events, output, drain, hold = self.setup_calls(stack)
            guest.emit_terminal({"success": True})
            self.assertEqual(events[:2], ["attributes", "sync"])
            self.assertEqual(events[2], ("print", 'QINDA_ANDROID_RESULT={"success": true}', True))
            self.assertEqual(events[3], "drain")
            drain.assert_called_once_with(1)
            hold.assert_not_called()

    def test_non_tty_refuses_before_publication(self):
        with ExitStack() as stack:
            _, output, drain, hold = self.setup_calls(stack, tty=False)
            with self.assertRaisesRegex(RuntimeError, "terminal-not-guest-console"):
                guest.emit_terminal({"success": True})
            output.assert_not_called(); drain.assert_not_called(); hold.assert_not_called()

    def test_other_tty_refuses_before_publication(self):
        with ExitStack() as stack:
            _, output, drain, hold = self.setup_calls(stack, name="/dev/pts/0")
            with self.assertRaisesRegex(RuntimeError, "terminal-not-guest-console"):
                guest.emit_terminal({"success": True})
            output.assert_not_called(); drain.assert_not_called(); hold.assert_not_called()

    def test_termios_refusal_never_publishes(self):
        with ExitStack() as stack:
            _, output, drain, hold = self.setup_calls(stack)
            stack.enter_context(patch.object(guest.termios, "tcgetattr", side_effect=OSError("unsupported")))
            with self.assertRaises(OSError):
                guest.emit_terminal({"success": True})
            output.assert_not_called(); drain.assert_not_called(); hold.assert_not_called()

    def test_drain_failure_holds_and_preserves_original_result(self):
        with ExitStack() as stack:
            events, output, drain, hold = self.setup_calls(stack)
            drain.side_effect = OSError("failed")
            result = {"success": False, "errorType": "OriginalFailure"}
            with self.assertRaisesRegex(RuntimeError, "held-until-host-deadline"):
                guest.emit_terminal(result)
            self.assertEqual(result, {"success": False, "errorType": "OriginalFailure"})
            self.assertEqual(output.call_count, 2)
            hold.assert_called_once()
            self.assertEqual(events[-1][1], "QINDA_ANDROID_TERMINAL_ERROR=OSError")

    def test_print_and_diagnostic_failure_still_holds(self):
        with ExitStack() as stack:
            _, output, drain, hold = self.setup_calls(stack)
            output.side_effect = OSError("sink")
            with self.assertRaisesRegex(RuntimeError, "held-until-host-deadline"):
                guest.emit_terminal({"success": True})
            drain.assert_not_called(); hold.assert_called_once()


if __name__ == "__main__":
    unittest.main()
