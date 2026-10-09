# SPDX-License-Identifier: GPL-3.0-or-later
"""Linux child groups with a reserved leader until bounded cleanup and reaping."""
from __future__ import annotations
import hashlib
import os
from pathlib import Path
import signal
import subprocess
import time
from collections.abc import Callable


class ProcessCancelled(RuntimeError):
    """The caller's cooperative request denied this attempt, not its cleanup."""


def require_active(cancellation: Callable[[], bool] | None) -> None:
    if cancellation is not None and cancellation():
        raise ProcessCancelled("owned phase cancelled or its attempt deadline expired")


def observe_exit(process: subprocess.Popen, deadline: float, *,
                 cancellation: Callable[[], bool] | None = None) -> bool:
    while time.monotonic() < deadline:
        require_active(cancellation)
        # WNOWAIT leaves the leader unreaped. Its PID/group cannot be reused by
        # an unrelated session between observed command exit and group cleanup.
        observed = os.waitid(os.P_PID, process.pid,
                             os.WEXITED | os.WNOHANG | os.WNOWAIT)
        if observed is not None: return True
        time.sleep(0.05)
    return False


def _preserve_errors(primary: BaseException | None, errors: list[BaseException]):
    if primary is None and errors:
        primary = errors.pop(0)
    if primary is not None:
        primary.cleanup_errors = tuple(errors)
        for error in errors:
            primary.add_note("Owned cleanup error: " + type(error).__name__ + ": " + str(error))
        raise primary


def run_logged(argv: list[str], environment: dict[str, str], directory: Path,
               evidence: Path, name: str, timeout: int, *,
               cancellation: Callable[[], bool] | None = None) -> dict:
    """Own acquisition through group retirement; cancellation is borrowed.

    AGENT-CONTRACT: an optional same-thread, non-raising/non-reentrant callback
    outlives this call and never waits/reaps/signals our process. Signal handlers
    may only latch its state; asynchronously raising handlers remain unsupported.
    None retains the original phase timeout, return fields and normal behavior.
    Every cooperative refusal unwinds through owned cleanup. A lost exclusive
    reservation denies signaling; the caller must not claim settled descendants.
    """
    if not hasattr(os, "WNOWAIT"):
        raise RuntimeError("Linux unreaped-group lifetime support is required")
    log = evidence / (name + ".log")
    timed_out = False
    process = None; leader_reserved = False; primary = None; cleanup_errors = []
    output = log.open("wb")
    try:
        try:
            require_active(cancellation)
            process = subprocess.Popen(argv, cwd=directory, env=environment,
                                       stdout=output, stderr=subprocess.STDOUT,
                                       start_new_session=True, close_fds=True)
            leader_reserved = True
            require_active(cancellation)
            if not observe_exit(process, time.monotonic() + timeout,
                                cancellation=cancellation):
                timed_out = True
                os.killpg(process.pid, signal.SIGTERM)
                # Cancellation cannot interrupt the grace/retirement path.
                observe_exit(process, time.monotonic() + 5)
        except ChildProcessError as error:
            leader_reserved = False
            primary = RuntimeError("owned process leader was unexpectedly reaped")
            primary.reservation_error = error
        except BaseException as error:
            primary = error
        finally:
            if process is not None and leader_reserved:
                # Recheck our waitable child before sending a group signal.
                # No caller may reap it; WNOWAIT keeps the identity reserved.
                try:
                    os.waitid(os.P_PID, process.pid,
                              os.WEXITED | os.WNOHANG | os.WNOWAIT)
                except ChildProcessError as error:
                    leader_reserved = False
                    cleanup_errors.append(error)
                except BaseException as error:
                    cleanup_errors.append(error)
                    leader_reserved = False
                if leader_reserved:
                    try: os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError: pass
                    except BaseException as error: cleanup_errors.append(error)
                    try: code = process.wait(timeout=5)
                    except BaseException as error: cleanup_errors.append(error)
    finally:
        try: output.close()
        except BaseException as error: cleanup_errors.append(error)
    _preserve_errors(primary, cleanup_errors)
    checksum = hashlib.sha256(log.read_bytes()).hexdigest()
    # Requests during exit, cleanup, log close/hash cannot report a successful
    # phase or dispatch a subsequent phase; child retirement has finished.
    require_active(cancellation)
    return {"name": name, "argv": argv, "exit": code, "timedOut": timed_out,
            "ownedProcessGroup": process.pid, "leaderReservedThroughCleanup": True,
            "log": str(log), "sha256": checksum}
