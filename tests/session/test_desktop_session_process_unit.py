# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import shutil
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

from desktop_session_process import (
    ProcessContractError,
    capture_process_identity,
    identity_is_live,
    terminate_processes,
)
from desktop_session_runtime import RuntimeState, _cleanup


def write_process(proc: Path, pid: int, executable: Path, start_ticks: int) -> Path:
    root = proc / str(pid)
    root.mkdir(parents=True)
    (root / "exe").symlink_to(executable)
    fields = ["S"] + ["0"] * 18 + [str(start_ticks)] + ["0"] * 8
    (root / "stat").write_text(f"{pid} (name with spaces) " + " ".join(fields))
    return root


class ProcessTests(unittest.TestCase):
    def test_capture_binds_executable_and_starttime(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            proc = root / "proc"
            proc.mkdir()
            executable = root / "service"
            executable.write_text("x")
            write_process(proc, 42, executable, 900)
            identity = capture_process_identity("service", 42, [executable], proc_root=proc)
            self.assertEqual(identity.start_ticks, 900)
            self.assertTrue(identity_is_live(identity, proc_root=proc))

    def test_wrong_executable_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            proc = root / "proc"
            proc.mkdir()
            actual = root / "actual"
            allowed = root / "allowed"
            actual.write_text("x")
            allowed.write_text("x")
            write_process(proc, 44, actual, 100)
            with self.assertRaisesRegex(ProcessContractError, "outside"):
                capture_process_identity("service", 44, [allowed], proc_root=proc)

    def test_term_removes_exact_identity(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            proc = root / "proc"
            proc.mkdir()
            executable = root / "service"
            executable.write_text("x")
            process = write_process(proc, 46, executable, 101)
            identity = capture_process_identity("service", 46, [executable], proc_root=proc)
            signals: list[tuple[int, int]] = []

            def signal_group(group: int, signum: int) -> None:
                signals.append((group, signum))
                shutil.rmtree(process)

            self.assertEqual(
                terminate_processes(
                    [identity],
                    proc_root=proc,
                    signal_group=signal_group,
                    sleep=lambda _: None,
                ),
                [],
            )
            self.assertEqual(len(signals), 1)

    def test_reused_pid_is_never_signalled(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            proc = root / "proc"
            proc.mkdir()
            executable = root / "service"
            executable.write_text("x")
            process = write_process(proc, 48, executable, 102)
            identity = capture_process_identity("service", 48, [executable], proc_root=proc)
            (process / "stat").unlink()
            fields = ["S"] + ["0"] * 18 + ["103"] + ["0"] * 8
            (process / "stat").write_text("48 (reused) " + " ".join(fields))
            signals: list[tuple[int, int]] = []
            terminate_processes(
                [identity],
                proc_root=proc,
                signal_group=lambda group, signum: signals.append((group, signum)),
                sleep=lambda _: None,
            )
            self.assertEqual(signals, [])

    def test_live_direct_process_that_cannot_be_authenticated_fails_cleanup(self) -> None:
        process = Mock(pid=52)
        process.poll.return_value = None
        state = RuntimeState(
            processes=[process],
            spawned=[(process, [Path("/staged/service")])],
        )
        with patch(
            "desktop_session_runtime.capture_process_identity",
            side_effect=ProcessContractError("executable changed"),
        ):
            with self.assertRaisesRegex(ProcessContractError, "exact desktop cleanup"):
                _cleanup(state)


if __name__ == "__main__":
    unittest.main()
