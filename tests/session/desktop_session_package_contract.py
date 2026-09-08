# SPDX-License-Identifier: GPL-3.0-or-later
"""Exact installed-QML closure required by the contained Settings process."""

from __future__ import annotations

import configparser
import stat
from pathlib import Path, PurePosixPath


class PackagePayloadError(RuntimeError):
    """The staged first-party Settings import closure is incomplete or unsafe."""


WINDOW_SWITCHER_FILES = (
    "metadata.json",
    "contents/ui/main.qml",
    "contents/ui/QindaQtAppearanceBridge.qml",
    "contents/ui/QindaQtSwitcherFrame.qml",
    "contents/ui/QindaQtSwitcherRow.qml",
)

NETWORK_QML_FILES = (
    "qml/NetworkAccessPointSection.qml",
    "qml/NetworkDeviceSection.qml",
    "qml/NetworkPage.qml",
    "qml/NetworkRadioSection.qml",
    "qml/NetworkSavedSection.qml",
)

FIRST_PARTY_DESKTOP_ICONS = {
    "org.qindaqt.Settings.desktop": "preferences-system",
    "org.qindaqt.TextEditor.desktop": "org.qindaqt.TextEditor",
    "org.qindaqt.Terminal.desktop": "org.qindaqt.Terminal",
    "org.qindaqt.FileManager.desktop": "org.qindaqt.FileManager",
    "org.qindaqt.Welcome.desktop": "help-about",
}


def _relative_path(value: str, label: str) -> Path:
    path = PurePosixPath(value)
    if not value or path.is_absolute() or "." in path.parts or ".." in path.parts:
        raise PackagePayloadError(f"{label} must be a normalized relative path")
    return Path(*path.parts)


def _regular_file(
    root: Path,
    relative: Path,
    label: str,
    *,
    package_name: str = "Network package",
) -> Path:
    candidate = root / relative
    try:
        info = candidate.lstat()
    except OSError as error:
        raise PackagePayloadError(f"{package_name} omitted {label}") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise PackagePayloadError(f"{package_name} {label} is not a regular file")
    resolved = candidate.resolve(strict=True)
    if root not in resolved.parents:
        raise PackagePayloadError(f"{package_name} {label} escapes its package")
    return resolved


def authenticate_window_switcher_package(stage_root: Path) -> tuple[Path, ...]:
    """Authenticate the exact production KWin TabBox payload in a stage."""

    stage = stage_root.resolve(strict=True)
    package = stage / "share/kwin/tabbox/qindaqt"
    try:
        info = package.lstat()
    except OSError as error:
        raise PackagePayloadError("Window switcher package directory is missing") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISDIR(info.st_mode):
        raise PackagePayloadError("Window switcher package is not a regular directory")
    package = package.resolve(strict=True)
    if stage not in package.parents:
        raise PackagePayloadError("Window switcher package escapes the stage")
    return tuple(
        _regular_file(
            package,
            Path(relative),
            relative,
            package_name="Window switcher package",
        )
        for relative in WINDOW_SWITCHER_FILES
    )


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


def authenticate_first_party_desktop_entries(stage_root: Path) -> tuple[Path, ...]:
    """Authenticate task-icon metadata staged for first-party window app IDs."""

    stage = stage_root.resolve(strict=True)
    applications = stage / "share/applications"
    resolved: list[Path] = []
    for filename, expected_icon in FIRST_PARTY_DESKTOP_ICONS.items():
        entry = _regular_file(
            stage, Path("share/applications") / filename,
            f"first-party desktop entry {filename}",
        )
        parser = configparser.ConfigParser(interpolation=None, strict=True)
        parser.optionxform = str
        try:
            with entry.open(encoding="utf-8") as stream:
                parser.read_file(stream)
            icon = parser.get("Desktop Entry", "Icon")
        except (OSError, configparser.Error) as error:
            raise PackagePayloadError(
                f"first-party desktop entry {filename} is malformed"
            ) from error
        if icon != expected_icon or entry.parent != applications.resolve(strict=True):
            raise PackagePayloadError(
                f"first-party desktop entry {filename} has the wrong icon identity"
            )
        resolved.append(entry)
    return tuple(resolved)
