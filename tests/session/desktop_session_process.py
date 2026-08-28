# SPDX-License-Identifier: GPL-3.0-or-later
"""PID/executable authenticated process tracking and bounded teardown."""

from __future__ import annotations

import os
import signal
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable


class ProcessContractError(RuntimeError):
    """Process identity or teardown could not be proven safely."""


@dataclass(frozen=True)
class ProcessIdentity:
    role: str
    pid: int
    process_group: int
    executable: Path
    start_ticks: int


def _proc_stat_start_ticks(contents: str) -> int:
    closing = contents.rfind(")")
    if closing < 2:
        raise ProcessContractError("/proc stat omitted the command delimiter")
    fields = contents[closing + 2 :].split()
    # fields begins at the documented field 3 (`state`); starttime is field 22.
    if len(fields) < 20:
        raise ProcessContractError("/proc stat was truncated")
    try:
        value = int(fields[19], 10)
    except ValueError as error:
        raise ProcessContractError("/proc stat starttime is malformed") from error
    if value <= 0:
        raise ProcessContractError("/proc stat starttime must be positive")
    return value


def capture_process_identity(
    role: str,
    pid: int,
    allowed_executables: Iterable[Path],
    *,
    proc_root: Path = Path("/proc"),
) -> ProcessIdentity:
    """Capture both executable and kernel start time before any later signal."""

    if pid <= 1:
        raise ProcessContractError("refusing to track PID 1 or a non-process value")
    allowed = {path.resolve(strict=True) for path in allowed_executables}
    if not allowed:
        raise ProcessContractError("process identity requires an executable allow-list")
    process = proc_root / str(pid)
    try:
        executable = (process / "exe").resolve(strict=True)
        start_ticks = _proc_stat_start_ticks(
            (process / "stat").read_text(encoding="ascii")
        )
        process_group = os.getpgid(pid) if proc_root == Path("/proc") else pid
    except (OSError, UnicodeError) as error:
        raise ProcessContractError(f"could not capture PID {pid}: {error}") from error
    if executable not in allowed:
        raise ProcessContractError(
            f"PID {pid} executable {executable} is outside the role allow-list"
        )
    if process_group <= 1:
        raise ProcessContractError("refusing to track a system process group")
    return ProcessIdentity(role, pid, process_group, executable, start_ticks)


def identity_is_live(
    identity: ProcessIdentity, *, proc_root: Path = Path("/proc")
) -> bool:
    process = proc_root / str(identity.pid)
    try:
        executable = (process / "exe").resolve(strict=True)
        start_ticks = _proc_stat_start_ticks(
            (process / "stat").read_text(encoding="ascii")
        )
    except (OSError, UnicodeError, ProcessContractError):
        return False
    return executable == identity.executable and start_ticks == identity.start_ticks


def terminate_processes(
    identities: Iterable[ProcessIdentity],
    *,
    term_seconds: float = 3.0,
    kill_seconds: float = 2.0,
    proc_root: Path = Path("/proc"),
    signal_group: Callable[[int, int], None] = os.killpg,
    monotonic: Callable[[], float] = time.monotonic,
    sleep: Callable[[float], None] = time.sleep,
) -> list[int]:
    """TERM then KILL exact authenticated groups; return no survivors or fail.

    PID reuse is treated as the original process having exited. A reused PID is
    never signalled because every signal phase revalidates executable/starttime.
    """

    tracked = list(identities)
    if len({item.pid for item in tracked}) != len(tracked):
        raise ProcessContractError("tracked process PIDs must be unique")
    groups: dict[int, ProcessIdentity] = {}
    for item in tracked:
        groups.setdefault(item.process_group, item)

    def live() -> list[ProcessIdentity]:
        return [item for item in tracked if identity_is_live(item, proc_root=proc_root)]

    def signal_live_groups(signum: int) -> None:
        for group, representative in groups.items():
            members = [
                item
                for item in tracked
                if item.process_group == group
                and identity_is_live(item, proc_root=proc_root)
            ]
            if not members:
                continue
            # AGENT-GUARD: Authenticate a currently live recorded member before
            # addressing its group. Never derive a pgid from a potentially
            # reused PID during teardown.
            try:
                signal_group(group, signum)
            except ProcessLookupError:
                continue
            except PermissionError as error:
                raise ProcessContractError(
                    f"could not signal authenticated group for {representative.role}"
                ) from error

    def await_absent(seconds: float) -> list[ProcessIdentity]:
        deadline = monotonic() + seconds
        remaining = live()
        while remaining and monotonic() < deadline:
            sleep(0.02)
            remaining = live()
        return remaining

    remaining = live()
    if remaining:
        signal_live_groups(signal.SIGTERM)
        remaining = await_absent(term_seconds)
    if remaining:
        signal_live_groups(signal.SIGKILL)
        remaining = await_absent(kill_seconds)
    if remaining:
        pids = [item.pid for item in remaining]
        raise ProcessContractError(f"authenticated processes survived teardown: {pids}")
    return []
