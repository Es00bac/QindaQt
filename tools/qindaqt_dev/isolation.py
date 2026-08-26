# SPDX-License-Identifier: GPL-3.0-or-later
"""Create and supervise a disposable desktop-session environment."""

from __future__ import annotations

import os
import signal
import shutil
import subprocess
import tempfile
from contextlib import AbstractContextManager
from pathlib import Path
from types import TracebackType
from typing import Mapping

from .backends import BackendPlan


class IsolatedRuntime(AbstractContextManager["IsolatedRuntime"]):
    """Own XDG directories that TemporaryDirectory can remove without ambiguity."""

    def __init__(self) -> None:
        self._temporary = tempfile.TemporaryDirectory(prefix="qindaqt-dev-")
        self.root = Path(self._temporary.name).resolve()
        self.environment = self._build_environment()

    def _make_directory(self, name: str, *, mode: int = 0o755) -> Path:
        directory = self.root / name
        directory.mkdir(mode=mode)
        os.chmod(directory, mode)
        return directory

    def _build_environment(self) -> dict[str, str]:
        environment = dict(os.environ)
        # AGENT-GUARD: Inherited session buses and desktop markers could
        # mutate the developer's live session even when XDG storage is isolated.
        for key in (
            "DBUS_SESSION_BUS_ADDRESS",
            "DESKTOP_STARTUP_ID",
            "GNOME_DESKTOP_SESSION_ID",
            "KDE_FULL_SESSION",
            "SESSION_MANAGER",
            "XDG_SESSION_ID",
        ):
            environment.pop(key, None)
        home = self._make_directory("home")
        config = self._make_directory("config")
        data = self._make_directory("data")
        cache = self._make_directory("cache")
        state = self._make_directory("state")
        runtime = self._make_directory("runtime", mode=0o700)
        environment.update(
            {
                "HOME": str(home),
                "XDG_CONFIG_HOME": str(config),
                "XDG_DATA_HOME": str(data),
                "XDG_CACHE_HOME": str(cache),
                "XDG_STATE_HOME": str(state),
                "XDG_RUNTIME_DIR": str(runtime),
                "XDG_CURRENT_DESKTOP": "QindaQt",
                "XDG_SESSION_DESKTOP": "qindaqt",
                "XDG_SESSION_TYPE": "wayland",
                "QINDAQT_DEV_SESSION": "1",
            }
        )
        return environment

    def __exit__(
        self,
        exception_type: type[BaseException] | None,
        exception: BaseException | None,
        traceback: TracebackType | None,
    ) -> None:
        self._temporary.cleanup()


def execute_plan(plan: BackendPlan, runtime: IsolatedRuntime, timeout: float | None) -> int:
    """Run a plan without a shell and return its foreground process exit status."""
    environment = dict(runtime.environment)
    environment.update(plan.environment)
    executable = shutil.which(plan.argv[0], path=environment.get("PATH"))
    if executable is None:
        raise FileNotFoundError(f"required executable was not found on PATH: {plan.argv[0]}")
    argv = (executable, *plan.argv[1:])
    dbus_runner = shutil.which("dbus-run-session", path=environment.get("PATH"))
    if dbus_runner is None:
        raise FileNotFoundError("dbus-run-session is required for an isolated executable session")
    argv = (dbus_runner, "--", *argv)
    process = subprocess.Popen(argv, env=environment, start_new_session=True)
    try:
        return process.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        _stop_process_group(process)
        return 124
    except KeyboardInterrupt:
        _stop_process_group(process)
        return 130


def _stop_process_group(process: subprocess.Popen[bytes]) -> None:
    """Stop every process owned by the private session before XDG cleanup."""
    if process.poll() is not None:
        return
    os.killpg(process.pid, signal.SIGTERM)
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait()


def environment_preview(plan: BackendPlan) -> Mapping[str, str]:
    """Describe isolation symbolically so dry-runs do not create temporary files."""
    values = {
        "HOME": "<temporary-xdg-root>/home",
        "XDG_CONFIG_HOME": "<temporary-xdg-root>/config",
        "XDG_DATA_HOME": "<temporary-xdg-root>/data",
        "XDG_CACHE_HOME": "<temporary-xdg-root>/cache",
        "XDG_STATE_HOME": "<temporary-xdg-root>/state",
        "XDG_RUNTIME_DIR": "<temporary-xdg-root>/runtime",
        "QINDAQT_DEV_SESSION": "1",
    }
    values.update(plan.environment)
    return values
