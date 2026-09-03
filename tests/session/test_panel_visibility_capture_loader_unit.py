# SPDX-License-Identifier: GPL-3.0-or-later
"""Hostile controls for the panel capture loader boundary."""

from __future__ import annotations

import argparse
import unittest
from pathlib import Path

import panel_visibility_capture as capture
from desktop_session_sandbox import SandboxContractError


class PanelVisibilityCaptureLoaderTests(unittest.TestCase):
    def setUp(self) -> None:
        self.arguments = argparse.Namespace(
            visibility_probe=Path("/opt/qindaqt-tools/panel-probe"),
            weston_screenshooter=Path("/private/weston/bin/weston-screenshooter"),
        )

    def test_capture_child_receives_parent_weston_loader_path(self) -> None:
        parent = {
            "LD_LIBRARY_PATH": "/private/weston/lib:/private/weston/lib/libweston-15"
        }
        command = capture.visibility_probe_command(self.arguments, parent)
        self.assertEqual(command[:2], [str(self.arguments.visibility_probe),
                                       str(self.arguments.weston_screenshooter)])
        self.assertEqual(command[2], parent["LD_LIBRARY_PATH"])

    def test_missing_or_relative_parent_loader_path_is_rejected(self) -> None:
        for parent in ({}, {"LD_LIBRARY_PATH": "relative/lib"},
                       {"LD_LIBRARY_PATH": "/private/lib:"}):
            with self.subTest(parent=parent):
                with self.assertRaisesRegex(
                    SandboxContractError, "exact absolute search path"
                ):
                    capture.visibility_probe_command(self.arguments, parent)


if __name__ == "__main__":
    unittest.main()
