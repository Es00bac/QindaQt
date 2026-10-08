#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Narrow owned-fixture wrapper; injected controls cannot establish kernel proof."""
import os
import signal
import subprocess
import sys
import time
from dataclasses import dataclass, field
from types import SimpleNamespace

@dataclass
class Outcome:
    code: int = 124
    primary: str = ""
    cleanup: list[str] = field(default_factory=list)

def execute(command, ports, cancelled):
    """Borrow narrow ports for one call; retain primary and settlement failures."""
    result = Outcome()
    child = None
    reserved = True
    def error(stage, exc):
        result.cleanup.append(f"{stage}: {type(exc).__name__}: {exc}")
    def admit(stage):
        nonlocal reserved
        if not reserved:
            error(stage, RuntimeError("leader reservation already lost"))
            return False
        try:
            ports.observe(child.pid)  # None means still live; status is unreaped.
            return True
        except Exception as exc:
            reserved = False
            error(stage, exc)
            return False
    try:
        # The caller has installed nonraising cancellation before acquisition.
        child = ports.acquire(command)
        deadline = ports.clock() + 50
        while not cancelled():
            status = ports.observe(child.pid)
            if status is not None:
                result.code = status.si_status if status.si_code == os.CLD_EXITED else 128 + status.si_status
                break
            if ports.clock() >= deadline:
                result.primary = "timeout"
                break
            ports.sleep(0.05)
        if cancelled():
            result.code = 130
            result.primary = "cancelled"
    except Exception as exc:
        result.primary = f"{type(exc).__name__}: {exc}"
        # A failed wait may mean auto-reap. Fresh admission alone can authorize
        # cleanup; ChildProcessError permanently removes numeric signal authority.
        if isinstance(exc, ChildProcessError):
            reserved = False
    finally:
        if child is not None:
            for name, number in (("TERM", signal.SIGTERM), ("KILL", signal.SIGKILL)):
                # AGENT-GUARD: never signal a cached numeric group after loss of
                # its unreaped leader; independently re-admit before EACH signal.
                if admit(name + " admission"):
                    try:
                        ports.send(child.pid, number)
                    except ProcessLookupError:
                        pass
                    except Exception as exc:
                        error(name, exc)
                if name == "TERM":
                    try:
                        ports.sleep(0.1)
                    except Exception as exc:
                        error("cleanup delay", exc)
            try:
                child.wait(timeout=2)
            except Exception as exc:
                error("reap", exc)
    if cancelled() and not result.primary:
        result.primary = "cancelled"
        result.code = 130
    if result.cleanup and result.code == 0:
        result.code = 125
    return result

def main():
    if len(sys.argv) < 2:
        return 2
    cancellation = [False]
    def cancel(_number, _frame):
        cancellation[0] = True
    previous = {}
    try:
        for number in (signal.SIGTERM, signal.SIGINT):
            previous[number] = signal.signal(number, cancel)
        ports = SimpleNamespace(
            acquire=lambda argv: subprocess.Popen(argv, start_new_session=True),
            observe=lambda pid: os.waitid(os.P_PID, pid, os.WEXITED | os.WNOHANG | os.WNOWAIT),
            send=os.killpg, clock=time.monotonic, sleep=time.sleep)
        result = execute(sys.argv[1:], ports, lambda: cancellation[0])
    finally:
        for number, handler in previous.items():
            signal.signal(number, handler)
    if result.primary:
        print("primary: " + result.primary, file=sys.stderr)
    for error in result.cleanup:
        print("cleanup: " + error, file=sys.stderr)
    # Leader exit/group signals are not evidence that descendants are absent.
    return result.code

if __name__ == "__main__":
    raise SystemExit(main())
