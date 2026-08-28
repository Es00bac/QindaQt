# SPDX-License-Identifier: GPL-3.0-or-later
"""Construct the fail-closed bubblewrap boundary for a virtual desktop row."""

from __future__ import annotations

import fcntl
import hashlib
import json
import os
import re
import shutil
import stat
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Mapping, Sequence


class SandboxContractError(RuntimeError):
    """A requested sandbox path or lifecycle operation is unsafe."""


RUN_ID_PATTERN = re.compile(r"^[a-f0-9]{32}$")
FORBIDDEN_ENVIRONMENT = frozenset(
    {
        "DISPLAY",
        "WAYLAND_DISPLAY",
        "WAYLAND_SOCKET",
        "XAUTHORITY",
        "DBUS_STARTER_ADDRESS",
        "DBUS_STARTER_BUS_TYPE",
        "PIPEWIRE_REMOTE",
        "PULSE_SERVER",
        "QINDAQT_DOTOOL",
        "QINDAQT_ALLOW_HOST_UINPUT",
    }
)


@dataclass(frozen=True)
class SandboxPaths:
    root: Path
    runtime: Path
    artifacts: Path
    logs: Path
    private_home: Path
    private_config: Path
    private_data: Path
    private_cache: Path
    private_state: Path
    machine_id: Path
    sentinel: Path


@dataclass(frozen=True)
class ReadOnlyMount:
    source: Path
    destination: PurePosixPath


@dataclass(frozen=True)
class SandboxSpec:
    bwrap: Path
    run_id: str
    uid: int
    paths: SandboxPaths
    stage: ReadOnlyMount
    tests: ReadOnlyMount
    probe: ReadOnlyMount
    system_mounts: tuple[ReadOnlyMount, ...]
    environment: Mapping[str, str]
    command: tuple[str, ...]


def _run_sentinel(run_id: str, build_root: Path) -> str:
    digest = hashlib.sha256(str(build_root.resolve()).encode()).hexdigest()
    return f"qindaqt-desktop-run-v1\n{run_id}\n{digest}\n"


def _assert_run_id(run_id: str) -> None:
    if RUN_ID_PATTERN.fullmatch(run_id) is None:
        raise SandboxContractError("run id must be exactly 32 lowercase hexadecimal digits")


def create_run_root(build_root: Path, run_id: str) -> SandboxPaths:
    """Create one authenticated run root beneath the caller's build tree."""

    _assert_run_id(run_id)
    build = build_root.resolve(strict=True)
    parent = build / "tests" / "session" / "desktop-session-runs"
    parent.mkdir(parents=True, exist_ok=True)
    root = parent / run_id
    if root.exists() or root.is_symlink():
        raise SandboxContractError(f"run root already exists: {root}")
    root.mkdir(mode=0o700)
    sentinel = root / ".qindaqt-desktop-run"
    sentinel.write_text(_run_sentinel(run_id, build), encoding="ascii")
    directories = {
        "runtime": root / "runtime",
        "artifacts": root / "artifacts",
        "logs": root / "logs",
        "private_home": root / "home",
        "private_config": root / "config",
        "private_data": root / "data",
        "private_cache": root / "cache",
        "private_state": root / "state",
    }
    for directory in directories.values():
        directory.mkdir(mode=0o700)
    machine_id = root / "machine-id"
    machine_id.write_text(run_id + "\n", encoding="ascii")
    return SandboxPaths(root=root, machine_id=machine_id, sentinel=sentinel, **directories)


def remove_run_root(paths: SandboxPaths, build_root: Path, run_id: str) -> None:
    """Remove only the exact sentinel-authenticated run root."""

    _assert_run_id(run_id)
    build = build_root.resolve(strict=True)
    root = paths.root.absolute()
    expected_parent = build / "tests" / "session" / "desktop-session-runs"
    if root.parent != expected_parent or root.name != run_id:
        raise SandboxContractError("run root is outside the exact build-local namespace")
    if root.is_symlink() or paths.sentinel.is_symlink():
        raise SandboxContractError("run root or sentinel became a symlink")
    try:
        actual = paths.sentinel.read_text(encoding="ascii")
    except OSError as error:
        raise SandboxContractError("run root sentinel is missing") from error
    if actual != _run_sentinel(run_id, build):
        raise SandboxContractError("run root sentinel does not match this run")
    shutil.rmtree(root)


def _validate_mount(mount: ReadOnlyMount, label: str) -> None:
    try:
        source = mount.source.resolve(strict=True)
    except OSError as error:
        raise SandboxContractError(f"{label} source is unavailable: {error}") from error
    if not source.is_file() and not source.is_dir():
        raise SandboxContractError(f"{label} source must be a file or directory")
    destination = mount.destination
    if not destination.is_absolute() or ".." in destination.parts:
        raise SandboxContractError(f"{label} destination must be normalized and absolute")
    if str(destination) in {"/", "/run", "/tmp", "/dev", "/home"}:
        raise SandboxContractError(f"{label} destination is too broad")


def sandbox_environment(
    *, run_id: str, uid: int, stage_bin: str, system_path: Sequence[str]
) -> dict[str, str]:
    """Return a complete environment, never a mutation of the host environment."""

    _assert_run_id(run_id)
    runtime = f"/run/user/{uid}"
    path_entries = [stage_bin, *system_path]
    if any(not entry.startswith("/") or ":" in entry for entry in path_entries):
        raise SandboxContractError("PATH entries must be absolute sandbox paths")
    environment = {
        "HOME": "/home/qindaqt",
        "XDG_CONFIG_HOME": "/home/qindaqt/.config",
        "XDG_DATA_HOME": "/home/qindaqt/.local/share",
        "XDG_CACHE_HOME": "/home/qindaqt/.cache",
        "XDG_STATE_HOME": "/home/qindaqt/.local/state",
        "XDG_RUNTIME_DIR": runtime,
        "XDG_DATA_DIRS": "/opt/qindaqt/share:/usr/share",
        "XDG_CURRENT_DESKTOP": "QindaQt",
        "XDG_SESSION_DESKTOP": "qindaqt",
        "XDG_SESSION_TYPE": "wayland",
        "DBUS_SESSION_BUS_ADDRESS": f"unix:path={runtime}/bus",
        "PATH": ":".join(path_entries),
        "LANG": "C.UTF-8",
        "LC_ALL": "C.UTF-8",
        "TZ": "UTC",
        "KWIN_COMPOSE": "Q",
        "QT_QPA_PLATFORM": "wayland",
        "QT_QUICK_BACKEND": "software",
        "QINDAQT_SESSION_RUN_ID": run_id,
    }
    overlap = FORBIDDEN_ENVIRONMENT.intersection(environment)
    if overlap:
        raise SandboxContractError(f"sandbox environment admitted forbidden keys: {overlap}")
    return environment


def build_bwrap_argv(spec: SandboxSpec) -> list[str]:
    """Translate a typed spec into deterministic, structurally isolated argv."""

    _assert_run_id(spec.run_id)
    try:
        bwrap = spec.bwrap.resolve(strict=True)
    except OSError as error:
        raise SandboxContractError(f"bubblewrap is unavailable: {error}") from error
    if not bwrap.is_file() or not os.access(bwrap, os.X_OK):
        raise SandboxContractError("bubblewrap path is not executable")
    for label, mount in (
        ("stage", spec.stage),
        ("tests", spec.tests),
        ("probe", spec.probe),
    ):
        _validate_mount(mount, label)
    for index, mount in enumerate(spec.system_mounts):
        _validate_mount(mount, f"system mount {index}")
    if not spec.command or not spec.command[0].startswith("/"):
        raise SandboxContractError("sandbox command must be a nonempty absolute argv")
    if FORBIDDEN_ENVIRONMENT.intersection(spec.environment):
        raise SandboxContractError("sandbox environment contains a host endpoint key")
    runtime_destination = f"/run/user/{spec.uid}"
    argv = [
        str(bwrap),
        "--unshare-user",
        "--unshare-pid",
        "--unshare-net",
        "--unshare-ipc",
        "--unshare-uts",
        "--die-with-parent",
        "--new-session",
        "--clearenv",
        "--tmpfs",
        "/",
        "--proc",
        "/proc",
        "--dev",
        "/dev",
        "--tmpfs",
        "/tmp",
        "--dir",
        "/run",
        "--dir",
        "/run/user",
        "--dir",
        runtime_destination,
        "--bind",
        str(spec.paths.runtime.resolve()),
        runtime_destination,
        "--dir",
        "/home",
        "--dir",
        "/home/qindaqt",
        "--dir",
        "/home/qindaqt/.config",
        "--dir",
        "/home/qindaqt/.local",
        "--dir",
        "/home/qindaqt/.local/share",
        "--dir",
        "/home/qindaqt/.local/state",
        "--dir",
        "/home/qindaqt/.cache",
        "--dir",
        "/etc",
        "--ro-bind",
        str(spec.paths.machine_id.resolve()),
        "/etc/machine-id",
        "--dir",
        "/var",
        "--dir",
        "/var/lib",
        "--bind",
        str(spec.paths.artifacts.resolve()),
        "/var/lib/qindaqt-evidence",
        "--dir",
        "/var/log",
        "--bind",
        str(spec.paths.logs.resolve()),
        "/var/log/qindaqt-desktop",
    ]
    for mount in (*spec.system_mounts, spec.stage, spec.tests, spec.probe):
        argv.extend(
            ["--ro-bind", str(mount.source.resolve()), str(mount.destination)]
        )
    for key, value in sorted(spec.environment.items()):
        argv.extend(["--setenv", key, value])
    argv.extend(["--", *spec.command])
    return argv


def write_command_evidence(path: Path, spec: SandboxSpec) -> None:
    document = {
        "schemaVersion": 1,
        "runId": spec.run_id,
        "argv": build_bwrap_argv(spec),
        "environment": dict(sorted(spec.environment.items())),
        "forbiddenEnvironment": sorted(FORBIDDEN_ENVIRONMENT),
        "hostInputNodesBound": False,
        "hostRuntimeBound": False,
        "renderNodeBound": False,
    }
    path.write_text(json.dumps(document, sort_keys=True, indent=2) + "\n")


class PrivateLaneLock:
    """Per-user cross-worktree advisory lock; manager allocation remains required."""

    def __init__(self, path: Path | None = None) -> None:
        self._path = path or Path(f"/tmp/qindaqt-private-session-{os.getuid()}.lock")
        self._fd: int | None = None

    def __enter__(self) -> "PrivateLaneLock":
        flags = os.O_CREAT | os.O_RDWR | getattr(os, "O_CLOEXEC", 0)
        flags |= getattr(os, "O_NOFOLLOW", 0)
        fd = os.open(self._path, flags, 0o600)
        info = os.fstat(fd)
        if not stat.S_ISREG(info.st_mode) or info.st_uid != os.getuid():
            os.close(fd)
            raise SandboxContractError("private-lane lock is not a caller-owned regular file")
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            os.close(fd)
            raise SandboxContractError("the cross-worktree private-session lane is busy")
        self._fd = fd
        return self

    def __exit__(self, *_: object) -> None:
        if self._fd is not None:
            fcntl.flock(self._fd, fcntl.LOCK_UN)
            os.close(self._fd)
            self._fd = None
