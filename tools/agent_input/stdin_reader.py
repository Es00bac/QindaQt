# SPDX-License-Identifier: GPL-3.0-or-later
"""Incremental UTF-8 stdin reader with residual line buffering.

AGENT-GUARD: Always call feed() before treating HUP as EOF. A closed pipe
reports both IN and HUP; reading first drains the final data chunk before
the watch callback closes the session.
"""
from __future__ import annotations

import codecs


class StdinReader:
    """Stateful decoder that buffers partial lines across chunked reads."""

    def __init__(self) -> None:
        self._decoder = codecs.getincrementaldecoder("utf-8")("replace")
        self._buf = ""

    def feed(self, data: bytes) -> list[str]:
        """Decode bytes; return complete newline-terminated lines.

        Partial trailing lines are kept internally until the next feed or flush.
        """
        text = self._decoder.decode(data, False)
        parts = (self._buf + text).split("\n")
        self._buf = parts[-1]
        return parts[:-1]

    def flush(self) -> list[str]:
        """Flush decoder state and residual buffer at EOF.

        Returns any remaining non-empty lines.
        """
        text = self._decoder.decode(b"", True)
        parts = (self._buf + text).split("\n")
        self._buf = ""
        return [p for p in parts if p.strip()]
