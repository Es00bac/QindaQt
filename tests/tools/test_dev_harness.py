# SPDX-License-Identifier: GPL-3.0-or-later
"""Contract tests for development-session plans that launch external tools."""

from __future__ import annotations

import unittest
from pathlib import Path

from qindaqt_dev.backends import BACKEND_NAMES, build_plan
from qindaqt_dev.scenarios import ScenarioRepository


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]


class DevelopmentHarnessTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        repository = ScenarioRepository(REPOSITORY_ROOT / "tests" / "scenarios")
        cls.scenarios = repository.load_all()
        cls.baseline = repository.load("single-1080p")

    def test_required_scenario_matrix_is_valid(self) -> None:
        self.assertGreaterEqual(len(self.scenarios), 14)
        self.assertEqual(self.baseline.canvas_size, (1920, 1080))

    def test_every_backend_builds_a_shell_free_argument_vector(self) -> None:
        for backend in BACKEND_NAMES:
            with self.subTest(backend=backend):
                plan = build_plan(self.baseline, backend, program=f"test-{backend}")
                self.assertEqual(plan.argv[0], f"test-{backend}")
                self.assertTrue(all(plan.argv))

    def test_preview_plan_matches_the_current_preview_cli(self) -> None:
        plan = build_plan(self.baseline, "preview", program="qindaqt-shell-preview")
        self.assertEqual(
            plan.argv,
            (
                "qindaqt-shell-preview",
                "--profile",
                self.baseline.profile,
                "--theme",
                self.baseline.theme,
                "--width",
                "1920",
                "--height",
                "1080",
            ),
        )

    def test_xephyr_uses_the_supported_single_screen_argument(self) -> None:
        plan = build_plan(self.baseline, "xephyr", program="Xephyr")
        screen_index = plan.argv.index("-screen")
        self.assertEqual(plan.argv[screen_index + 1], "1920x1080x24")
        self.assertNotEqual(plan.argv[screen_index + 1], "0")


if __name__ == "__main__":
    unittest.main()
