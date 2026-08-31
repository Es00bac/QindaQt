# SPDX-License-Identifier: GPL-3.0-or-later
"""Trace the essential staged-session process boundary for a private row."""

from __future__ import annotations

import argparse
import ctypes
import errno
import json
import os
import signal
import subprocess
import sys
import time
from pathlib import Path
from typing import Any, Sequence


LIFECYCLE_TRACE_ENVIRONMENT = "QINDAQT_DESKTOP_LIFECYCLE_TRACE"
TRACE_ROLES = ("session", "notification", "shell")


def _append_event(trace: Path, role: str, event: str, **details: Any) -> None:
    document = {
        "schemaVersion": 1,
        "monotonicNs": time.monotonic_ns(),
        "role": role,
        "event": event,
        **details,
    }
    encoded = (
        json.dumps(document, sort_keys=True, separators=(",", ":")) + "\n"
    ).encode("utf-8")
    if len(encoded) > 4096:
        raise RuntimeError("session lifecycle event exceeds its atomic write bound")
    descriptor = os.open(
        trace,
        os.O_APPEND | os.O_CLOEXEC | os.O_CREAT | os.O_WRONLY,
        0o600,
    )
    try:
        written = os.write(descriptor, encoded)
        if written != len(encoded):
            raise OSError(errno.EIO, "short session lifecycle trace write")
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


def _forwarded_descriptors(arguments: Sequence[str]) -> tuple[int, ...]:
    descriptors: list[int] = []
    for index, argument in enumerate(arguments[:-1]):
        if argument != "--presentation-token-fd":
            continue
        try:
            descriptor = int(arguments[index + 1], 10)
        except ValueError:
            continue
        if descriptor >= 3:
            descriptors.append(descriptor)
    return tuple(sorted(set(descriptors)))


def _parent_death_guard(parent_process_id: int) -> None:
    """Keep a traced target inside the supervisor's essential-child lifetime."""

    libc = ctypes.CDLL(None, use_errno=True)
    if libc.prctl(1, signal.SIGKILL, 0, 0, 0) != 0:
        os._exit(127)
    if os.getppid() != parent_process_id:
        os._exit(127)


def _exec_only(
    role: str, executable: Path, arguments: list[str], trace: Path
) -> int:
    _append_event(
        trace,
        role,
        "exec-requested",
        executable=str(executable),
        processId=os.getpid(),
        parentProcessId=os.getppid(),
        arguments=arguments,
    )
    try:
        os.execv(executable, [str(executable), *arguments])
    except OSError as error:
        _append_event(
            trace,
            role,
            "exec-failed",
            executable=str(executable),
            processId=os.getpid(),
            errorNumber=error.errno,
            error=str(error),
        )
        return 127
    raise AssertionError("os.execv returned unexpectedly")


def _supervise_child(
    role: str, executable: Path, arguments: list[str], trace: Path
) -> int:
    wrapper_process_id = os.getpid()
    command = [str(executable), *arguments]
    _append_event(
        trace,
        role,
        "launch-requested",
        executable=str(executable),
        wrapperProcessId=wrapper_process_id,
        parentProcessId=os.getppid(),
        arguments=arguments,
    )
    try:
        process = subprocess.Popen(
            command,
            stdin=subprocess.DEVNULL,
            pass_fds=_forwarded_descriptors(arguments),
            preexec_fn=lambda: _parent_death_guard(wrapper_process_id),
        )
    except OSError as error:
        _append_event(
            trace,
            role,
            "exec-failed",
            executable=str(executable),
            wrapperProcessId=wrapper_process_id,
            errorNumber=error.errno,
            error=str(error),
        )
        return 127

    _append_event(
        trace,
        role,
        "started",
        executable=str(executable),
        wrapperProcessId=wrapper_process_id,
        processId=process.pid,
    )
    forwarded_signal = 0

    def forward(signum: int, _frame: Any) -> None:
        nonlocal forwarded_signal
        forwarded_signal = signum
        _append_event(
            trace,
            role,
            "teardown-requested",
            executable=str(executable),
            wrapperProcessId=wrapper_process_id,
            processId=process.pid,
            signal=signum,
        )
        if process.poll() is None:
            process.send_signal(signum)

    for signum in (signal.SIGHUP, signal.SIGINT, signal.SIGTERM):
        signal.signal(signum, forward)

    return_code = process.wait()
    if return_code < 0:
        terminal_signal = -return_code
        exit_code: int | None = None
        teardown_cause = (
            f"forwarded-signal-{forwarded_signal}"
            if forwarded_signal
            else f"target-signal-{terminal_signal}"
        )
    else:
        terminal_signal = None
        exit_code = return_code
        teardown_cause = (
            f"forwarded-signal-{forwarded_signal}"
            if forwarded_signal
            else "target-exit"
        )
    _append_event(
        trace,
        role,
        "exited",
        executable=str(executable),
        wrapperProcessId=wrapper_process_id,
        processId=process.pid,
        exitCode=exit_code,
        signal=terminal_signal,
        teardownCause=teardown_cause,
    )
    return return_code if return_code >= 0 else 128 + terminal_signal


def parse_arguments(arguments: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--role", choices=TRACE_ROLES, required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--trace", type=Path, required=True)
    parser.add_argument("--exec-only", action="store_true")
    parser.add_argument("arguments", nargs=argparse.REMAINDER)
    parsed = parser.parse_args(arguments)
    if parsed.arguments[:1] == ["--"]:
        parsed.arguments = parsed.arguments[1:]
    if not parsed.executable.is_absolute() or not parsed.trace.is_absolute():
        parser.error("executable and trace paths must be absolute")
    return parsed


def main(arguments: Sequence[str] | None = None) -> int:
    parsed = parse_arguments(arguments)
    if parsed.exec_only:
        return _exec_only(
            parsed.role, parsed.executable, parsed.arguments, parsed.trace
        )
    return _supervise_child(
        parsed.role, parsed.executable, parsed.arguments, parsed.trace
    )


if __name__ == "__main__":
    raise SystemExit(main())
