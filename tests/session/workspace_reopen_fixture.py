# SPDX-License-Identifier: GPL-3.0-or-later
"""Fixture and evidence helpers for the saved-workspace two-session row.

The fixture desktop entries live only inside the disposable XDG data root of
the nested run. Two same-app windows exercise duplicate-slot explicit
assignment; the phantom entry is deleted between sessions so the fresh
compositor genuinely reports a missing application.
"""

from __future__ import annotations

import json
import os
from pathlib import Path
from typing import Any

TERMINAL_APP_ID = "org.qindaqt.wreopen-terminal"
PHANTOM_APP_ID = "org.qindaqt.wreopen-phantom"
EDITOR_APP_ID = "org.qindaqt.wreopen-editor"

FIXTURE_APPLICATIONS = {
    TERMINAL_APP_ID: "WReopen Terminal",
    PHANTOM_APP_ID: "WReopen Phantom",
    EDITOR_APP_ID: "WReopen Editor",
}

RESULT_MARKER = "QINDAQT_WORKSPACE_REOPEN="


def desktop_entry_text(app_id: str, name: str) -> str:
    """One minimal valid desktop entry; Exec is never dispatched by the row."""
    if not app_id or any(character in app_id for character in "[]=\n"):
        raise ValueError(f"invalid fixture desktop entry id: {app_id!r}")
    return (
        "[Desktop Entry]\n"
        "Type=Application\n"
        f"Name={name}\n"
        f"Exec=/bin/true # {app_id}\n"
        "NoDisplay=true\n"
        "Terminal=false\n"
    )


def applications_directory(xdg_data_home: str) -> Path:
    return Path(xdg_data_home) / "applications"


def write_fixture_applications(xdg_data_home: str) -> None:
    directory = applications_directory(xdg_data_home)
    directory.mkdir(parents=True, exist_ok=True)
    for app_id, name in FIXTURE_APPLICATIONS.items():
        path = directory / f"{app_id}.desktop"
        path.write_text(desktop_entry_text(app_id, name), encoding="utf-8")


def remove_fixture_application(xdg_data_home: str, app_id: str) -> Path:
    path = applications_directory(xdg_data_home) / f"{app_id}.desktop"
    if not path.is_file():
        raise RuntimeError(f"fixture desktop entry is missing: {path}")
    path.unlink()
    if path.exists():
        raise RuntimeError(f"fixture desktop entry survived removal: {path}")
    return path


def clear_ksycoca(xdg_cache_home: str) -> int:
    """Drop the private sycoca cache so session 2 re-scans applications."""
    removed = 0
    cache = Path(xdg_cache_home)
    if cache.is_dir():
        for candidate in cache.glob("ksycoca6*"):
            candidate.unlink()
            removed += 1
    return removed


def store_directory(xdg_data_home: str) -> Path:
    return Path(xdg_data_home) / "qindaqt" / "workspaces"


def load_workspace_documents(xdg_data_home: str) -> list[dict[str, Any]]:
    directory = store_directory(xdg_data_home)
    documents = []
    if directory.is_dir():
        for path in sorted(directory.glob("*.json")):
            documents.append(json.loads(path.read_text(encoding="utf-8")))
    return documents


def extract_reopen_result(stdout: str, phase: str) -> dict[str, Any]:
    marker = next(
        (
            line[len(RESULT_MARKER) :]
            for line in stdout.splitlines()
            if line.startswith(RESULT_MARKER)
        ),
        None,
    )
    if marker is None:
        raise RuntimeError(f"the {phase} phase printed no evidence marker")
    result = json.loads(marker)
    if not isinstance(result, dict) or result.get("phase") != phase:
        raise RuntimeError(f"the {phase} phase evidence is malformed")
    return result


def validate_save_evidence(result: dict[str, Any], document: dict[str, Any]) -> None:
    if (
        not result.get("workspaceId")
        or result["workspaceId"] != document.get("id")
        or not str(document.get("name", "")).endswith("cvn")
        or document.get("color") != "#30A46C"
        or len(document.get("applications", [])) != 3
    ):
        raise RuntimeError("session 1 did not save the named/color fixture workspace")
    entry_ids = sorted(
        slot.get("desktopEntryId") for slot in document.get("applications", [])
    )
    if entry_ids != [PHANTOM_APP_ID, TERMINAL_APP_ID, TERMINAL_APP_ID]:
        raise RuntimeError(f"saved workspace slots are not the fixture: {entry_ids}")


def validate_reopen_evidence(
    result: dict[str, Any], save_result: dict[str, Any], document: dict[str, Any]
) -> None:
    if result.get("workspaceId") != save_result.get("workspaceId"):
        raise RuntimeError("the reopened workspace identity changed across sessions")
    expected = {
        "layoutMatchesSavedDocument": True,
        "duplicateSlotsExplicitlyAssigned": True,
        "nameColorRoundTripThroughRestore": True,
        "missingApplicationReported": PHANTOM_APP_ID,
        "replacementApplication": EDITOR_APP_ID,
    }
    for key, value in expected.items():
        if result.get(key) != value:
            raise RuntimeError(f"reopen evidence mismatch for {key}: {result.get(key)!r}")
    if not result.get("containerId"):
        raise RuntimeError("the fresh session adopted no container")
    if document.get("id") != result["workspaceId"]:
        raise RuntimeError("the re-saved document changed identity")
    entry_ids = sorted(
        slot.get("desktopEntryId") for slot in document.get("applications", [])
    )
    if entry_ids != [EDITOR_APP_ID, TERMINAL_APP_ID, TERMINAL_APP_ID]:
        raise RuntimeError(
            "the re-saved document did not record the explicit replacement: "
            f"{entry_ids}"
        )


def nested_environment_variables(environment: dict[str, str]) -> dict[str, str]:
    forbidden = {"DISPLAY", "WAYLAND_DISPLAY"}
    leaked = forbidden & {
        key for key, value in environment.items() if value and os.environ.get(key) == value
    }
    if leaked:
        raise RuntimeError(f"isolated environment still inherits host display: {leaked}")
    return environment
