# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded held-descriptor observations for private cohort evidence."""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path
import re
import stat

MAX_EVIDENCE = 16 * 1024 * 1024


def _version(info):
    return (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns,
            info.st_ctime_ns, info.st_mode, info.st_nlink)


def read_evidence(path: Path, *, readonly: bool = False) -> bytes:
    descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK | os.O_CLOEXEC)
    try:
        before = os.fstat(descriptor)
        if (not stat.S_ISREG(before.st_mode) or not 0 < before.st_size <= MAX_EVIDENCE
                or before.st_nlink != 1 or readonly and before.st_mode & 0o022):
            raise ValueError("evidence must be a bounded single-link regular file")
        data = bytearray()
        while chunk := os.read(descriptor, min(65536, MAX_EVIDENCE + 1 - len(data))):
            data.extend(chunk)
            if len(data) > MAX_EVIDENCE:
                raise ValueError("evidence grew outside its bound")
        after = os.fstat(descriptor)
        named = path.stat(follow_symlinks=False)
        if _version(before) != _version(after) or _version(after) != _version(named):
            raise ValueError("evidence changed while observed")
        if len(data) != before.st_size:
            raise ValueError("incomplete evidence read")
        return bytes(data)
    finally:
        os.close(descriptor)


def _unique(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON evidence key")
        result[key] = value
    return result


def json_value(data: bytes):
    return json.loads(data, object_pairs_hook=_unique)


def record(path: Path) -> dict:
    path = path.resolve(strict=True)
    raw = read_evidence(path, readonly=True)
    return {"path": str(path), "sha256": hashlib.sha256(raw).hexdigest(), "bytes": len(raw)}


def exact_record(value: dict) -> bytes:
    if (not isinstance(value, dict) or set(value) != {"path", "sha256", "bytes"}
            or not isinstance(value["path"], str)
            or not 0 < len(value["path"]) <= 4096
            or type(value["bytes"]) is not int or not 0 < value["bytes"] <= MAX_EVIDENCE
            or not isinstance(value["sha256"], str)
            or not re.fullmatch("[0-9a-f]{64}", value["sha256"])):
        raise ValueError("unknown or malformed evidence record")
    path = Path(value["path"])
    if not path.is_absolute() or str(path.resolve(strict=True)) != value["path"]:
        raise ValueError("evidence path must be canonical")
    raw = read_evidence(path, readonly=True)
    if len(raw) != value["bytes"] or hashlib.sha256(raw).hexdigest() != value["sha256"]:
        raise ValueError("pinned evidence changed")
    return raw


def snapshot(source: Path, destination: Path) -> dict:
    raw = read_evidence(source)
    descriptor = os.open(destination, os.O_WRONLY | os.O_CREAT | os.O_EXCL
                         | os.O_NOFOLLOW | os.O_CLOEXEC, 0o600)
    try:
        pending = memoryview(raw)
        while pending:
            written = os.write(descriptor, pending)
            if written <= 0:
                raise OSError("evidence snapshot write failed")
            pending = pending[written:]
        os.fchmod(descriptor, 0o400)
    finally:
        os.close(descriptor)
    return record(destination)
