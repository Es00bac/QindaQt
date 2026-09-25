#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Validate a compat-db-v1 document with the rules QindaLutris applies.

    validate.py FILE [FILE...]

Exit status 0 when every file is accepted, 1 when any is refused. The rules
live in qlcompat/schema.py, the twin of the C++ parser (AGENT-CONTRACT there).
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from qlcompat import schema  # noqa: E402


def main(argv=None) -> int:
    paths = sys.argv[1:] if argv is None else argv
    if not paths:
        print("usage: validate.py FILE [FILE...]", file=sys.stderr)
        return 2
    status = 0
    for name in paths:
        path = Path(name)
        try:
            if path.is_symlink() or not path.is_file():
                raise schema.Refused("not a regular file (symlinks are refused)")
            if path.stat().st_size > schema.MAX_DB_BYTES:
                raise schema.Refused("document too large")
            document = schema.load_bytes(path.read_bytes())
        except (schema.Refused, OSError) as error:
            print(f"{name}: refused: {error}", file=sys.stderr)
            status = 1
            continue
        print(f"{name}: ok ({len(document['games'])} games, generated "
              f"{document['generated']})")
    return status


if __name__ == "__main__":
    raise SystemExit(main())
