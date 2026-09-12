#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Unit tests for the pure helpers behind the shade-visibility nested rows."""

from __future__ import annotations

import struct
import sys
import unittest
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))

import shade_fixtures  # noqa: E402
from shade_framebuffer import (CaptureError, Frame, area_mismatch, encode_png,  # noqa: E402
                               parse_colour, pixel_matches, select_fresh_buffer)


def solid_pixels(width: int, height: int, colour: tuple[int, int, int]) -> bytearray:
    red, green, blue = colour
    pixel = bytes((blue, green, red, 0xFF)) if sys.byteorder == "little" else bytes(
        (0xFF, red, green, blue))
    return bytearray(pixel * (width * height))


def paint(pixels: bytearray, width: int, x: int, y: int, colour: tuple[int, int, int]) -> None:
    red, green, blue = colour
    offset = (y * width + x) * 4
    pixels[offset : offset + 4] = bytes((blue, green, red, 0xFF)) if sys.byteorder == "little" \
        else bytes((0xFF, red, green, blue))


class SelectFreshBufferTests(unittest.TestCase):
    def test_single_changed_slot_is_the_front_buffer(self) -> None:
        pixels, method = select_fresh_buffer({1: b"old", 2: b"same"}, {1: b"new", 2: b"same"})
        self.assertEqual(pixels, b"new")
        self.assertEqual(method, "freshly-rendered-slot")

    def test_static_single_distinct_buffer_is_accepted(self) -> None:
        pixels, method = select_fresh_buffer({1: b"x", 2: b"x"}, {1: b"x", 2: b"x"})
        self.assertEqual((pixels, method), (b"x", "single-distinct-buffer"))

    def test_a_slot_that_appears_during_the_repaint_is_the_fresh_one(self) -> None:
        pixels, method = select_fresh_buffer({1: b"x"}, {1: b"x", 2: b"y"})
        self.assertEqual((pixels, method), (b"y", "freshly-rendered-slot"))

    def test_ambiguous_swapchains_are_rejected(self) -> None:
        with self.assertRaises(CaptureError):
            select_fresh_buffer({1: b"a", 2: b"b"}, {1: b"a", 2: b"b"})
        with self.assertRaises(CaptureError):
            select_fresh_buffer({1: b"a", 2: b"b"}, {1: b"c", 2: b"d"})
        with self.assertRaises(CaptureError):
            select_fresh_buffer({}, {})


class PixelEvidenceTests(unittest.TestCase):
    def test_area_mismatch_counts_only_non_expected_pixels_outside_the_exclusion(self) -> None:
        width, height = 20, 10
        pixels = solid_pixels(width, height, (0x1F, 0x7A, 0x4D))
        paint(pixels, width, 3, 3, (0xFF, 0x00, 0x00))   # ghost pixel
        paint(pixels, width, 15, 1, (0x00, 0x00, 0xFF))  # inside the strip exclusion
        frame = Frame(bytes(pixels), width, height, 1.0, "test")
        result = area_mismatch(frame, (0, 0, 20, 10), (12, 0, 6, 2), "#1f7a4d", step=1)
        self.assertEqual(result["mismatched"], 1)
        self.assertEqual(result["bounds"], [3, 3, 3, 3])
        self.assertEqual(result["samples"][0]["rgb"], [0xFF, 0x00, 0x00])
        self.assertGreater(result["sampled"], 0)

    def test_area_mismatch_maps_logical_coordinates_through_the_scale(self) -> None:
        width, height = 20, 20
        pixels = solid_pixels(width, height, (0, 0, 0))
        paint(pixels, width, 16, 16, (0xFF, 0xFF, 0xFF))
        frame = Frame(bytes(pixels), width, height, 2.0, "test")
        self.assertEqual(area_mismatch(frame, (0, 0, 5, 5), None, "#000000", step=1)["mismatched"], 0)
        self.assertEqual(area_mismatch(frame, (5, 5, 5, 5), None, "#000000", step=1)["mismatched"], 1)
        self.assertTrue(pixel_matches(frame, (8.0, 8.0), "#ffffff"))
        self.assertFalse(pixel_matches(frame, (1.0, 1.0), "#ffffff"))

    def test_png_round_trips_the_framebuffer_rows(self) -> None:
        width, height = 3, 2
        pixels = solid_pixels(width, height, (10, 20, 30))
        paint(pixels, width, 2, 1, (200, 100, 50))
        data = encode_png(Frame(bytes(pixels), width, height, 1.0, "test"))
        self.assertTrue(data.startswith(b"\x89PNG\r\n\x1a\n"))
        length = struct.unpack(">I", data[33:37])[0]
        self.assertEqual(data[37:41], b"IDAT")
        raw = zlib.decompress(data[41 : 41 + length])
        self.assertEqual(len(raw), height * (1 + width * 4))
        self.assertEqual(raw[1:5], bytes((10, 20, 30, 255)))
        row = 1 + width * 4
        self.assertEqual(raw[row + 1 + 8 : row + 1 + 12], bytes((200, 100, 50, 255)))
        with self.assertRaises(CaptureError):
            encode_png(Frame(b"short", width, height, 1.0, "test"))

    def test_colour_parsing_rejects_malformed_values(self) -> None:
        self.assertEqual(parse_colour("#1f7a4d"), (0x1F, 0x7A, 0x4D))
        for value in ("1f7a4d", "#1f7a4", "#1f7a4dff"):
            with self.assertRaises(ValueError):
                parse_colour(value)


class RunnerEvaluationTests(unittest.TestCase):
    def setUp(self) -> None:
        import run_shade_visibility

        self.evaluate = run_shade_visibility.evaluate
        self.plugin_root = Path("/build/plugins")
        self.library = str((self.plugin_root / "kwin/plugins/qindaqt_compositor.so").resolve())

    def evidence(self, **overrides):
        base = {"result": "completed",
                "verdicts": {f"v{index}": True for index in range(16)},
                "mappedQindaqtLibraries": [self.library]}
        base.update(overrides)
        return base

    def test_complete_passing_evidence_passes(self) -> None:
        self.assertEqual(self.evaluate(self.evidence(), "cycles", self.plugin_root), [])

    def test_any_false_or_missing_verdict_fails(self) -> None:
        verdicts = {f"v{index}": True for index in range(16)}
        verdicts["ghostFreeAfterRollUp0"] = False
        failures = self.evaluate(self.evidence(verdicts=verdicts), "cycles", self.plugin_root)
        self.assertEqual(failures, ["verdict failed: ghostFreeAfterRollUp0"])
        failures = self.evaluate(self.evidence(verdicts={"only": True}), "cycles", self.plugin_root)
        self.assertTrue(any("verdicts recorded" in failure for failure in failures))

    def test_incomplete_flow_or_wrong_plugin_fails(self) -> None:
        self.assertTrue(self.evaluate(self.evidence(result="failed: timeout"), "cycles",
                                      self.plugin_root))
        failures = self.evaluate(self.evidence(mappedQindaqtLibraries=[
            "/usr/lib64/qt6/plugins/kwin/plugins/qindaqt_compositor.so"]), "cycles", self.plugin_root)
        self.assertTrue(any("did not map" in failure for failure in failures))


def window(x: float, y: float, width: float, height: float, stack: int, container: str = "") -> dict:
    return {"geometry": {"x": x, "y": y, "width": width, "height": height}, "stackIndex": stack,
            "hidden": False, "minimized": False, "containerId": container}


class GestureGeometryTests(unittest.TestCase):
    def test_a_window_above_the_target_edge_moves_the_drop_to_another_edge(self) -> None:
        from shade_control import dock_drop_point
        inventory = {"source": window(0, 0, 100, 100, 3), "target": window(400, 100, 200, 200, 1),
                     "cover": window(380, 80, 120, 260, 2)}
        drop = dock_drop_point(inventory, "source", "target")
        self.assertIsNotNone(drop)
        self.assertGreater(drop[0], 500)

    def test_the_dragged_source_never_blocks_its_own_drop(self) -> None:
        from shade_control import dock_drop_point
        inventory = {"source": window(390, 90, 300, 300, 5), "target": window(400, 100, 200, 200, 1)}
        self.assertEqual(dock_drop_point(inventory, "source", "target"), (416.0, 200.0))

    def test_a_fully_covered_target_has_no_drop_point(self) -> None:
        from shade_control import dock_drop_point, uncovered_content_point
        inventory = {"source": window(0, 0, 50, 50, 1), "target": window(400, 100, 200, 200, 2),
                     "cover": window(300, 0, 500, 500, 3)}
        self.assertIsNone(dock_drop_point(inventory, "source", "target"))
        self.assertIsNone(uncovered_content_point(inventory, "target"))

    def test_a_grouped_window_covers_its_shared_row(self) -> None:
        from shade_control import title_point, uncovered
        inventory = {"below": window(100, 100, 400, 300, 1),
                     "grouped": window(80, 130, 440, 300, 2, container="c1")}
        self.assertFalse(uncovered(inventory, "below", (300.0, 112.0)))
        self.assertIsNone(title_point(inventory, "below"))
        self.assertFalse(uncovered(inventory, "below", (900.0, 112.0)))  # outside its frame

    def test_a_window_above_covers_its_input_margin_beyond_the_frame(self) -> None:
        from shade_control import INPUT_MARGIN, uncovered
        inventory = {"below": window(0, 0, 400, 400, 1), "above": window(200, 0, 200, 400, 2)}
        self.assertFalse(uncovered(inventory, "below", (200 - INPUT_MARGIN / 2, 200.0)))
        self.assertTrue(uncovered(inventory, "below", (200 - INPUT_MARGIN - 4, 200.0)))


class TiledSiblingTests(unittest.TestCase):
    def test_container_siblings_cover_only_their_exact_frames(self) -> None:
        from shade_control import uncovered, uncovered_content_point
        inventory = {"left": window(100, 100, 180, 400, 3, container="c1"),
                     "middle": window(280, 100, 180, 400, 1, container="c1"),
                     "right": window(460, 100, 180, 400, 2, container="c1")}
        self.assertTrue(uncovered(inventory, "middle", (285.0, 300.0)))
        self.assertIsNotNone(uncovered_content_point(inventory, "middle"))
        self.assertFalse(uncovered(inventory, "middle", (270.0, 300.0)))  # inside left's frame


class FixtureCatalogueTests(unittest.TestCase):
    def test_builtin_gtk_kinds_need_no_extra_executable(self) -> None:
        for kind in ("gtk-csd", "gtk-ssd", "gtk-borderless", "gtk-x11-csd"):
            self.assertIsNone(shade_fixtures.missing_executable(kind))

    def test_unknown_kind_is_rejected(self) -> None:
        with self.assertRaises(ValueError):
            shade_fixtures.missing_executable("not-a-kind")


if __name__ == "__main__":
    unittest.main()
