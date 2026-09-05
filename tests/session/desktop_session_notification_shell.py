# SPDX-License-Identifier: GPL-3.0-or-later
"""Authenticated notification-shell evidence for desktop readiness."""

from __future__ import annotations

import re
from typing import Any, Mapping


PENDING_WITHOUT_EVIDENCE = {
    "service-missing", "snapshot-object-pending", "snapshot-reply-pending",
    "task-list-not-ready",
}
PENDING_WITH_EVIDENCE = {
    "privacy-denied", "center-window-missing", "center-output-pending",
}


def _canonical_process_id(value: Any, location: str) -> int:
    process_id = (
        int(value, 10)
        if isinstance(value, str) and value.isascii() and value.isdecimal()
        else 0
    )
    if process_id <= 1 or value != str(process_id):
        raise RuntimeError(f"{location} is not a canonical positive PID")
    return process_id


def _canonical_counter(value: Any, location: str) -> int:
    counter = (
        int(value, 10)
        if isinstance(value, str) and value.isascii() and value.isdecimal()
        else -1
    )
    if counter < 0 or value != str(counter):
        raise RuntimeError(f"{location} is not a canonical counter")
    return counter


def _authority(dock_surfaces: Any, outputs: Any) -> tuple[int, str] | str:
    if not isinstance(outputs, list) or not outputs:
        raise RuntimeError("notification-shell evidence has no selected output")
    output_names: set[str] = set()
    for raw_output in outputs:
        if not isinstance(raw_output, Mapping):
            raise RuntimeError("notification-shell Outputs record is malformed")
        name = raw_output.get("name")
        if not isinstance(name, str) or not name or name in output_names:
            raise RuntimeError("notification-shell Outputs names are malformed")
        output_names.add(name)
    selected_output = outputs[0].get("name")

    if not isinstance(dock_surfaces, list) or not dock_surfaces:
        raise RuntimeError("notification-shell evidence has no dock owner")
    dock_pids: set[int] = set()
    dock_count = 0
    unsettled: str | None = None
    for raw_dock in dock_surfaces:
        if not isinstance(raw_dock, Mapping):
            raise RuntimeError("notification-shell dock record is malformed")
        if raw_dock.get("scope") != "dock":
            continue
        dock_count += 1
        process_id = _canonical_process_id(
            raw_dock.get("processId"), "dock process ID"
        )
        dock_pids.add(process_id)
        mapped = raw_dock.get("mapped")
        committed = raw_dock.get("committed")
        current = raw_dock.get("outputName")
        desired = raw_dock.get("desiredOutputName")
        geometry = raw_dock.get("geometry")
        if (
            not isinstance(mapped, bool)
            or not isinstance(committed, bool)
            or not isinstance(current, str)
            or not current
            or not isinstance(desired, str)
            or not desired
            or not isinstance(geometry, Mapping)
            or set(geometry) != {"x", "y", "width", "height"}
            or any(
                isinstance(geometry.get(key), bool)
                or not isinstance(geometry.get(key), int)
                for key in ("x", "y", "width", "height")
            )
        ):
            raise RuntimeError("notification-shell dock readiness is malformed")
        if current not in output_names or desired not in output_names:
            raise RuntimeError("notification-shell dock names a foreign output")
        if not mapped or not committed:
            unsettled = "notification-shell dock is not mapped and committed yet"
            continue
        if current != desired:
            unsettled = "notification-shell dock output has not converged yet"
            continue
        if geometry["width"] <= 0 or geometry["height"] <= 0:
            unsettled = "notification-shell dock geometry is not settled yet"
            continue
    if dock_count == 0:
        return "notification-shell dock owner is not available yet"
    if len(dock_pids) != 1:
        raise RuntimeError("notification-shell PID does not uniquely own every dock")
    if unsettled is not None:
        return unsettled
    return next(iter(dock_pids)), selected_output


def _validate_evidence(
    evidence: Any, *, shell_pid: int, selected_output: str,
    output_names: set[str], pending_code: str | None,
) -> None:
    if not isinstance(evidence, Mapping) or set(evidence) != {
        "owner", "servicePid", "shellPid", "tokens", "taskList", "presentation",
        "centerOpenedCount", "centerWindow",
    }:
        raise RuntimeError("notification-shell evidence has an unexpected field set")
    owner = evidence.get("owner")
    if not isinstance(owner, str) or not owner.startswith(":"):
        raise RuntimeError("notification-shell owner is not a unique bus name")
    service_pid = _canonical_process_id(
        evidence.get("servicePid"), "notification-shell service PID"
    )
    snapshot_pid = _canonical_process_id(
        evidence.get("shellPid"), "notification-shell snapshot PID"
    )
    if service_pid != shell_pid or snapshot_pid != shell_pid:
        raise RuntimeError("notification-shell PID does not own every dock")
    tokens = evidence.get("tokens")
    if (
        not isinstance(tokens, Mapping)
        or set(tokens) != {
            "ready", "qstRevision", "generation", "sourceThemeId",
            "backgroundBase",
        }
        or tokens.get("ready") is not True
        or tokens.get("qstRevision") != 1
        or _canonical_counter(
            tokens.get("generation"), "notification-shell token generation"
        ) <= 0
        or not isinstance(tokens.get("sourceThemeId"), str)
        or not tokens.get("sourceThemeId")
        or not isinstance(tokens.get("backgroundBase"), str)
        or re.fullmatch(r"#[0-9a-f]{6}", tokens["backgroundBase"]) is None
    ):
        raise RuntimeError("notification-shell tokens are not ready")
    task_list = evidence.get("taskList")
    if (
        not isinstance(task_list, Mapping)
        or set(task_list) != {"phase", "generation", "windowCount"}
        or task_list.get("phase") != "ready"
        or _canonical_counter(
            task_list.get("generation"), "notification-shell task-list generation"
        ) <= 0
        or not isinstance(task_list.get("windowCount"), int)
        or isinstance(task_list.get("windowCount"), bool)
        or task_list["windowCount"] < 1
    ):
        raise RuntimeError("notification-shell task list is not ready")
    _canonical_counter(
        evidence.get("centerOpenedCount"), "notification-shell opened count"
    )

    presentation = evidence.get("presentation")
    center = evidence.get("centerWindow")
    if (
        not isinstance(presentation, Mapping)
        or set(presentation) != {"privatePresentationAllowed", "centerOpen"}
        or not isinstance(presentation.get("privatePresentationAllowed"), bool)
        or not isinstance(presentation.get("centerOpen"), bool)
        or not isinstance(center, Mapping)
        or not isinstance(center.get("exists"), bool)
    ):
        raise RuntimeError("notification-shell presentation evidence is malformed")
    exists = center.get("exists")
    if exists:
        if (
            set(center) != {"exists", "visible", "outputName"}
            or not isinstance(center.get("visible"), bool)
            or not isinstance(center.get("outputName"), str)
            or center.get("outputName") not in output_names
        ):
            raise RuntimeError("notification-shell center window is malformed")
    elif set(center) != {"exists"}:
        raise RuntimeError("notification-shell absent center window is malformed")

    private = presentation.get("privatePresentationAllowed")
    center_open = presentation.get("centerOpen")
    if pending_code is None:
        if (
            private is not True
            or center_open is not False
            or exists is not True
            or center.get("visible") is not False
            or center.get("outputName") != selected_output
        ):
            raise RuntimeError("notification-shell closed presentation is malformed")
        return
    if center_open is not False:
        raise RuntimeError("pending notification-shell center is already open")
    if pending_code == "center-window-missing":
        valid = exists is False
    elif pending_code == "center-output-pending":
        valid = (
            exists is True
            and center.get("visible") is False
            and center.get("outputName") != selected_output
        )
    else:
        valid = (
            private is False
            and exists is True
            and center.get("visible") is False
            and center.get("outputName") == selected_output
        )
    if not valid:
        raise RuntimeError("pending notification-shell evidence disagrees with its code")


def notification_shell_pending(
    raw: Mapping[str, Any], *, dock_surfaces: Any, outputs: Any
) -> str | None:
    """Validate one authenticated closed/hidden shell presentation sample."""

    authority = _authority(dock_surfaces, outputs)
    if isinstance(authority, str):
        return authority
    shell_pid, selected_output = authority
    output_names = {str(output["name"]) for output in outputs}
    status = raw.get("status")
    if status == "pending":
        failure = raw.get("failure")
        if (
            not isinstance(failure, Mapping)
            or set(failure) != {"code", "message"}
            or not isinstance(failure.get("code"), str)
            or not isinstance(failure.get("message"), str)
        ):
            raise RuntimeError("pending notification-shell evidence was malformed")
        code = str(failure["code"])
        evidence = raw.get("evidence")
        if code in PENDING_WITHOUT_EVIDENCE:
            if set(raw) != {"status", "failure"}:
                raise RuntimeError("temporal notification-shell gap carried stale evidence")
        elif code in PENDING_WITH_EVIDENCE:
            if set(raw) != {"status", "failure", "evidence"}:
                raise RuntimeError("pending notification-shell shape is malformed")
            _validate_evidence(
                evidence, shell_pid=shell_pid, selected_output=selected_output,
                output_names=output_names, pending_code=code,
            )
        else:
            raise RuntimeError("notification-shell pending code is not retryable")
        return str(failure["message"])
    if status != "ok":
        raise RuntimeError("notification-shell evidence failed closed")
    if set(raw) != {"status", "evidence"}:
        raise RuntimeError("ready notification-shell shape is malformed")
    _validate_evidence(
        raw.get("evidence"), shell_pid=shell_pid,
        selected_output=selected_output, output_names=output_names,
        pending_code=None,
    )
    return None
