# SPDX-License-Identifier: GPL-3.0-or-later
"""Encode the authenticated private KWin virtual framebuffer as PNG."""

from __future__ import annotations

import argparse
import binascii
import hashlib
import json
import os
import struct
import sys
import zlib
from pathlib import Path


_EVIDENCE_PREFIX = "QINDAQT_PRIVATE_KWIN_CAPTURE="
_PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--parent-pid", type=int, required=True)
    parser.add_argument("--expected-width", type=int, required=True)
    parser.add_argument("--expected-height", type=int, required=True)
    parser.add_argument("--expected-logical-width", type=int, required=True)
    parser.add_argument("--expected-logical-height", type=int, required=True)
    parser.add_argument("--expected-scale", type=float, required=True)
    return parser.parse_args()


def _parent_arguments(arguments: argparse.Namespace) -> list[str]:
    process_root = Path(f"/proc/{arguments.parent_pid}")
    executable = process_root / "exe"
    if arguments.parent_pid <= 1:
        raise RuntimeError("capture PID is not the exact private KWin executable")
    resolved = executable.resolve(strict=True)
    if resolved.name != "kwin_wayland":
        raise RuntimeError("capture PID is not the exact private KWin executable")
    values = [
        value.decode("utf-8", "strict")
        for value in (process_root / "cmdline").read_bytes().split(b"\0")
        if value
    ]
    expected_tail = [
        "--virtual",
        "--width", str(arguments.expected_logical_width),
        "--height", str(arguments.expected_logical_height),
        "--scale", str(arguments.expected_scale),
        "--output-count", "1",
        "--socket", "qindaqt-parent-wayland",
        "--no-lockscreen",
        "--no-global-shortcuts",
    ]
    if len(values) < 2 or Path(values[0]).name != "kwin_wayland" or values[1:] != expected_tail:
        raise RuntimeError("private KWin arguments disagree with the capture row")
    return values[1:]


def _framebuffer(arguments: argparse.Namespace) -> tuple[bytes, str]:
    expected_bytes = arguments.expected_width * arguments.expected_height * 4
    candidates: dict[str, tuple[bytes, str]] = {}
    directory = Path(f"/proc/{arguments.parent_pid}/fd")
    for entry in sorted(directory.iterdir(), key=lambda path: int(path.name)):
        try:
            target = os.readlink(entry)
            if target not in {"/memfd:shm (deleted)", "memfd:shm (deleted)"}:
                continue
            if entry.stat().st_size != expected_bytes:
                continue
            pixels = entry.read_bytes()
        except OSError:
            continue
        if len(pixels) != expected_bytes:
            continue
        digest = hashlib.sha256(pixels).hexdigest()
        candidates.setdefault(digest, (pixels, target))
    if len(candidates) != 1:
        raise RuntimeError(
            "private KWin framebuffer is absent or has ambiguous distinct buffers"
        )
    return next(iter(candidates.values()))


def _png_chunk(kind: bytes, payload: bytes) -> bytes:
    chunk = kind + payload
    return (
        struct.pack(">I", len(payload))
        + chunk
        + struct.pack(">I", binascii.crc32(chunk) & 0xFFFFFFFF)
    )


def _write_rgba_png(path: Path, width: int, height: int, pixels: bytes) -> None:
    """Encode a raw 32-bit BGRA framebuffer as a lossless RGBA PNG."""
    row_bytes = width * 4
    if len(pixels) != row_bytes * height:
        raise RuntimeError("pixel buffer size does not match the declared dimensions")
    rgba = bytearray(len(pixels))
    if sys.byteorder == "little":
        # QImage::Format_RGB32 is native-endian 0xffRRGGBB: little-endian
        # framebuffer bytes are B, G, R, X.
        rgba[0::4] = pixels[2::4]
        rgba[1::4] = pixels[1::4]
        rgba[2::4] = pixels[0::4]
    else:
        # Big-endian framebuffer bytes are X, R, G, B.
        rgba[0::4] = pixels[1::4]
        rgba[1::4] = pixels[2::4]
        rgba[2::4] = pixels[3::4]
    rgba[3::4] = b"\xff" * (width * height)
    raw = bytearray()
    for row in range(height):
        raw.append(0)  # filter byte: None
        raw.extend(rgba[row * row_bytes : (row + 1) * row_bytes])
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    data = (
        _PNG_SIGNATURE
        + _png_chunk(b"IHDR", ihdr)
        + _png_chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + _png_chunk(b"IEND", b"")
    )
    path.write_bytes(data)


def _capture(arguments: argparse.Namespace) -> dict[str, object]:
    output = arguments.output
    if not output.is_absolute() or output.exists() or output.is_symlink():
        raise RuntimeError("capture output must be one fresh absolute path")
    if (
        arguments.expected_width <= 0
        or arguments.expected_height <= 0
        or arguments.expected_logical_width <= 0
        or arguments.expected_logical_height <= 0
        or arguments.expected_scale <= 1.0
    ):
        raise RuntimeError("fractional capture dimensions or scale are invalid")
    if os.environ.get("WAYLAND_DISPLAY") != "qindaqt-parent-wayland":
        raise RuntimeError("capture is not bound to the private parent Wayland socket")
    parent_arguments = _parent_arguments(arguments)
    pixels, fd_target = _framebuffer(arguments)
    stride = arguments.expected_width * 4
    _write_rgba_png(output, arguments.expected_width, arguments.expected_height, pixels)
    return {
        "type": "raw",
        "width": arguments.expected_width,
        "height": arguments.expected_height,
        "stride": stride,
        "format": "RGB32",
        "scale": arguments.expected_scale,
        "source": "authenticated-private-kwin-shm",
        "fdTarget": fd_target,
        "parentArguments": parent_arguments,
    }


def main() -> int:
    arguments = parse_arguments()
    try:
        evidence = _capture(arguments)
    except (OSError, RuntimeError, TypeError, ValueError) as error:
        print(f"private KWin capture failed: {error}", file=sys.stderr)
        return 1
    print(_EVIDENCE_PREFIX + json.dumps(evidence, sort_keys=True, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
