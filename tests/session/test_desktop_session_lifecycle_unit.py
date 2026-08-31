# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import json
import os
import signal
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from desktop_session_lifecycle import LIFECYCLE_TRACE_ENVIRONMENT
from desktop_session_runtime import _session_program
from test_desktop_session_nested import _enable_lifecycle_diagnostics


HERE = Path(__file__).resolve().parent
TRACER = HERE / "desktop_session_lifecycle.py"


def trace_documents(path: Path) -> list[dict[str, object]]:
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]


class LifecycleTraceTests(unittest.TestCase):
    def run_trace(
        self, trace: Path, arguments: list[str], *, exec_only: bool = False
    ) -> subprocess.CompletedProcess[str]:
        command = [
            sys.executable,
            str(TRACER),
            "--role",
            "shell",
            "--executable",
            sys.executable,
            "--trace",
            str(trace),
        ]
        if exec_only:
            command.append("--exec-only")
        command.extend(["--", *arguments])
        return subprocess.run(command, text=True, capture_output=True, check=False)

    def test_child_exit_code_and_order_are_recorded(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            trace = Path(directory) / "trace.jsonl"
            completed = self.run_trace(trace, ["-c", "raise SystemExit(7)"])
            self.assertEqual(completed.returncode, 7)
            documents = trace_documents(trace)
            self.assertEqual(
                [document["event"] for document in documents],
                ["launch-requested", "started", "exited"],
            )
            self.assertEqual(documents[-1]["exitCode"], 7)
            self.assertIsNone(documents[-1]["signal"])
            self.assertEqual(documents[-1]["teardownCause"], "target-exit")
            self.assertLess(documents[0]["monotonicNs"], documents[-1]["monotonicNs"])

    def test_child_signal_is_recorded(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            trace = Path(directory) / "trace.jsonl"
            completed = self.run_trace(
                trace,
                [
                    "-c",
                    "import os, signal; os.kill(os.getpid(), signal.SIGTERM)",
                ],
            )
            self.assertEqual(completed.returncode, 128 + signal.SIGTERM)
            document = trace_documents(trace)[-1]
            self.assertIsNone(document["exitCode"])
            self.assertEqual(document["signal"], signal.SIGTERM)
            self.assertEqual(document["teardownCause"], "target-signal-15")

    def test_exec_failure_is_recorded(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            trace = root / "trace.jsonl"
            completed = subprocess.run(
                [
                    sys.executable,
                    str(TRACER),
                    "--role",
                    "notification",
                    "--executable",
                    str(root / "missing"),
                    "--trace",
                    str(trace),
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(completed.returncode, 127)
            self.assertEqual(
                [document["event"] for document in trace_documents(trace)],
                ["launch-requested", "exec-failed"],
            )

    def test_session_exec_preserves_the_exec_status(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            trace = Path(directory) / "trace.jsonl"
            completed = self.run_trace(
                trace, ["-c", "raise SystemExit(6)"], exec_only=True
            )
            self.assertEqual(completed.returncode, 6)
            self.assertEqual(
                [document["event"] for document in trace_documents(trace)],
                ["exec-requested"],
            )

    def test_matrix_session_trace_is_exactly_opt_in(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            runtime = Path(directory)
            stage = SimpleNamespace(
                executables={
                    "session": Path("/opt/qindaqt/bin/qindaqt-session"),
                    "notification": Path(
                        "/opt/qindaqt/bin/qindaqt-notification-host"
                    ),
                    "shell": Path("/opt/qindaqt/bin/qindaqt-shell"),
                }
            )
            scenario = SimpleNamespace(profile_id="work", theme_id="light")
            environment = {"XDG_RUNTIME_DIR": str(runtime)}
            direct = _session_program(stage, environment, scenario)
            self.assertNotIn("desktop_session_lifecycle.py", direct.read_text())

            environment[LIFECYCLE_TRACE_ENVIRONMENT] = "1"
            traced = _session_program(stage, environment, scenario)
            contents = traced.read_text()
            self.assertIn("--role session", contents)
            self.assertIn("--notification-host", contents)
            self.assertIn("--shell", contents)
            for role in ("notification", "shell"):
                wrapper = runtime / f"qindaqt-{role}-trace"
                self.assertTrue(wrapper.is_file())
                self.assertIn(f"--role {role}", wrapper.read_text())

    def test_forced_qt_stderr_is_lifecycle_diagnostic_only(self) -> None:
        default_environment = {"PATH": "/opt/qindaqt/bin:/usr/bin"}
        with patch.dict(os.environ, {}, clear=True):
            _enable_lifecycle_diagnostics(default_environment)
        self.assertEqual(default_environment, {"PATH": "/opt/qindaqt/bin:/usr/bin"})

        diagnostic_environment = {"PATH": "/opt/qindaqt/bin:/usr/bin"}
        with patch.dict(
            os.environ, {LIFECYCLE_TRACE_ENVIRONMENT: "1"}, clear=True
        ):
            _enable_lifecycle_diagnostics(diagnostic_environment)
        self.assertEqual(
            diagnostic_environment,
            {
                "PATH": "/opt/qindaqt/bin:/usr/bin",
                LIFECYCLE_TRACE_ENVIRONMENT: "1",
                "QT_FORCE_STDERR_LOGGING": "1",
            },
        )


if __name__ == "__main__":
    unittest.main()
