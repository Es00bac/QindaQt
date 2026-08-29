# SPDX-License-Identifier: GPL-3.0-or-later
"""Encode the authenticated private KWin virtual framebuffer as PNG."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
from pathlib import Path

from PyQt6.QtGui import QImage


_EVIDENCE_PREFIX = "QINDAQT_PRIVATE_KWIN_CAPTURE="


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
    if arguments.parent_pid <= 1 or executable.resolve(strict=True) != Path(
        "/usr/bin/kwin_wayland"
    ):
        raise RuntimeError("capture PID is not the exact private KWin executable")
    values = [
        value.decode("utf-8", "strict")
        for value in (process_root / "cmdline").read_bytes().split(b"\0")
        if value
    ]
    expected = [
        "/usr/bin/kwin_wayland",
        "--virtual",
        "--width", str(arguments.expected_logical_width),
        "--height", str(arguments.expected_logical_height),
        "--scale", str(arguments.expected_scale),
        "--output-count", "1",
        "--socket", "qindaqt-parent-wayland",
        "--no-lockscreen",
        "--no-global-shortcuts",
    ]
    if values != expected:
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
    image = QImage(
        pixels,
        arguments.expected_width,
        arguments.expected_height,
        stride,
        QImage.Format.Format_RGB32,
    ).copy()
    if image.isNull() or not image.save(str(output), "PNG"):
        raise RuntimeError("private KWin framebuffer could not be encoded as PNG")
    return {
        "type": "raw",
        "width": arguments.expected_width,
        "height": arguments.expected_height,
        "stride": stride,
        "format": QImage.Format.Format_RGB32.value,
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
