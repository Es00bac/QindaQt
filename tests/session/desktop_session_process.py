# SPDX-License-Identifier: GPL-3.0-or-later
"""PID/executable authenticated process tracking and bounded teardown."""

from __future__ import annotations

import os
import signal
import subprocess
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable, Iterable, Mapping

from desktop_session_topology import DesktopTopology, desktop_1080p_topology


class ProcessContractError(RuntimeError):
    """Process identity or teardown could not be proven safely."""


@dataclass(frozen=True)
class ProcessIdentity:
    role: str
    pid: int
    process_group: int
    executable: Path
    start_ticks: int


@dataclass(frozen=True)
class CleanupRecord:
    role: str
    pid: int
    process_group: int
    executable: Path
    start_ticks: int
    terminal_phase: str

    def document(self) -> dict[str, object]:
        return {
            "role": self.role,
            "pid": self.pid,
            "processGroup": self.process_group,
            "executablePath": str(self.executable),
            "startTicks": self.start_ticks,
            "terminalPhase": self.terminal_phase,
        }


@dataclass
class RuntimeState:
    processes: list[subprocess.Popen[str]] = field(default_factory=list)
    spawned: list[tuple[subprocess.Popen[str], list[Path]]] = field(default_factory=list)
    identities: list[ProcessIdentity] = field(default_factory=list)

    def track(self, process: subprocess.Popen[str], allowed: list[Path]) -> None:
        self.processes.append(process)
        self.spawned.append((process, allowed))


def spawn_logged_process(
    name: str, command: list[str], environment: Mapping[str, str]
) -> subprocess.Popen[str]:
    """Start one isolated group with its complete output in the private log root."""

    log = Path(f"/var/log/qindaqt-desktop/{name}.log").open("w", encoding="utf-8")
    try:
        process = subprocess.Popen(
            command, env=dict(environment), stdout=log, stderr=subprocess.STDOUT,
            text=True, start_new_session=True,
        )
    except BaseException:
        log.close()
        raise
    process._qindaqt_log = log  # type: ignore[attr-defined]
    return process


def wait_for_path(path: Path, state: RuntimeState, seconds: float) -> None:
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if path.exists():
            return
        failed = [process.pid for process in state.processes if process.poll() is not None]
        if failed:
            raise RuntimeError(f"required process exited before {path}: {failed}")
        time.sleep(0.02)
    raise RuntimeError(f"timed out waiting for {path}")


def _proc_record(pid: int, proc_root: Path = Path("/proc")) -> tuple[str, int]:
    process = proc_root / str(pid)
    executable = (process / "exe").resolve(strict=True).name
    contents = (process / "stat").read_text(encoding="ascii")
    closing = contents.rfind(")")
    fields = contents[closing + 2 :].split()
    if closing < 2 or len(fields) < 2:
        raise RuntimeError(f"malformed process stat for PID {pid}")
    return executable, int(fields[1], 10)


def process_evidence(
    probe: Mapping[str, Any], topology: DesktopTopology | None = None,
    *,
    proc_root: Path = Path("/proc"),
    role_pid_hints: Mapping[str, int] | None = None,
    direct_parent_pid: int | None = None,
) -> tuple[dict[str, Any], dict[str, int]]:
    """Bind each exact topology role to one live executable and parent role."""

    topology = topology or desktop_1080p_topology()
    by_executable: dict[str, list[Any]] = {}
    for item in topology.processes:
        if item.role != "session-probe":
            by_executable.setdefault(item.executable, []).append(item)
    compositor_pid = 0
    for raw in probe.get("services", []):
        if (
            isinstance(raw, Mapping)
            and raw.get("name") == "org.qindaqt.Compositor"
            and raw.get("status") == "owned"
        ):
            try:
                compositor_pid = int(str(raw.get("pid", "0")), 10)
            except ValueError:
                compositor_pid = 0
    observed: dict[str, tuple[int, int]] = {}
    for entry in proc_root.iterdir():
        if not entry.name.isdigit():
            continue
        try:
            process_id = int(entry.name)
            executable, parent = _proc_record(process_id, proc_root)
        except (OSError, RuntimeError, ValueError):
            continue
        expectations = by_executable.get(executable, [])
        if not expectations:
            continue
        if len(expectations) == 1:
            role = expectations[0].role
        elif {item.role for item in expectations} == {
            "parent-compositor", "compositor"
        }:
            # AGENT-CONTRACT: A fractional S3 run has a raw KWin parent and a
            # QindaQt KWin child. Only the child owns the separately probed
            # compositor service; the other exact executable is the parent.
            hints = role_pid_hints or {}
            parent_hint = hints.get("parent-compositor", 0)
            child_hint = hints.get("compositor", 0)
            if (
                compositor_pid <= 1
                or child_hint != compositor_pid
                or parent_hint <= 1
                or parent_hint == child_hint
                or process_id not in {parent_hint, child_hint}
                or direct_parent_pid is None
                or direct_parent_pid <= 1
                or parent != direct_parent_pid
            ):
                raise RuntimeError(
                    "duplicate KWin roles lack exact service, spawn, and ancestry evidence"
            )
            role = "compositor" if process_id == child_hint else "parent-compositor"
        elif {item.role for item in expectations} == {
            "parent-private-bus", "private-bus"
        }:
            hints = role_pid_hints or {}
            parent_hint = hints.get("parent-private-bus", 0)
            child_hint = hints.get("private-bus", 0)
            if (
                parent_hint <= 1
                or child_hint <= 1
                or parent_hint == child_hint
                or process_id not in {parent_hint, child_hint}
                or direct_parent_pid is None
                or direct_parent_pid <= 1
                or parent != direct_parent_pid
            ):
                raise RuntimeError(
                    "duplicate D-Bus roles lack exact spawn and ancestry evidence"
                )
            role = "private-bus" if process_id == child_hint else "parent-private-bus"
        else:
            raise RuntimeError(f"executable {executable!r} has ambiguous process roles")
        if role in observed:
            raise RuntimeError(f"multiple processes claimed {role}")
        observed[role] = (process_id, parent)
    observed["session-probe"] = (
        int(str(probe.get("selfPid", "0"))), int(str(probe.get("parentPid", "0")))
    )
    if set(observed) != {item.role for item in topology.processes}:
        raise RuntimeError(f"process topology incomplete: {sorted(observed)}")
    roles_by_pid = {pid: role for role, (pid, _) in observed.items()}
    records: dict[str, Any] = {}
    pids: dict[str, int] = {}
    for expected in topology.processes:
        pid, parent_pid = observed[expected.role]
        records[expected.role] = {
            "pid": pid, "executable": expected.executable,
            "parentRole": roles_by_pid.get(parent_pid),
        }
        pids[expected.role] = pid
    return records, pids


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
) -> list[CleanupRecord]:
    """TERM then KILL exact authenticated groups and report terminal phases.

    PID reuse is treated as the original process having exited. A reused PID is
    never signalled because every signal phase revalidates executable/starttime.
    Phase names identify the bounded observation phase, not graceful exit.
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

    phase_by_pid: dict[int, str] = {}
    remaining = live()
    remaining_pids = {item.pid for item in remaining}
    for item in tracked:
        if item.pid not in remaining_pids:
            phase_by_pid[item.pid] = "already-exited"
    if remaining:
        before = {item.pid for item in remaining}
        signal_live_groups(signal.SIGTERM)
        remaining = await_absent(term_seconds)
        after = {item.pid for item in remaining}
        for pid in before - after:
            phase_by_pid[pid] = "term"
    if remaining:
        before = {item.pid for item in remaining}
        signal_live_groups(signal.SIGKILL)
        remaining = await_absent(kill_seconds)
        after = {item.pid for item in remaining}
        for pid in before - after:
            phase_by_pid[pid] = "kill"
    if remaining:
        pids = [item.pid for item in remaining]
        raise ProcessContractError(f"authenticated processes survived teardown: {pids}")
    if set(phase_by_pid) != {item.pid for item in tracked}:
        raise ProcessContractError("cleanup omitted an authenticated terminal phase")
    return [
        CleanupRecord(
            item.role,
            item.pid,
            item.process_group,
            item.executable,
            item.start_ticks,
            phase_by_pid[item.pid],
        )
        for item in tracked
    ]
