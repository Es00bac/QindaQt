#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Rewrite the family name records of the pinned Controls fixture fonts.

AGENT-CONTRACT: The visual baselines must render vendored glyph bytes only.
Upstream Noto files keep the family names "Noto Sans"/"Noto Sans Mono", which
host images also install, so a fixture that registered them would compete with
the host in Qt's font match and the winner could flip across Qt or fontconfig
updates. This tool renames the families inside the vendored TTFs to
repository-owned names so the fixture cannot collide with any host font. Only
name-table records change; glyph data stays byte-identical to upstream.

Usage: python3 rename_family_names.py [FILE ...]
With no arguments, rewrites every *.ttf beside this script in place. The
records rewritten per font: nameID 1/16 (family), 2/17 are untouched (style),
4 (full name), and 6 (PostScript name). Run once after copying fresh upstream
fonts, then re-run the visual baselines and review the diff.

OFL note: these Noto builds declare no Reserved Font Name, so the modified
files remain distributable under tests/controls/fonts/LICENSE-OFL.txt.
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path

RENAMES = {
    "Noto Sans Mono": "QindaQt Sans Mono",
    "Noto Sans SemiBold": "QindaQt Sans",
    "Noto Sans": "QindaQt Sans",
    "NotoSansMono-Regular": "QindaQtSansMono-Regular",
    "NotoSans-Regular": "QindaQtSans-Regular",
    "NotoSans-SemiBold": "QindaQtSans-SemiBold",
    "NotoSans-Bold": "QindaQtSans-Bold",
}

FAMILY_IDS = {1, 4, 6, 16}


def rewrite_name_table(table: bytes) -> bytes:
    count, storage_offset = struct.unpack(">HH", table[2:6])
    records = []
    strings = bytearray(table[storage_offset:])
    for index in range(count):
        offset = 6 + 12 * index
        platform, encoding, language, name_id, length, string_offset = struct.unpack(
            ">HHHHHH", table[offset : offset + 12]
        )
        raw = table[storage_offset + string_offset : storage_offset + string_offset + length]
        if name_id not in FAMILY_IDS:
            records.append((platform, encoding, language, name_id, bytes(raw)))
            continue
        if platform == 3:
            text = raw.decode("utf-16-be")
        elif platform == 1:
            text = raw.decode("latin-1")
        else:
            records.append((platform, encoding, language, name_id, bytes(raw)))
            continue
        # Longest match first so "Noto Sans Mono" wins over "Noto Sans".
        for old, new in sorted(RENAMES.items(), key=lambda item: -len(item[0])):
            text = text.replace(old, new)
        encoded = text.encode("utf-16-be") if platform == 3 else text.encode("latin-1")
        records.append((platform, encoding, language, name_id, encoded))
    header_size = 6 + 12 * len(records)
    blob = bytearray()
    rebuilt_records = []
    for platform, encoding, language, name_id, raw in records:
        # Offsets are relative to the storage area (stringOffset), not the
        # start of the name table.
        rebuilt_records.append(
            (
                platform,
                encoding,
                language,
                name_id,
                len(blob),
                len(raw),
            )
        )
        blob += raw
    out = bytearray(struct.pack(">HHH", 0, count, header_size))
    for platform, encoding, language, name_id, offset, length in rebuilt_records:
        # Name-record field order is ..., nameID, LENGTH, OFFSET.
        out += struct.pack(
            ">HHHHHH", platform, encoding, language, name_id, length, offset
        )
    out += blob
    return bytes(out)


def rename_font(path: Path) -> None:
    data = bytearray(path.read_bytes())
    num_tables = struct.unpack(">H", data[4:6])[0]
    name_entry_offset = None
    for index in range(num_tables):
        offset = 12 + 16 * index
        if bytes(data[offset : offset + 4]) == b"name":
            name_entry_offset = offset
            break
    if name_entry_offset is None:
        raise SystemExit(f"{path}: no name table found")
    table_offset = struct.unpack(">I", data[name_entry_offset + 8 : name_entry_offset + 12])[
        0
    ]
    table_length = struct.unpack(
        ">I", data[name_entry_offset + 12 : name_entry_offset + 16]
    )[0]
    rewritten = rewrite_name_table(bytes(data[table_offset : table_offset + table_length]))

    # Rebuild the sfnt container with the renamed name table so the file can
    # grow or shrink; every other table is copied byte-identically and the
    # directory entries are re-derived with 4-byte alignment.
    tables = []
    for index in range(num_tables):
        offset = 12 + 16 * index
        tag = bytes(data[offset : offset + 4])
        start = struct.unpack(">I", data[offset + 8 : offset + 12])[0]
        length = struct.unpack(">I", data[offset + 12 : offset + 16])[0]
        payload = rewritten if offset == name_entry_offset else bytes(data[start : start + length])
        tables.append((tag, payload))
    header_size = 12 + 16 * len(tables)
    blob = bytearray()
    directory = bytearray(data[:header_size])
    for index, (_, payload) in enumerate(tables):
        entry = 12 + 16 * index
        directory[entry + 8 : entry + 12] = struct.pack(">I", header_size + len(blob))
        directory[entry + 12 : entry + 16] = struct.pack(">I", len(payload))
        blob += payload
        if len(payload) % 4:
            blob += bytes(4 - len(payload) % 4)
    path.write_bytes(bytes(directory) + bytes(blob))
    print(f"rewrote family names in {path}")


def main() -> int:
    targets = sys.argv[1:]
    if not targets:
        here = Path(__file__).resolve().parent
        targets = sorted(str(path) for path in here.glob("*.ttf"))
    if not targets:
        raise SystemExit("no font files given or found")
    for target in targets:
        rename_font(Path(target))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
