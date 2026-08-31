# SPDX-License-Identifier: GPL-3.0-or-later
"""Exact installed-QML closure required by the contained Settings process."""

from __future__ import annotations

import stat
from pathlib import Path, PurePosixPath


class PackagePayloadError(RuntimeError):
    """The staged first-party Settings import closure is incomplete or unsafe."""


NETWORK_QML_FILES = (
    "qml/NetworkAccessPointSection.qml",
    "qml/NetworkDeviceSection.qml",
    "qml/NetworkPage.qml",
    "qml/NetworkRadioSection.qml",
    "qml/NetworkSavedSection.qml",
)


def _relative_path(value: str, label: str) -> Path:
    path = PurePosixPath(value)
    if not value or path.is_absolute() or "." in path.parts or ".." in path.parts:
        raise PackagePayloadError(f"{label} must be a normalized relative path")
    return Path(*path.parts)


def _regular_file(root: Path, relative: Path, label: str) -> Path:
    candidate = root / relative
    try:
        info = candidate.lstat()
    except OSError as error:
        raise PackagePayloadError(f"Network package omitted {label}") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise PackagePayloadError(f"Network package {label} is not a regular file")
    resolved = candidate.resolve(strict=True)
    if root not in resolved.parents:
        raise PackagePayloadError(f"Network package {label} escapes its module")
    return resolved


def authenticate_network_qml_package(
    stage_root: Path,
    *,
    qml_directory: str,
    library_name: str,
    plugin_name: str,
) -> tuple[Path, ...]:
    """Authenticate every artifact imported by QindaQt.SettingsApp.Network."""

    stage = stage_root.resolve(strict=True)
    module = (
        stage
        / _relative_path(qml_directory, "QML directory")
        / "QindaQt/SettingsApp/Network"
    )
    try:
        info = module.lstat()
    except OSError as error:
        raise PackagePayloadError("Network QML module directory is missing") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISDIR(info.st_mode):
        raise PackagePayloadError("Network QML module is not a regular directory")
    module = module.resolve(strict=True)
    if stage not in module.parents:
        raise PackagePayloadError("Network QML module escapes the stage")

    artifacts = (
        _relative_path(library_name, "Network QML library name"),
        _relative_path(plugin_name, "Network QML plugin name"),
        Path("qmldir"),
        Path("qindaqt_settings_network.qmltypes"),
        *(Path(value) for value in NETWORK_QML_FILES),
    )
    if any(len(path.parts) != 1 for path in artifacts[:4]):
        raise PackagePayloadError("Network QML binaries and metadata must be basenames")
    if len(set(artifacts)) != len(artifacts):
        raise PackagePayloadError("Network QML artifact names must be unique")
    return tuple(
        _regular_file(module, relative, str(relative)) for relative in artifacts
    )
