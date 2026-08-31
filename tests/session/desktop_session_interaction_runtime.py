# SPDX-License-Identifier: GPL-3.0-or-later
"""Private-seat interaction and diagnostic process boundary."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
from pathlib import Path
from typing import Any, Mapping

from desktop_session_process import RuntimeState
from desktop_session_output import validate_secondary_output_authority
from desktop_session_readiness import parse_and_archive_probe


def _secondary_primary_environment(
    environment: Mapping[str, str],
    backend_plugin: Path,
) -> dict[str, str]:
    """Expose the host KScreen backend only to the primary-selector child."""

    if not backend_plugin.is_file():
        raise RuntimeError("private multi-output KScreen backend is unavailable")
    plugin_root = str(backend_plugin.parents[2])
    result = dict(environment)
    existing = [
        entry for entry in result.get("QT_PLUGIN_PATH", "").split(":") if entry
    ]
    if plugin_root not in existing:
        existing.append(plugin_root)
    # AGENT-CONTRACT: The private Arch Qt plugin root remains first. Only the
    # selector child receives the host libkscreen backend plugin directory.
    result["QT_PLUGIN_PATH"] = ":".join(existing)
    return result


def _capture_interaction_failure(
    arguments: argparse.Namespace,
    environment: Mapping[str, str],
    state: RuntimeState,
    *,
    interaction_argument: str,
    interaction_return_code: int | None,
    failure_log_path: Path,
) -> None:
    """Snapshot the still-live private desktop after interaction has failed."""

    context: dict[str, object] = {
        "schemaVersion": 1,
        "interactionArgument": interaction_argument,
        "interactionReturnCode": interaction_return_code,
        "requestedOutputName": (
            "WL-1"
            if interaction_argument == "--open-notification-center-secondary"
            else "WL-0"
        ),
    }
    output = ""
    try:
        process = subprocess.Popen(
            [str(arguments.probe)], env=dict(environment),
            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, start_new_session=True,
        )
    except BaseException as error:
        context.update({"status": "launch-failed", "failure": str(error)})
    else:
        state.track(process, [arguments.probe])
        try:
            output, _ = process.communicate(timeout=1)
        except subprocess.TimeoutExpired as error:
            context.update({"status": "timeout", "failure": str(error)})
            if isinstance(error.output, str):
                output = error.output
        else:
            context.update({
                "status": "captured",
                "probeReturnCode": process.returncode,
            })
    # AGENT-CONTRACT: This diagnostic starts only after the acceptance probe
    # fails and cannot change the original event sequence or verdict.
    with failure_log_path.open("w", encoding="utf-8") as diagnostic:
        diagnostic.write(
            "QINDAQT_DESKTOP_SESSION_INTERACTION_FAILURE="
            f"{json.dumps(context, sort_keys=True, separators=(',', ':'))}\n"
        )
        diagnostic.write(output)


def _interaction_document(
    output: str,
    arguments: argparse.Namespace,
    environment: Mapping[str, str],
    state: RuntimeState,
    interaction_argument: str,
    return_code: int | None,
    failure_log_path: Path,
) -> dict[str, Any]:
    marker = "QINDAQT_DESKTOP_SESSION_INTERACTION="
    lines = [line for line in output.splitlines() if line.startswith(marker)]
    if return_code != 0 or len(lines) != 1:
        _capture_interaction_failure(
            arguments, environment, state,
            interaction_argument=interaction_argument,
            interaction_return_code=return_code,
            failure_log_path=failure_log_path,
        )
        raise RuntimeError("private-seat interaction did not return exact evidence")
    try:
        document = json.loads(lines[0].removeprefix(marker))
    except (json.JSONDecodeError, UnicodeError):
        _capture_interaction_failure(
            arguments, environment, state,
            interaction_argument=interaction_argument,
            interaction_return_code=return_code,
            failure_log_path=failure_log_path,
        )
        raise
    if not isinstance(document, dict):
        _capture_interaction_failure(
            arguments, environment, state,
            interaction_argument=interaction_argument,
            interaction_return_code=return_code,
            failure_log_path=failure_log_path,
        )
        raise RuntimeError("private-seat interaction evidence was malformed")
    return document


def _run_interaction(
    arguments: argparse.Namespace,
    environment: Mapping[str, str],
    state: RuntimeState,
    *,
    secondary_output: bool = False,
    interaction_log_path: Path = Path(
        "/var/log/qindaqt-desktop/session-interaction.log"
    ),
    failure_log_path: Path = Path(
        "/var/log/qindaqt-desktop/session-interaction-failure.log"
    ),
) -> dict[str, Any]:
    interaction_argument = (
        "--open-notification-center-secondary"
        if secondary_output else "--open-notification-center"
    )
    log = interaction_log_path.open("w", encoding="utf-8")
    try:
        process = subprocess.Popen(
            [str(arguments.probe), interaction_argument],
            env=dict(environment), stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE, stderr=log, text=True,
            start_new_session=True,
        )
    except BaseException:
        log.close()
        raise
    process._qindaqt_log = log  # type: ignore[attr-defined]
    state.track(process, [arguments.probe])
    output, _ = process.communicate(timeout=5)
    # Preserve the exact marker before parsing; canonical evidence is validated
    # separately after cleanup.
    log.write(output)
    log.flush()
    return _interaction_document(
        output, arguments, environment, state, interaction_argument,
        process.returncode, failure_log_path,
    )


def _select_secondary_primary(
    arguments: argparse.Namespace,
    environment: Mapping[str, str],
    state: RuntimeState,
) -> None:
    """Make WL-1 the private dual-row primary before opening shell chrome."""

    executable = arguments.kscreen_doctor
    backend_plugin = arguments.kscreen_wayland_backend
    if (
        not executable.is_absolute()
        or not executable.is_file()
        or not os.access(executable, os.X_OK)
    ):
        raise RuntimeError("private multi-output selector is unavailable")
    if not backend_plugin.is_absolute() or not backend_plugin.is_file():
        raise RuntimeError("private multi-output KScreen backend is unavailable")
    log = Path("/var/log/qindaqt-desktop/secondary-primary.log").open(
        "w", encoding="utf-8"
    )
    try:
        process = subprocess.Popen(
            [str(executable), "output.WL-1.primary"],
            env=_secondary_primary_environment(environment, backend_plugin),
            stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
            text=True, start_new_session=True,
        )
    except BaseException:
        log.close()
        raise
    process._qindaqt_log = log  # type: ignore[attr-defined]
    state.track(process, [executable])
    if process.wait(timeout=5) != 0:
        raise RuntimeError("private multi-output primary selection failed")


def _read_post_selector_outputs(
    arguments: argparse.Namespace,
    environment: Mapping[str, str],
    state: RuntimeState,
    *,
    log_path: Path = Path(
        "/var/log/qindaqt-desktop/post-selector-output-probe.log"
    ),
) -> Mapping[str, Any]:
    """Reacquire one exact public Outputs envelope after primary selection."""

    log = log_path.open("w", encoding="utf-8")
    try:
        process = subprocess.Popen(
            [str(arguments.probe)], env=dict(environment),
            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=log,
            text=True, start_new_session=True,
        )
    except BaseException:
        log.close()
        raise
    process._qindaqt_log = log  # type: ignore[attr-defined]
    state.track(process, [arguments.probe])
    output, _ = process.communicate(timeout=2)
    marker = "QINDAQT_DESKTOP_SESSION_PROBE="
    lines = [line for line in output.splitlines() if line.startswith(marker)]
    if process.returncode != 0 or len(lines) != 1:
        raise RuntimeError("post-selector probe did not return exact evidence")
    document = parse_and_archive_probe(lines[0], log)
    outputs = document.get("outputs")
    if not isinstance(outputs, Mapping):
        raise RuntimeError("post-selector probe omitted its public Outputs envelope")
    return outputs


def _prepare_secondary_evidence(
    arguments: argparse.Namespace,
    environment: Mapping[str, str],
    state: RuntimeState,
    evidence: dict[str, Any],
) -> None:
    """Prove and preserve secondary authority before interaction can start."""

    _select_secondary_primary(arguments, environment, state)
    snapshot = _read_post_selector_outputs(arguments, environment, state)
    generations = evidence.get("generations")
    previous_generation = (
        generations.get("outputs") if isinstance(generations, Mapping) else None
    )
    evidence["postSelectorOutputs"] = validate_secondary_output_authority(
        snapshot,
        previous_outputs=evidence.get("outputs"),
        previous_generation=previous_generation,
    )
