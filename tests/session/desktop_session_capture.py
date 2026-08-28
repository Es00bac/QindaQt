# SPDX-License-Identifier: GPL-3.0-or-later
"""Fail-closed capture and PNG evidence for the private parent framebuffer."""

from __future__ import annotations

import binascii
import hashlib
import os
import struct
import subprocess
import zlib
from pathlib import Path
from typing import Mapping


class CaptureContractError(ValueError):
    """A capture was absent, ambiguous, malformed, or visually empty."""


_PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
_MAX_PNG_BYTES = 32 * 1024 * 1024


def _paeth(left: int, above: int, upper_left: int) -> int:
    value = left + above - upper_left
    distances = (abs(value - left), abs(value - above), abs(value - upper_left))
    return (left, above, upper_left)[distances.index(min(distances))]


def _decode_png(path: Path) -> tuple[int, int, bytes, int]:
    if path.is_symlink() or not path.is_file():
        raise CaptureContractError("capture must be one regular non-symlink file")
    data = path.read_bytes()
    if len(data) > _MAX_PNG_BYTES or not data.startswith(_PNG_SIGNATURE):
        raise CaptureContractError("capture is not a bounded PNG")
    offset = len(_PNG_SIGNATURE)
    header: tuple[int, int, int] | None = None
    compressed = bytearray()
    ended = False
    while offset + 12 <= len(data):
        length = struct.unpack(">I", data[offset:offset + 4])[0]
        if length > _MAX_PNG_BYTES or offset + 12 + length > len(data):
            raise CaptureContractError("PNG chunk length escapes the capture")
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + length]
        checksum = struct.unpack(">I", data[offset + 8 + length:offset + 12 + length])[0]
        if binascii.crc32(kind + payload) & 0xffffffff != checksum:
            raise CaptureContractError("PNG chunk checksum is invalid")
        offset += 12 + length
        if kind == b"IHDR":
            if header is not None or length != 13:
                raise CaptureContractError("PNG has an invalid IHDR")
            width, height, depth, color, compression, filtering, interlace = struct.unpack(
                ">IIBBBBB", payload
            )
            if (
                width <= 0 or height <= 0 or width > 32768
                or height > 32768 or width * height > 16_777_216
                or depth != 8 or color not in (2, 6)
                or compression != 0 or filtering != 0 or interlace != 0
            ):
                raise CaptureContractError("PNG encoding is outside the capture contract")
            header = (width, height, 3 if color == 2 else 4)
        elif kind == b"IDAT":
            if header is None:
                raise CaptureContractError("PNG IDAT precedes IHDR")
            compressed.extend(payload)
        elif kind == b"IEND":
            if length != 0:
                raise CaptureContractError("PNG IEND is malformed")
            ended = True
            break
    if header is None or not compressed or not ended or offset != len(data):
        raise CaptureContractError("PNG is incomplete or has trailing bytes")
    width, height, channels = header
    row_bytes = width * channels
    try:
        decompressor = zlib.decompressobj()
        expected_size = (row_bytes + 1) * height
        raw = decompressor.decompress(bytes(compressed), expected_size + 1)
        if len(raw) > expected_size or decompressor.unconsumed_tail:
            raise CaptureContractError("PNG decompressed length exceeds its header")
        raw += decompressor.flush()
    except zlib.error as error:
        raise CaptureContractError("PNG image data cannot be decompressed") from error
    if len(raw) != expected_size or not decompressor.eof or decompressor.unused_data:
        raise CaptureContractError("PNG decompressed length is not exact")
    prior = bytearray(row_bytes)
    pixels = bytearray()
    cursor = 0
    for _ in range(height):
        filter_type = raw[cursor]
        encoded = raw[cursor + 1:cursor + 1 + row_bytes]
        cursor += row_bytes + 1
        if filter_type > 4:
            raise CaptureContractError("PNG uses an unknown row filter")
        decoded = bytearray(row_bytes)
        for index, value in enumerate(encoded):
            left = decoded[index - channels] if index >= channels else 0
            above = prior[index]
            upper_left = prior[index - channels] if index >= channels else 0
            predictor = (0, left, above, (left + above) // 2,
                         _paeth(left, above, upper_left))[filter_type]
            decoded[index] = (value + predictor) & 0xff
        pixels.extend(decoded)
        prior = decoded
    return width, height, bytes(pixels), channels


def validate_capture(
    path: Path, *, expected_width: int = 1920, expected_height: int = 1080,
    minimum_colors: int = 16,
) -> dict[str, object]:
    """Return canonical evidence for one exact, visibly non-uniform PNG."""

    width, height, pixels, channels = _decode_png(path)
    if (width, height) != (expected_width, expected_height):
        raise CaptureContractError("captured framebuffer dimensions are not exact")
    distinct: set[bytes] = set()
    # Sample a deterministic 128x72 grid; this catches blank/failed frames
    # without retaining millions of Python objects for a 1080p image.
    for row in range(72):
        y = min(height - 1, row * height // 72)
        for column in range(128):
            x = min(width - 1, column * width // 128)
            start = (y * width + x) * channels
            distinct.add(pixels[start:start + channels])
            if len(distinct) >= minimum_colors:
                break
        if len(distinct) >= minimum_colors:
            break
    if len(distinct) < minimum_colors:
        raise CaptureContractError("captured framebuffer is visually uniform")
    return {
        "tool": "weston-screenshooter",
        "path": "desktop-1080p.png",
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "byteCount": path.stat().st_size,
        "width": width,
        "height": height,
        "sampledDistinctColors": len(distinct),
    }


def capture_parent_frame(
    executable: Path, environment: Mapping[str, str], artifact_directory: Path,
) -> dict[str, object]:
    """Capture only the caller-provided private parent Wayland socket."""

    if not executable.is_absolute() or not executable.is_file():
        raise CaptureContractError("screenshooter executable is not an exact file")
    artifact_directory.mkdir(parents=True, exist_ok=True)
    before = set(artifact_directory.glob("wayland-screenshot-*.png"))
    if before or (artifact_directory / "desktop-1080p.png").exists():
        raise CaptureContractError("capture destination is not fresh")
    # AGENT-GUARD: Never inherit a display endpoint here. The outer sandbox
    # removed host endpoints, and the caller must name only the private parent.
    display = environment.get("WAYLAND_DISPLAY", "")
    if display != "qindaqt-parent-wayland" or os.path.isabs(display):
        raise CaptureContractError("screenshooter target is not the private parent socket")
    result = subprocess.run(
        [str(executable)], cwd=artifact_directory, env=dict(environment),
        stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, timeout=5, check=False,
    )
    if result.returncode != 0:
        raise CaptureContractError(
            f"weston-screenshooter failed with status {result.returncode}: {result.stdout}"
        )
    created = set(artifact_directory.glob("wayland-screenshot-*.png")) - before
    if len(created) != 1:
        raise CaptureContractError("screenshooter did not create exactly one fresh PNG")
    source = created.pop()
    target = artifact_directory / "desktop-1080p.png"
    source.rename(target)
    return validate_capture(target)
