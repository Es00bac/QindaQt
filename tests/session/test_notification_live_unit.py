# SPDX-License-Identifier: GPL-3.0-or-later
"""Focused parser and containment tests for notification-live qualification."""

from __future__ import annotations

import json
import os
import re
import shlex
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from notification_live_stage import safe_artifact
from notification_live_process import (
    _validate_private_process_group,
    run_private_process_group,
    validate_private_session_process,
)
from notification_live_outer import REPETITION_TIMEOUT_SECONDS
from test_notification_live_nested import (
    _extract_probe,
    _scenario_profile,
    _write_session_wrapper,
    _write_private_locker_policy,
)


class NotificationLiveDriverTests(unittest.TestCase):
    def test_live_registration_imports_directory_scoped_global_accel(self) -> None:
        registration = Path(__file__).with_name("NotificationLiveTests.cmake")
        source = registration.read_text(encoding="utf-8")
        import_position = source.find(
            "find_package(KF6GlobalAccel 6.0 REQUIRED CONFIG)"
        )
        target_gate_position = source.find("if(TARGET qindaqt-shell\n")
        self.assertGreaterEqual(import_position, 0)
        self.assertGreater(target_gate_position, import_position)

    def test_race_timeout_preserves_outer_teardown_margin(self) -> None:
        registration = Path(__file__).with_name("NotificationLiveTests.cmake")
        source = registration.read_text(encoding="utf-8")
        race_block = source.partition("if(ARG_RACE_TEN)")[2].partition("endif()")[0]
        repeat_match = re.search(r"set\(_repeat (\d+)\)", race_block)
        timeout_match = re.search(r"set\(_timeout (\d+)\)", race_block)
        self.assertIsNotNone(repeat_match)
        self.assertIsNotNone(timeout_match)
        repeat = int(repeat_match.group(1))
        timeout = int(timeout_match.group(1))
        # AGENT-GUARD: Preserve time for staging, JSON serialization, and the
        # bounded TERM/KILL cleanup after every inner run uses its full budget.
        self.assertGreaterEqual(
            timeout - (repeat * REPETITION_TIMEOUT_SECONDS),
            300,
        )

    def test_probe_marker_requires_exact_passing_phase(self) -> None:
        result = _extract_probe(
            'noise\nQINDAQT_NOTIFICATION_LIVE={"passed":true,"phase":"primary"}\n',
            "primary",
        )
        self.assertTrue(result["passed"])
        with self.assertRaises(RuntimeError):
            _extract_probe(
                'QINDAQT_NOTIFICATION_LIVE={"passed":true,"phase":"other"}\n',
                "primary",
            )
        with self.assertRaises(RuntimeError):
            _extract_probe("", "primary")

    def test_staged_artifact_is_confined_and_must_exist(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory).resolve()
            binary = prefix / "bin" / "qindaqt-shell"
            binary.parent.mkdir()
            binary.touch()
            self.assertEqual(
                safe_artifact(prefix, Path("bin/qindaqt-shell"), "shell"),
                binary,
            )
            with self.assertRaises(RuntimeError):
                safe_artifact(prefix, Path("../escape"), "escape")
            with self.assertRaises(RuntimeError):
                safe_artifact(prefix, binary, "absolute")
            with self.assertRaises(RuntimeError):
                safe_artifact(prefix, Path("bin/missing"), "missing")

    def test_scenario_requires_catalog_ids(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            scenario = Path(directory) / "scenario.json"
            scenario.write_text(
                json.dumps({"profile": "macos-inspired", "theme": "qinda-macos"}),
                encoding="utf-8",
            )
            self.assertEqual(
                _scenario_profile(scenario), ("macos-inspired", "qinda-macos")
            )
            scenario.write_text(json.dumps({"profile": "macos-inspired"}), encoding="utf-8")
            with self.assertRaises(RuntimeError):
                _scenario_profile(scenario)

    def test_locker_policy_is_confined_to_supplied_config_root(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = root / "config"
            config.mkdir()
            _write_private_locker_policy(root, config)
            self.assertEqual(
                (config / "kscreenlockerrc").read_text(encoding="utf-8"),
                "[Daemon]\nRequirePassword=false\n",
            )
            with self.assertRaises(RuntimeError):
                _write_private_locker_policy(config, root)

    def test_session_wrapper_posix_quotes_every_hostile_argument(self) -> None:
        with tempfile.TemporaryDirectory(prefix="qindaqt $() ' ") as directory:
            root = Path(directory)
            artifacts = {
                "session": root / "session $(touch forbidden)",
                "notification_host": root / "host `touch forbidden`",
                "shell": root / "shell $HOME ' quoted",
            }
            profile = "profile $(touch forbidden); '"
            theme = "theme `touch forbidden` $PATH"
            wrapper = _write_session_wrapper(root, artifacts, profile, theme)
            command = wrapper.read_text(encoding="utf-8").splitlines()[1]
            self.assertTrue(command.startswith("exec "))
            self.assertEqual(
                shlex.split(command.removeprefix("exec ")),
                [
                    str(artifacts["session"]),
                    "--notification-host",
                    str(artifacts["notification_host"]),
                    "--shell",
                    str(artifacts["shell"]),
                    "--profile",
                    profile,
                    "--theme",
                    theme,
                ],
            )

    def test_process_group_guard_rejects_self_and_non_leader_targets(self) -> None:
        with self.assertRaises(RuntimeError):
            _validate_private_process_group(12345, 54321)
        with self.assertRaises(RuntimeError):
            _validate_private_process_group(os.getpid(), os.getpgrp())

    def test_signal_target_guard_requires_the_private_session(self) -> None:
        self.assertEqual(validate_private_session_process(os.getpid()), os.getpid())
        with self.assertRaisesRegex(RuntimeError, "unsafe private process PID"):
            validate_private_session_process(1)

        outside_session = subprocess.Popen(
            [sys.executable, "-c", "import time; time.sleep(30)"],
            start_new_session=True,
        )
        try:
            with self.assertRaisesRegex(RuntimeError, "outside private session"):
                validate_private_session_process(outside_session.pid)
        finally:
            outside_session.terminate()
            try:
                outside_session.wait(timeout=5)
            except subprocess.TimeoutExpired:
                outside_session.kill()
                outside_session.wait(timeout=5)

    def test_private_process_group_reports_success_and_cleans_timeout(self) -> None:
        completed = run_private_process_group(
            [sys.executable, "-c", "print('private-ok')"], os.environ, 2
        )
        self.assertEqual(completed.returncode, 0)
        self.assertEqual(completed.stdout.strip(), "private-ok")
        with self.assertRaisesRegex(RuntimeError, "timed out"):
            run_private_process_group(
                [sys.executable, "-c", "import time; time.sleep(30)"],
                os.environ,
                0.05,
            )


if __name__ == "__main__":
    unittest.main()
