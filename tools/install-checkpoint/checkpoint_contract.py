#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Shared paths and fixed contracts for the install checkpoint."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / "build" / "install-checkpoint"
BUILD = BASE / "cmake"
STAGE_ROOT = BASE / "stage"
INSTALL_PREFIX = Path("/usr")
MANIFEST = BASE / "stage-manifest.json"
SNAPSHOT = BASE / "pre-live-state.tar"
SNAPSHOT_SCRIPT = BASE / "capture-live-state.sh"
ROLLBACK_SCRIPT = BASE / "rollback-live-install.sh"
ROLLBACK_CONFIRMATION = "RESTORE-QINDAQT-DESKTOP"
SNAPSHOT_CONFIRMATION = "COPY-QINDAQT-USER-STATE"

SERVICES = {
    "Audio1": {
        "unit": "qindaqt-audio-service.service",
        "bus_name": "org.qindaqt.Audio1",
        "object_path": "/org/qindaqt/Audio1",
        "interface": "org.qindaqt.Audio1",
        "binary": "qindaqt-audio-service",
        "source": "src/services/audio_service",
        "version_kind": "audio",
    },
    "Network1": {
        "unit": "qindaqt-network-service.service",
        "bus_name": "org.qindaqt.Network1",
        "object_path": "/org/qindaqt/Network1",
        "interface": "org.qindaqt.Network1",
        "binary": "qindaqt-network-service",
        "source": "src/services/network_manager_adapter",
        "version_kind": "network",
    },
}

EXPECTED_BINARIES = (
    "qindaqt-settings",
    "qindaqt-settings-service",
    "qindaqt-audio-service",
    "qindaqt-network-service",
    "qindaqt-wm",
    "qindaqt-session",
    "qindaqt-shell",
)
SESSION_EXEC = "/usr/bin/qindaqt-wm --drm"
SESSION_TRY_EXEC = "/usr/bin/qindaqt-wm"

# Match the desktop ebuild's production composition. The stage proves the
# installable runtime build; focused checkpoint tests run separately and are
# not part of the generated install tree.
CMAKE_OPTIONS = (
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_INSTALL_PREFIX=/usr",
    "-DBUILD_TESTING=OFF",
    "-DQINDAQT_BUILD_KWIN_PLUGIN=ON",
    "-DQINDAQT_BUILD_SHELL=ON",
    "-DQINDAQT_BUILD_PRODUCTION_SHELL=ON",
    "-DQINDAQT_BUILD_VIEWER=ON",
    "-DQINDAQT_BUILD_SYSTEM_MONITOR=OFF",
    "-DQINDAQT_BUILD_OBS_BRIDGE=OFF",
    "-DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF",
    "-DQINDAQT_ENABLE_STRICT_WARNINGS=OFF",
)
