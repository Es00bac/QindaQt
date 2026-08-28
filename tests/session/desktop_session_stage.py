# SPDX-License-Identifier: GPL-3.0-or-later
"""Create and resolve the exact staged production desktop artifacts."""

from __future__ import annotations

import configparser
import hashlib
import json
import os
import shutil
import stat
import subprocess
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Mapping, Sequence


class StageContractError(RuntimeError):
    """A stage path, artifact, or service descriptor is unsafe or incomplete."""


PRODUCTION_EXECUTABLES: Mapping[str, str] = {
    "launcher": "qindaqt-wm",
    "session": "qindaqt-session",
    "notification": "qindaqt-notification-host",
    "shell": "qindaqt-shell",
    "settings-service": "qindaqt-settings-service",
    "audio-service": "qindaqt-audio-service",
    "settings-app": "qindaqt-settings",
    "editor-app": "qindaqt-editor",
}

SERVICE_DESCRIPTORS: Mapping[str, str] = {
    "org.qindaqt.Settings1": "org.qindaqt.Settings1.service",
    "org.qindaqt.Audio1": "org.qindaqt.Audio1.service",
}


@dataclass(frozen=True)
class ResolvedService:
    name: str
    descriptor: Path
    activation_exec: str
    executable: Path


@dataclass(frozen=True)
class ResolvedStage:
    root: Path
    executables: Mapping[str, Path]
    compositor_plugin: Path
    decoration_plugin: Path
    services: Mapping[str, ResolvedService]

    def document(self) -> dict[str, object]:
        return {
            "root": str(self.root),
            "executables": {
                role: str(path) for role, path in sorted(self.executables.items())
            },
            "compositorPlugin": str(self.compositor_plugin),
            "decorationPlugin": str(self.decoration_plugin),
            "services": {
                name: {
                    "descriptor": str(service.descriptor),
                    "descriptorExec": service.activation_exec,
                    "directExecutable": str(service.executable),
                }
                for name, service in sorted(self.services.items())
            },
        }


def _relative_path(value: str, label: str) -> Path:
    path = PurePosixPath(value)
    if not value or path.is_absolute() or ".." in path.parts or "." in path.parts:
        raise StageContractError(f"{label} must be a normalized relative path: {value!r}")
    return Path(*path.parts)


def _resolved_file(root: Path, relative: Path, label: str, *, executable: bool) -> Path:
    root = root.resolve(strict=True)
    candidate = root / relative
    try:
        info = candidate.lstat()
    except OSError as error:
        raise StageContractError(f"stage omitted {label}: {candidate}: {error}") from error
    if stat.S_ISLNK(info.st_mode):
        raise StageContractError(f"staged {label} must not be a symlink: {candidate}")
    try:
        resolved = candidate.resolve(strict=True)
    except OSError as error:
        raise StageContractError(f"could not resolve staged {label}: {error}") from error
    if root not in resolved.parents or not resolved.is_file():
        raise StageContractError(f"staged {label} escapes the stage or is not a file")
    if executable and not os.access(resolved, os.X_OK):
        raise StageContractError(f"staged {label} is not executable: {resolved}")
    return resolved


def _read_service_descriptor(path: Path, expected_name: str) -> str:
    parser = configparser.ConfigParser(interpolation=None, strict=True)
    parser.optionxform = str
    try:
        with path.open(encoding="utf-8") as stream:
            parser.read_file(stream)
        name = parser.get("D-BUS Service", "Name")
        activation_exec = parser.get("D-BUS Service", "Exec")
    except (OSError, configparser.Error) as error:
        raise StageContractError(f"invalid service descriptor {path}: {error}") from error
    if name != expected_name or not activation_exec.strip():
        raise StageContractError(f"service descriptor {path} has the wrong identity")
    return activation_exec


def resolve_stage(
    stage_root: Path,
    *,
    bin_directory: str,
    plugin_relative: str,
    decoration_relative: str,
    settings_service_directory: str,
    audio_service_directory: str,
) -> ResolvedStage:
    """Resolve exact artifacts without consulting ``PATH`` or descriptor Exec.

    Settings1 and Audio1 descriptors currently contain configure-time absolute
    paths. They are inspected for package identity only; runtime starts the
    resolved staged executables directly and authenticates their owner PIDs.
    """

    root = stage_root.resolve(strict=True)
    bin_dir = _relative_path(bin_directory, "binary directory")
    plugin = _resolved_file(
        root,
        _relative_path(plugin_relative, "compositor plugin"),
        "compositor plugin",
        executable=False,
    )
    decoration = _resolved_file(
        root,
        _relative_path(decoration_relative, "decoration plugin"),
        "decoration plugin",
        executable=False,
    )
    executables = {
        role: _resolved_file(
            root, bin_dir / basename, f"{role} executable", executable=True
        )
        for role, basename in PRODUCTION_EXECUTABLES.items()
    }
    services: dict[str, ResolvedService] = {}
    role_by_service = {
        "org.qindaqt.Settings1": "settings-service",
        "org.qindaqt.Audio1": "audio-service",
    }
    directories = {
        "org.qindaqt.Settings1": _relative_path(
            settings_service_directory, "Settings1 service directory"
        ),
        "org.qindaqt.Audio1": _relative_path(
            audio_service_directory, "Audio1 service directory"
        ),
    }
    for name, filename in SERVICE_DESCRIPTORS.items():
        descriptor = _resolved_file(
            root, directories[name] / filename, f"{name} descriptor", executable=False
        )
        services[name] = ResolvedService(
            name=name,
            descriptor=descriptor,
            activation_exec=_read_service_descriptor(descriptor, name),
            executable=executables[role_by_service[name]],
        )
    return ResolvedStage(root, executables, plugin, decoration, services)


def _sentinel_text(build_root: Path) -> str:
    digest = hashlib.sha256(str(build_root.resolve()).encode()).hexdigest()
    return f"qindaqt-desktop-stage-v1\n{digest}\n"


def reset_stage_root(stage_root: Path, build_root: Path) -> Path:
    """Reset only an authenticated stage strictly beneath one build root."""

    build = build_root.resolve(strict=True)
    stage = stage_root.absolute()
    if stage == build or build not in stage.parents:
        raise StageContractError("stage root must be a strict child of the build root")
    sentinel = stage / ".qindaqt-desktop-stage"
    if stage.exists():
        if not stage.is_dir() or sentinel.is_symlink():
            raise StageContractError("existing stage root is not an authenticated directory")
        try:
            actual = sentinel.read_text(encoding="ascii")
        except OSError as error:
            raise StageContractError("refusing to replace a stage without its sentinel") from error
        if actual != _sentinel_text(build):
            raise StageContractError("stage sentinel belongs to a different build root")
        shutil.rmtree(stage)
    stage.mkdir(parents=True, mode=0o700)
    sentinel.write_text(_sentinel_text(build), encoding="ascii")
    return stage


def install_stage(
    cmake: Path,
    build_root: Path,
    stage_root: Path,
    *,
    configuration: str = "",
    component: str = "",
) -> Path:
    """Run one bounded CMake install into an authenticated build-local prefix."""

    executable = cmake.resolve(strict=True)
    if not executable.is_file() or not os.access(executable, os.X_OK):
        raise StageContractError("cmake executable is unavailable")
    stage = reset_stage_root(stage_root, build_root)
    command = [str(executable), "--install", str(build_root.resolve()), "--prefix", str(stage)]
    if configuration:
        command.extend(["--config", configuration])
    if component:
        command.extend(["--component", component])
    completed = subprocess.run(
        command, text=True, capture_output=True, check=False, timeout=120
    )
    if completed.returncode != 0:
        raise StageContractError(
            "staged install failed:\n" + completed.stdout + completed.stderr
        )
    return stage


def write_stage_evidence(path: Path, stage: ResolvedStage) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(stage.document(), sort_keys=True, indent=2) + "\n")
