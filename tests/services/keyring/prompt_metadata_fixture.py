# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded QMP2 decoder for isolated synthetic prompt helpers."""
import sys

def read_metadata():
    frame = sys.stdin.buffer.read(12289)
    assert 23 <= len(frame) <= 12288 and frame[:4] == b"QMP2"
    cursor = 4
    def number(size):
        nonlocal cursor
        assert cursor + size <= len(frame)
        value = int.from_bytes(frame[cursor:cursor+size], "big")
        cursor += size
        return value
    def text(limit):
        nonlocal cursor
        size = number(2)
        assert size <= limit and cursor + size <= len(frame)
        value = frame[cursor:cursor+size].decode("utf-8")
        assert "\0" not in value
        cursor += size
        return value
    result = {"pid": number(4), "uid": number(4)}
    for key, limit in [("label",1024),("caller",256),("executable",4096),("operation",32),("application",256)]:
        result[key] = text(limit)
    count = number(1)
    assert count <= 6
    result["parents"] = [text(1024) for _ in range(count)]
    assert cursor == len(frame)
    return result
