# SPDX-License-Identifier: GPL-3.0-or-later
"""The Terminal PTY proof's sentinel contract.

Pure, side-effect-free definitions of what gets delivered into the Terminal
and what the Terminal's own shell must therefore have written back. Kept
apart from every runtime module so the exact byte expectation stays
unit-testable without holding the private runtime lane.

AGENT-CONTRACT: the delivery module and the readback assertion must both
derive their bytes from these functions. A second, independently written
expectation would let a delivery bug and a readback bug cancel out.
"""

from __future__ import annotations

import uuid


def sentinel_output_filename(run_id: str) -> str:
    return f"gabbee-terminal-pty-sentinel-{run_id}.txt"


def generate_sentinel(run_id: str) -> str:
    """A fresh, unique, non-ASCII payload that cannot be a stale/replayed match."""

    return f"QINDAQT-PTY-{run_id}-{uuid.uuid4().hex}-✓日本語★"


def expected_sentinel_bytes(sentinel: str) -> bytes:
    """The exact bytes the shell's ``cat`` should have written.

    The delivered keystrokes are the sentinel followed by one Return (so the
    terminal line discipline flushes the full line to ``cat``) and then one
    Ctrl-D on the now-empty line, which signals true EOF without adding any
    further bytes.
    """

    return sentinel.encode("utf-8") + b"\n"
