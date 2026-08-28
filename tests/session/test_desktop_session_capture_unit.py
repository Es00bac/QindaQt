# SPDX-License-Identifier: GPL-3.0-or-later
"""Hostile PNG evidence tests for the private parent framebuffer."""

from __future__ import annotations

import binascii
import struct
import tempfile
import unittest
import zlib
from pathlib import Path

from desktop_session_capture import CaptureContractError, validate_capture


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


class CaptureTests(unittest.TestCase):
    def test_exact_nonuniform_png_passes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.png"
            write_png(path, 32, 24, varied=True)
            evidence = validate_capture(
                path, expected_width=32, expected_height=24, minimum_colors=16
            )
            self.assertEqual((evidence["width"], evidence["height"]), (32, 24))
            self.assertEqual(len(evidence["sha256"]), 64)

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
