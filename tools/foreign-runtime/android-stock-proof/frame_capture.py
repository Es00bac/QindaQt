"""Bounded pixels from the exact private guest compositor; never host capture."""
import base64
import hashlib
import os
from pathlib import Path
import stat
import struct
import sys
import zlib

WIDTH, HEIGHT = 1280, 800
SIZE = WIDTH * HEIGHT * 4


def process_version(pid):
    root = Path("/proc") / str(pid)
    fields = (root / "stat").read_text().rsplit(")", 1)[1].split()
    executable = root / "exe"
    resolved = executable.resolve(strict=True)
    expected = Path("/usr/bin/qindaqt-kwin").resolve(strict=True)
    if pid <= 1 or resolved != expected or fields[0] == "Z":
        raise RuntimeError("capture-compositor-identity")
    info = executable.stat()
    return (fields[19], info.st_dev, info.st_ino)


def png(pixels):
    if len(pixels) != SIZE or sys.byteorder != "little":
        raise RuntimeError("capture-frame-format")
    rgba = bytearray(SIZE)
    rgba[0::4], rgba[1::4], rgba[2::4] = pixels[2::4], pixels[1::4], pixels[0::4]
    rgba[3::4] = b"\xff" * (WIDTH * HEIGHT)
    raw = b"".join(b"\0" + rgba[y * WIDTH * 4:(y + 1) * WIDTH * 4]
                   for y in range(HEIGHT))
    def chunk(kind, data):
        return (struct.pack(">I", len(data)) + kind + data
                + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff))
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 3)) + chunk(b"IEND", b""))


def capture(pid, version, label, guard):
    # AGENT-GUARD: bind both sides of the read to the initiating bus owner and
    # exact process incarnation. Multiple distinct buffers are ambiguity, not
    # an invitation to choose the most visually convenient frame.
    guard()
    if process_version(pid) != version:
        raise RuntimeError("capture-compositor-replaced")
    directory = Path("/proc") / str(pid) / "fd"
    entries = list(directory.iterdir())
    if len(entries) > 4096:
        raise RuntimeError("capture-fd-bound")
    candidates = {}
    matching = 0
    for entry in entries:
        try:
            if os.readlink(entry) not in ("memfd:shm (deleted)", "/memfd:shm (deleted)"):
                continue
            fd = os.open(entry, os.O_RDONLY | os.O_CLOEXEC)
            try:
                before = os.fstat(fd)
                if not stat.S_ISREG(before.st_mode) or before.st_size != SIZE:
                    continue
                matching += 1
                if matching > 32 or os.readlink("/proc/self/fd/" + str(fd)) not in (
                        "memfd:shm (deleted)", "/memfd:shm (deleted)"):
                    raise RuntimeError("capture-held-buffer-bound-or-kind")
                pixels = os.pread(fd, SIZE + 1, 0)
                after = os.fstat(fd)
                if (before.st_dev, before.st_ino, before.st_size) != (
                        after.st_dev, after.st_ino, after.st_size) or len(pixels) != SIZE:
                    raise RuntimeError("capture-buffer-replaced")
                if os.pread(fd, SIZE + 1, 0) != pixels:
                    continue
                candidates.setdefault(hashlib.sha256(pixels).hexdigest(), pixels)
                if len(candidates) > 1:
                    raise RuntimeError("capture-distinct-buffer-ambiguity")
            finally:
                os.close(fd)
        except FileNotFoundError:
            continue
    guard()
    if process_version(pid) != version or len(candidates) != 1:
        raise RuntimeError("capture-ambiguous-or-lost")
    digest, pixels = next(iter(candidates.items()))
    # Exclude cursor and clock/status edges from a minimal content test.
    samples = {pixels[(y * WIDTH + x) * 4:(y * WIDTH + x) * 4 + 3]
               for y in range(100, 700, 8) for x in range(100, 1180, 8)}
    if len(samples) < 8:
        raise RuntimeError("capture-content-uniform")
    encoded = png(pixels)
    if len(encoded) > 512 * 1024:
        raise RuntimeError("capture-output-bound")
    return {"label": label, "width": WIDTH, "height": HEIGHT, "sha256": digest,
            "pngSha256": hashlib.sha256(encoded).hexdigest(),
            "pngBase64": base64.b64encode(encoded).decode("ascii")}, pixels
