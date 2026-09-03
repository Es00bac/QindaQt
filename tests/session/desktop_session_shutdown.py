# SPDX-License-Identifier: GPL-3.0-or-later
"""Orderly compositor-first shutdown helpers for contained desktop rows."""

from __future__ import annotations

import signal
import time
from pathlib import Path
from typing import Callable, Iterable, Protocol


SESSION_CRASH_DIAGNOSTIC = "Session process has crashed"


class OrderlyShutdownError(RuntimeError):
    """An orderly target or its terminal diagnostics were not trustworthy."""


class ProcessIdentityLike(Protocol):
    role: str
    pid: int


def require_clean_session_shutdown(log_path: Path) -> None:
    """Reject KWin's explicit report that its owned session child crashed."""

    try:
        contents = log_path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        raise OrderlyShutdownError(
            f"could not inspect compositor shutdown diagnostics: {error}"
        ) from error
    if SESSION_CRASH_DIAGNOSTIC in contents:
        raise OrderlyShutdownError("KWin reported a crashed session during teardown")


def terminate_orderly_roles(
    tracked: list[ProcessIdentityLike], roles: Iterable[str], *,
    is_live: Callable[[ProcessIdentityLike], bool],
    signal_process: Callable[[int, int], None],
    term_seconds: float, monotonic: Callable[[], float] = time.monotonic,
    sleep: Callable[[float], None] = time.sleep,
) -> dict[int, str]:
    """Give exact named processes a bounded TERM turn before group cleanup."""

    by_role = {item.role: item for item in tracked}
    requested = list(roles)
    if len(by_role) != len(tracked):
        raise OrderlyShutdownError("tracked process roles must be unique")
    if len(set(requested)) != len(requested):
        raise OrderlyShutdownError("orderly process roles must be unique")
    missing = [role for role in requested if role not in by_role]
    if missing:
        raise OrderlyShutdownError(f"orderly process roles are not tracked: {missing}")

    phases: dict[int, str] = {}
    for role in requested:
        target = by_role[role]
        if not is_live(target):
            continue
        before = {item.pid for item in tracked if is_live(item)}
        try:
            # AGENT-GUARD: Signal the authenticated KWin PID, not its shared
            # process group. Simultaneous SIGTERM makes --exit-with-session
            # report a harness-induced qindaqt-session crash.
            signal_process(target.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        except PermissionError as error:
            raise OrderlyShutdownError(
                f"could not signal authenticated process for {target.role}"
            ) from error
        deadline = monotonic() + term_seconds
        while is_live(target) and monotonic() < deadline:
            sleep(0.02)
        after = {item.pid for item in tracked if is_live(item)}
        phases.update({pid: "term" for pid in before - after})
    return phases
