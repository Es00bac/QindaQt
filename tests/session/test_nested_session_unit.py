# SPDX-License-Identifier: GPL-3.0-or-later
"""Focused tests for virtual-display scenario representability."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from test_nested_session import (
    ScenarioCoverageError,
    virtual_spec_from_document,
    write_virtual_output_config,
)


def output(
    name: str,
    *,
    width: int = 1920,
    height: int = 1080,
    scale: float = 1.0,
    enabled: bool = True,
    transform: str = "normal",
) -> dict[str, object]:
    return {
        "name": name,
        "enabled": enabled,
        "mode": {"width": width, "height": height},
        "scale": scale,
        "transform": transform,
    }


class VirtualSpecTests(unittest.TestCase):
    def test_common_fractional_mode_becomes_exact_logical_geometry(self) -> None:
        document = {
            "id": "dual-fractional",
            "outputs": [
                output("DP-1", scale=1.25),
                output("DP-2", scale=1.25),
                output("disabled", width=2560, height=1440, enabled=False),
            ],
        }

        spec = virtual_spec_from_document(document)

        self.assertEqual(spec.output_count, 2)
        self.assertEqual((spec.pixel_width, spec.pixel_height), (1920, 1080))
        self.assertEqual((spec.logical_width, spec.logical_height), (1536, 864))
        self.assertEqual(spec.scale, 1.25)
        self.assertIn("positions", spec.coverage["notApplied"])

    def test_heterogeneous_enabled_modes_are_rejected_explicitly(self) -> None:
        document = {
            "id": "mixed",
            "outputs": [output("DP-1"), output("DP-2", width=2560, height=1440)],
        }

        with self.assertRaisesRegex(
            ScenarioCoverageError,
            "heterogeneous enabled output modes/scales",
        ):
            virtual_spec_from_document(document)

    def test_non_integral_logical_extent_is_rejected(self) -> None:
        document = {
            "id": "non-integral",
            "outputs": [output("DP-1", width=2560, height=1440, scale=1.5)],
        }

        with self.assertRaisesRegex(ScenarioCoverageError, "non-integral logical size"):
            virtual_spec_from_document(document)

    def test_transform_is_not_misreported_as_applied(self) -> None:
        document = {
            "id": "portrait",
            "outputs": [output("DP-1", width=1920, height=1200, transform="rotate-90")],
        }

        with self.assertRaisesRegex(ScenarioCoverageError, "cannot apply output transforms"):
            virtual_spec_from_document(document)

    def test_isolated_kwin_config_pins_common_scale(self) -> None:
        spec = virtual_spec_from_document(
            {
                "id": "fractional",
                "outputs": [output("DP-1", scale=1.25)],
            }
        )
        with tempfile.TemporaryDirectory() as directory:
            write_virtual_output_config(Path(directory), spec)
            document = json.loads(
                (Path(directory) / "kwinoutputconfig.json").read_text(encoding="utf-8")
            )

        output_data = document[0]["data"][0]
        self.assertEqual(output_data["mode"]["width"], 1920)
        self.assertEqual(output_data["mode"]["height"], 1080)
        self.assertEqual(output_data["scale"], 1.25)


if __name__ == "__main__":
    unittest.main()
