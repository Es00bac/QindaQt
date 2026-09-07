# SPDX-License-Identifier: GPL-3.0-or-later
"""Host-safe contracts for the lane-gated daily-controls row."""

from __future__ import annotations

import os
import unittest
from unittest.mock import patch

from test_daily_controls_live import LANE, _outer, _parser


class DailyControlsLiveContractTests(unittest.TestCase):
    def test_unassigned_lane_skips_before_stage_or_private_processes(self) -> None:
        arguments = _parser().parse_args([
            "--build-root", "/missing", "--source-root", "/missing", "--cmake", "/missing",
            "--bwrap", "/missing", "--python", "/missing", "--dbus-daemon", "/missing",
            "--kwin-wayland", "/missing", "--pipewire", "/missing", "--wireplumber", "/missing",
            "--pw-cli", "/missing", "--spectacle", "/missing", "--pipewire-config", "/missing",
            "--probe", "/missing", "--bin-directory", "bin", "--plugin-relative", "plugin",
            "--decoration-relative", "decoration", "--settings-service-directory", "services",
            "--audio-service-directory", "services",
        ])
        with patch.dict(os.environ, {"QINDAQT_PRIVATE_RUNTIME_LANE": "other"}, clear=False):
            self.assertEqual(_outer(arguments), 77)

    def test_lane_name_is_specific(self) -> None:
        self.assertEqual(LANE, "daily-controls")


if __name__ == "__main__":
    unittest.main()
