# SPDX-License-Identifier: GPL-3.0-or-later
"""Exact private-process observation and cleanup for notification qualification."""

from __future__ import annotations

import os
import re
import signal
import subprocess
import time
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path


@dataclass
class InnerProcesses:
    settings: subprocess.Popen[str] | None = None
    compositor: subprocess.Popen[str] | None = None
    resident_owner: subprocess.Popen[str] | None = None


def start_logged_process(executable: Path, log_path: Path) -> subprocess.Popen[str]:
    """Start a private child without allowing diagnostics onto result stdout."""
    with log_path.open("a", encoding="utf-8") as log:
        return subprocess.Popen(
            [str(executable)], text=True, stdout=log, stderr=subprocess.STDOUT
        )


def _validate_private_process_group(process_id: int, process_group_id: int) -> int:
    """Reject any cleanup target that is not the new-session leader itself."""
    if (
        process_id <= 1
        or process_group_id != process_id
        or process_group_id == os.getpgrp()
        or process_group_id == os.getpid()
    ):
        raise RuntimeError(
            "refused unsafe private process group: "
            f"pid={process_id} pgid={process_group_id} "
            f"self={os.getpid()} selfPgid={os.getpgrp()}"
        )
    return process_group_id


def validate_private_session_process(process_id: int) -> int:
    """Refuse a signal target outside the disposable driver's session."""
    if process_id <= 1:
        raise RuntimeError(f"refused unsafe private process PID: {process_id}")
    try:
        target_session_id = os.getsid(process_id)
        driver_session_id = os.getsid(0)
    except OSError as error:
        raise RuntimeError(
            f"could not authenticate private process PID {process_id}: {error}"
        ) from error
    if target_session_id != driver_session_id:
        raise RuntimeError(
            "refused process outside private session: "
            f"pid={process_id} targetSid={target_session_id} "
            f"driverSid={driver_session_id}"
        )
    return process_id


def _group_exists(process_group_id: int) -> bool:
    try:
        os.killpg(process_group_id, 0)
    except ProcessLookupError:
        return False
    return True


def _terminate_private_process_group(
    process: subprocess.Popen[str], process_group_id: int
) -> None:
    """Boundedly terminate only the exact new session created by this driver."""
    _validate_private_process_group(process.pid, process_group_id)
    try:
        os.killpg(process_group_id, signal.SIGTERM)
    except ProcessLookupError:
        pass
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        pass
    deadline = time.monotonic() + 3.0
    while _group_exists(process_group_id) and time.monotonic() < deadline:
        time.sleep(0.05)
    if _group_exists(process_group_id):
        try:
            os.killpg(process_group_id, signal.SIGKILL)
        except ProcessLookupError:
            pass
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired as error:
        raise RuntimeError(
            f"private process-group leader {process.pid} did not exit"
        ) from error
    deadline = time.monotonic() + 2.0
    while _group_exists(process_group_id) and time.monotonic() < deadline:
        time.sleep(0.05)
    if _group_exists(process_group_id):
        raise RuntimeError(
            f"private process group {process_group_id} survived bounded teardown"
        )


def run_private_process_group(
    command: Sequence[str], environment: Mapping[str, str], timeout: float
) -> subprocess.CompletedProcess[str]:
    """Run one disposable bus/session tree with exact group-owned cleanup."""
    process = subprocess.Popen(
        command,
        env=environment,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        start_new_session=True,
    )
    try:
        process_group_id = _validate_private_process_group(
            process.pid, os.getpgid(process.pid)
        )
    except (OSError, RuntimeError):
        process.kill()
        process.wait(timeout=5)
        raise
    try:
        stdout, stderr = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired as error:
        _terminate_private_process_group(process, process_group_id)
        stdout, stderr = process.communicate()
        raise RuntimeError(
            f"private process group timed out after {timeout:g}s:\n"
            + stdout
            + stderr
        ) from error
    completed = subprocess.CompletedProcess(
        list(command), process.returncode, stdout, stderr
    )
    # A successful leader exit is not evidence that qindaqt-session, shell,
    # locker, or bus grandchildren have left the exact disposable group.
    _terminate_private_process_group(process, process_group_id)
    return completed


def service_pid(busctl: str, service: str) -> int | None:
    """Return the private bus daemon's owner PID for one well-known name."""
    completed = subprocess.run(
        [busctl, "--user", "--no-pager", "status", service],
        text=True,
        capture_output=True,
        timeout=2,
        check=False,
    )
    if completed.returncode != 0:
        return None
    match = re.search(r"(?:^|\s)PID=(\d+)(?:\s|$)", completed.stdout)
    return int(match.group(1)) if match else None


def await_service_pid(
    busctl: str,
    service: str,
    *,
    expected: int | None = None,
    different_from: int | None = None,
    timeout: float = 12.0,
) -> int:
    """Wait for one exact owner PID rather than trusting process launch success."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        process_id = service_pid(busctl, service)
        if (
            process_id is not None
            and (expected is None or process_id == expected)
            and (different_from is None or process_id != different_from)
        ):
            return process_id
        time.sleep(0.05)
    raise RuntimeError(
        f"private service {service} did not reach expected PID state; "
        f"last={service_pid(busctl, service)} expected={expected} "
        f"different={different_from}"
    )


def terminate_authenticated_private_service(
    busctl: str, service: str, authenticated_process_id: int
) -> None:
    """Re-authenticate a private bus owner immediately before terminating it."""
    current_process_id = service_pid(busctl, service)
    if current_process_id != authenticated_process_id:
        raise RuntimeError(
            f"{service} owner changed before termination: "
            f"authenticated={authenticated_process_id} current={current_process_id}"
        )
    validate_private_session_process(current_process_id)
    try:
        os.kill(current_process_id, signal.SIGTERM)
    except OSError as error:
        raise RuntimeError(
            f"could not terminate exact private {service} PID "
            f"{current_process_id}: {error}"
        ) from error


def terminate(process: subprocess.Popen[str] | None) -> None:
    """Stop only the exact subprocess object created by the private harness."""
    if process is None or process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)
