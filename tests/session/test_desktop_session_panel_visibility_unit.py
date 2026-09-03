# SPDX-License-Identifier: GPL-3.0-or-later
"""Hostile controls for installed panel visibility evidence."""

from __future__ import annotations

import binascii
import copy
import struct
import tempfile
import unittest
import zlib
from pathlib import Path
from unittest.mock import patch

import test_panel_visibility_nested as visibility


WIDTH = 160
HEIGHT = 120


def _chunk(kind: bytes, payload: bytes) -> bytes:
    return (struct.pack(">I", len(payload)) + kind + payload
            + struct.pack(">I", binascii.crc32(kind + payload) & 0xFFFFFFFF))


def _write_phase(path: Path, panel: str, visible: bool) -> None:
    palette = (
        (15, 20, 30), (220, 30, 40), (20, 190, 60), (40, 80, 220),
        (230, 190, 20), (180, 40, 210), (20, 190, 200), (235, 235, 235),
    )
    rows = bytearray()
    for y in range(HEIGHT):
        rows.append(0)
        for x in range(WIDTH):
            color = palette[((x // 11) + (y // 7)) % len(palette)]
            if visible and panel == "left" and x < 40:
                color = (180, (x * 5 + y) & 0xFF, 40)
            if visible and panel == "bottom" and y >= HEIGHT - 48:
                color = (40, (x + y * 3) & 0xFF, 190)
            rows.extend(color)
    header = struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 2, 0, 0, 0)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n" + _chunk(b"IHDR", header)
        + _chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + _chunk(b"IEND", b"")
    )


def _surface(edge: str) -> dict[str, object]:
    geometries = {
        "top": {"x": 0, "y": 0, "width": WIDTH, "height": 30},
        "left": {"x": 0, "y": 0, "width": 40, "height": HEIGHT},
        "bottom": {"x": 40, "y": HEIGHT - 48, "width": 80, "height": 48},
    }
    return {"mapped": True, "committed": True, "geometry": geometries[edge]}


def _interaction() -> dict[str, object]:
    phases = []
    for name in visibility.PHASES:
        surfaces = [_surface("top")]
        if name in {"window-moved-away", "window-closed-restored"}:
            surfaces.append(_surface("left"))
        if name in {"edge-revealed", "shortcut-revealed", "popup-held"}:
            surfaces.append(_surface("bottom"))
        phases.append({"phase": name, "surfaces": surfaces})
    return {
        "schemaVersion": 1, "outputWidth": WIDTH, "outputHeight": HEIGHT,
        "move": {
            "before": {"x": 0, "y": 30, "width": 120, "height": 80},
            "after": {"x": 60, "y": 35, "width": 50, "height": 50},
            "surfacesBefore": [_surface("top")],
        },
        "close": {
            "before": {"x": 0, "y": 35, "width": 50, "height": 50},
            "windowAbsentAfter": True,
        },
        "phases": phases,
    }


def _write_captures(root: Path, *, identical: bool = False) -> None:
    for name in visibility.PHASES:
        panel = "left" if name.startswith("window-") else "bottom"
        shown = name not in {
            "window-overlap-hidden", "window-close-hidden", "popup-closed"
        }
        _write_phase(root / f"panel-{name}.png", panel,
                     False if identical else shown)


class PanelVisibilityEvidenceTests(unittest.TestCase):
    def _validate(self, root: Path, interaction: dict[str, object]) -> object:
        real_path = visibility.Path
        with patch.object(
            visibility, "Path",
            side_effect=lambda value: root if value == "/var/lib/qindaqt-evidence"
            else real_path(value),
        ):
            return visibility._validate_captures(WIDTH, HEIGHT, interaction)

    def test_joins_each_phase_to_authority_panel_pixels(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _write_captures(root)
            captures = self._validate(root, _interaction())
            self.assertEqual([item["phase"] for item in captures], list(visibility.PHASES))
            self.assertTrue(all("panelRegion" in item for item in captures))

    def test_identical_unrelated_phases_are_rejected(self) -> None:
        # AGENT-NOTE: P1-3's exact hostile image class formerly passed 6/6.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _write_captures(root, identical=True)
            with self.assertRaisesRegex(RuntimeError, "panel pixels did not change"):
                self._validate(root, _interaction())

    def test_move_and_close_authority_are_causal(self) -> None:
        # AGENT-NOTE: P1-4 requires changed compositor geometry, a covering
        # close precondition, and observed post-close window absence.
        document = _interaction()
        visibility._validate_interaction(document, WIDTH, HEIGHT)
        unchanged = copy.deepcopy(document)
        unchanged["move"]["after"] = unchanged["move"]["before"]
        with self.assertRaisesRegex(RuntimeError, "did not move"):
            visibility._validate_interaction(unchanged, WIDTH, HEIGHT)
        visible_before_drag = copy.deepcopy(document)
        visible_before_drag["move"]["surfacesBefore"].append(_surface("left"))
        with self.assertRaisesRegex(RuntimeError, "hidden immediately before"):
            visibility._validate_interaction(visible_before_drag, WIDTH, HEIGHT)
        still_present = copy.deepcopy(document)
        still_present["close"]["windowAbsentAfter"] = False
        with self.assertRaisesRegex(RuntimeError, "close authority"):
            visibility._validate_interaction(still_present, WIDTH, HEIGHT)
        noncovering_close = copy.deepcopy(document)
        noncovering_close["close"]["before"]["x"] = 60
        with self.assertRaisesRegex(RuntimeError, "did not cover"):
            visibility._validate_interaction(noncovering_close, WIDTH, HEIGHT)


if __name__ == "__main__":
    unittest.main()
