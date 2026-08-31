# SPDX-License-Identifier: GPL-3.0-or-later
"""Focused fail-closed CLI contract for the desktop interaction probe."""

from __future__ import annotations

import argparse
import subprocess
import unittest
from pathlib import Path


class ProbeCliTests(unittest.TestCase):
    probe: Path
    dbus_run_session: Path

    def run_probe(self, *probe_arguments: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [str(self.dbus_run_session), "--", str(self.probe), *probe_arguments],
            text=True,
            capture_output=True,
            check=False,
            timeout=5,
        )

    def test_primary_and_secondary_interaction_modes_are_recognized(self) -> None:
        for option in (
            "--open-notification-center",
            "--open-notification-center-secondary",
        ):
            with self.subTest(option=option):
                completed = self.run_probe(option)
                self.assertEqual(completed.returncode, 3)
                self.assertIn(
                    "required private services are unavailable", completed.stderr
                )
                self.assertNotIn("unsupported probe arguments", completed.stderr)

    def test_unknown_or_ambiguous_interaction_modes_fail_closed(self) -> None:
        for probe_arguments in (
            ("--open-notification-center-tertiary",),
            ("--open-notification-center-secondary", "extra"),
        ):
            with self.subTest(probe_arguments=probe_arguments):
                completed = self.run_probe(*probe_arguments)
                self.assertEqual(completed.returncode, 2)
                self.assertEqual(completed.stderr, "unsupported probe arguments\n")


def arguments() -> tuple[argparse.Namespace, list[str]]:
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True, type=Path)
    parser.add_argument("--dbus-run-session", required=True, type=Path)
    return parser.parse_known_args()


if __name__ == "__main__":
    parsed, remaining = arguments()
    ProbeCliTests.probe = parsed.probe.resolve(strict=True)
    ProbeCliTests.dbus_run_session = parsed.dbus_run_session.resolve(strict=True)
    # unittest owns its optional argv; no task argument may leak into it.
    unittest.main(argv=[__file__, *remaining])
