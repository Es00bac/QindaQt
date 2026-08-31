# SPDX-License-Identifier: GPL-3.0-or-later
"""Hostile PNG evidence tests for the private parent framebuffer."""

from __future__ import annotations

import binascii
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

from desktop_session_capture import CaptureContractError, _decode_png, validate_capture
from desktop_session_kwin_capture import _write_rgba_png


def _chunk(kind: bytes, payload: bytes) -> bytes:
    return (
        struct.pack(">I", len(payload)) + kind + payload
        + struct.pack(">I", binascii.crc32(kind + payload) & 0xffffffff)
    )


def write_png(path: Path, width: int, height: int, *, varied: bool) -> None:
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            value = (x * 17 + y * 31) & 0xff if varied else 0
            rows.extend((value, (value * 3) & 0xff, (value * 7) & 0xff))
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n" + _chunk(b"IHDR", header)
        + _chunk(b"IDAT", zlib.compress(bytes(rows))) + _chunk(b"IEND", b"")
    )


_SPARSE_REGION = {"x": 32, "y": 32, "width": 128, "height": 128}


def write_sparse_region_png(path: Path, *, inject_colors: bool) -> None:
    """Write a varied frame whose bounded region defeats the former 64x64 grid."""

    rows = bytearray()
    for y in range(192):
        rows.append(0)
        for x in range(192):
            inside = 32 <= x < 160 and 32 <= y < 160
            if inside:
                value = x - 32 if inject_colors and y == 33 and 33 <= x <= 48 else 0
            else:
                value = (x * 17 + y * 31) & 0xff
            rows.extend((value, (value * 3) & 0xff, (value * 7) & 0xff))
    header = struct.pack(">IIBBBBB", 192, 192, 8, 2, 0, 0, 0)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n" + _chunk(b"IHDR", header)
        + _chunk(b"IDAT", zlib.compress(bytes(rows))) + _chunk(b"IEND", b"")
    )


def legacy_region_grid_color_count(path: Path) -> int:
    width, _height, pixels, channels = _decode_png(path)
    colors: set[bytes] = set()
    for row in range(64):
        y = _SPARSE_REGION["y"] + row * _SPARSE_REGION["height"] // 64
        for column in range(64):
            x = _SPARSE_REGION["x"] + column * _SPARSE_REGION["width"] // 64
            start = (y * width + x) * channels
            colors.add(pixels[start:start + channels])
    return len(colors)


class CaptureTests(unittest.TestCase):
    def test_native_rgb32_capture_is_encoded_as_rgba(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.png"
            native = (
                bytes((3, 2, 1, 255))
                if sys.byteorder == "little"
                else bytes((255, 1, 2, 3))
            )
            _write_rgba_png(path, 1, 1, native)
            self.assertEqual(_decode_png(path), (1, 1, bytes((1, 2, 3, 255)), 4))

    def test_exact_nonuniform_png_passes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.png"
            write_png(path, 32, 24, varied=True)
            evidence = validate_capture(
                path, expected_width=32, expected_height=24, minimum_colors=16,
                content_region={"x": 8, "y": 4, "width": 16, "height": 12},
            )
            self.assertEqual((evidence["width"], evidence["height"]), (32, 24))
            self.assertEqual(len(evidence["sha256"]), 64)
            self.assertGreaterEqual(
                evidence["contentRegion"]["sampledDistinctColors"], 16
            )

    def test_sparse_grid_cannot_hide_nonuniform_content_region(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.png"
            write_sparse_region_png(path, inject_colors=True)
            self.assertEqual(legacy_region_grid_color_count(path), 1)
            evidence = validate_capture(
                path, expected_width=192, expected_height=192, minimum_colors=16,
                content_region=_SPARSE_REGION,
            )
            self.assertEqual(
                evidence["contentRegion"]["sampledDistinctColors"], 16
            )

    def test_uniform_content_region_fails_inside_a_varied_desktop(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.png"
            write_sparse_region_png(path, inject_colors=False)
            with self.assertRaisesRegex(CaptureContractError, "content region"):
                validate_capture(
                    path, expected_width=192, expected_height=192, minimum_colors=16,
                    content_region=_SPARSE_REGION,
                )

    def test_uniform_frame_and_wrong_dimensions_fail(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.png"
            write_png(path, 32, 24, varied=False)
            with self.assertRaisesRegex(CaptureContractError, "uniform"):
                validate_capture(
                    path, expected_width=32, expected_height=24, minimum_colors=2
                )
            write_png(path, 31, 24, varied=True)
            with self.assertRaisesRegex(CaptureContractError, "dimensions"):
                validate_capture(path, expected_width=32, expected_height=24)

    def test_corrupt_crc_and_symlink_fail(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "capture.png"
            write_png(path, 32, 24, varied=True)
            data = bytearray(path.read_bytes())
            data[29] ^= 1
            path.write_bytes(data)
            with self.assertRaisesRegex(CaptureContractError, "checksum"):
                validate_capture(path, expected_width=32, expected_height=24)
            target = root / "target.png"
            write_png(target, 32, 24, varied=True)
            path.unlink()
            path.symlink_to(target)
            with self.assertRaisesRegex(CaptureContractError, "non-symlink"):
                validate_capture(path, expected_width=32, expected_height=24)


if __name__ == "__main__":
    unittest.main()
