# SPDX-License-Identifier: GPL-3.0-or-later
"""Pixel evidence for the shade-visibility nested rows.

Reads the private virtual KWin's own output framebuffer from its memfd
swapchain, encodes lossless PNGs, and measures whether an area still shows
stale member content after a container rolls up.
"""

from __future__ import annotations

import binascii
import hashlib
import os
import struct
import sys
import zlib
from dataclasses import dataclass
from pathlib import Path

_PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


class CaptureError(RuntimeError):
    """The framebuffer could not be read or identified unambiguously."""


@dataclass(frozen=True)
class Frame:
    """One captured output in native-endian 32-bit XRGB rows."""

    pixels: bytes
    width: int
    height: int
    scale: float
    method: str

    @property
    def sha256(self) -> str:
        return hashlib.sha256(self.pixels).hexdigest()

    def rgb(self, x: int, y: int) -> tuple[int, int, int]:
        offset = (y * self.width + x) * 4
        if sys.byteorder == "little":
            return self.pixels[offset + 2], self.pixels[offset + 1], self.pixels[offset]
        return self.pixels[offset + 1], self.pixels[offset + 2], self.pixels[offset + 3]


def swapchain_buffers(pid: int, width: int, height: int) -> dict[int, bytes]:
    """Every distinct output-sized memfd of `pid`, keyed by inode (swapchain slot).

    AGENT-GUARD: /usr/bin/kwin_wayland carries file capabilities
    (cap_sys_nice), which makes the process non-dumpable and its /proc fd
    table unreadable. The runner launches a byte-identical capability-free
    copy for exactly this reason; do not "simplify" it back to the host binary.
    """
    expected = width * height * 4
    buffers: dict[int, bytes] = {}
    directory = Path(f"/proc/{pid}/fd")
    try:
        entries = sorted(directory.iterdir(), key=lambda path: int(path.name))
    except OSError as error:
        raise CaptureError(f"cannot read compositor fd table: {error}") from error
    for entry in entries:
        try:
            if "memfd:" not in os.readlink(entry):
                continue
            info = entry.stat()
            if info.st_size != expected or info.st_ino in buffers:
                continue
            pixels = entry.read_bytes()
        except OSError:
            continue
        if len(pixels) == expected:
            buffers[info.st_ino] = pixels
    return buffers


def select_fresh_buffer(before: dict[int, bytes], after: dict[int, bytes]) -> tuple[bytes, str]:
    """Pick the current front buffer across one forced repaint.

    KWin's QPainter swapchain keeps two output-sized buffers. The caller
    samples all slots, forces exactly one new frame (a one-pixel pointer
    nudge), and samples again. KWin repairs the slot it renders into with
    buffer-age damage, so the single slot whose bytes changed is complete and
    current. A static single-buffer output is accepted as-is; anything else is
    ambiguous and rejected rather than guessed.
    """
    changed = [inode for inode, pixels in after.items() if before.get(inode) != pixels]
    if len(changed) == 1:
        return after[changed[0]], "freshly-rendered-slot"
    if after and len(set(after.values())) == 1:
        return next(iter(after.values())), "single-distinct-buffer"
    raise CaptureError(
        f"ambiguous framebuffer: {len(after)} buffers, {len(changed)} changed after repaint")


def _chunk(kind: bytes, payload: bytes) -> bytes:
    body = kind + payload
    return (struct.pack(">I", len(payload)) + body
            + struct.pack(">I", binascii.crc32(body) & 0xFFFFFFFF))


def encode_png(frame: Frame) -> bytes:
    """Lossless RGBA PNG of a native-endian XRGB framebuffer."""
    if len(frame.pixels) != frame.width * frame.height * 4:
        raise CaptureError("pixel buffer size does not match the frame dimensions")
    rgba = bytearray(len(frame.pixels))
    if sys.byteorder == "little":
        rgba[0::4] = frame.pixels[2::4]
        rgba[1::4] = frame.pixels[1::4]
        rgba[2::4] = frame.pixels[0::4]
    else:
        rgba[0::4] = frame.pixels[1::4]
        rgba[1::4] = frame.pixels[2::4]
        rgba[2::4] = frame.pixels[3::4]
    rgba[3::4] = b"\xff" * (frame.width * frame.height)
    raw = bytearray()
    row = frame.width * 4
    for y in range(frame.height):
        raw.append(0)
        raw.extend(rgba[y * row : (y + 1) * row])
    header = struct.pack(">IIBBBBB", frame.width, frame.height, 8, 6, 0, 0, 0)
    return (_PNG_SIGNATURE + _chunk(b"IHDR", header)
            + _chunk(b"IDAT", zlib.compress(bytes(raw), 6)) + _chunk(b"IEND", b""))


def parse_colour(value: str) -> tuple[int, int, int]:
    if len(value) != 7 or not value.startswith("#"):
        raise ValueError(f"colour must be #rrggbb: {value!r}")
    return tuple(int(value[index : index + 2], 16) for index in (1, 3, 5))  # type: ignore[return-value]


def area_mismatch(frame: Frame,
                  area: tuple[float, float, float, float],
                  excluded: tuple[float, float, float, float] | None,
                  expected: str,
                  step: int = 3,
                  tolerance: int = 3) -> dict[str, object]:
    """Count sampled device pixels in a logical area that differ from `expected`.

    `excluded` (logical, typically the shaded strip) is skipped with a
    two-device-pixel margin for antialiased chrome edges. Zero mismatches over
    the vacated area is the ghost-free criterion; a boolean shaded flag or a
    hidden-window inventory bit is never accepted in its place.
    """
    target = parse_colour(expected)
    ax, ay, aw, ah = (value * frame.scale for value in area)
    left, top = max(0, int(ax)), max(0, int(ay))
    right, bottom = min(frame.width, int(ax + aw)), min(frame.height, int(ay + ah))
    skip = [value * frame.scale for value in excluded] if excluded else None
    mismatched = sampled = 0
    bounds: list[int] | None = None
    samples: list[dict[str, object]] = []
    for y in range(top, bottom, step):
        for x in range(left, right, step):
            if skip and (skip[0] - 2 <= x <= skip[0] + skip[2] + 2
                         and skip[1] - 2 <= y <= skip[1] + skip[3] + 2):
                continue
            sampled += 1
            colour = frame.rgb(x, y)
            if any(abs(colour[index] - target[index]) > tolerance for index in range(3)):
                mismatched += 1
                bounds = [x, y, x, y] if bounds is None else [
                    min(bounds[0], x), min(bounds[1], y), max(bounds[2], x), max(bounds[3], y)]
                if len(samples) < 8:
                    samples.append({"x": x, "y": y, "rgb": list(colour)})
    return {"mismatched": mismatched, "sampled": sampled, "bounds": bounds, "samples": samples}


def pixel_matches(frame: Frame, point: tuple[float, float], expected: str,
                  tolerance: int = 3) -> bool:
    target = parse_colour(expected)
    colour = frame.rgb(int(point[0] * frame.scale), int(point[1] * frame.scale))
    return all(abs(colour[index] - target[index]) <= tolerance for index in range(3))
