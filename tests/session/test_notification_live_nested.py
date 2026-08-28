# SPDX-License-Identifier: GPL-3.0-or-later
"""Qualify notifications through a staged, private, nested QindaQt session."""

from __future__ import annotations

import argparse
import json
import os
import shlex
import stat
import subprocess
import sys
from pathlib import Path
from typing import Any

sys.dont_write_bytecode = True

from nested_session_scenario import (
    ScenarioCoverageError,
    VirtualOutputSpec,
    load_virtual_spec,
    write_virtual_output_config,
)
from notification_live_process import (
    InnerProcesses,
    await_service_pid,
    start_logged_process,
    terminate,
    terminate_authenticated_private_service,
)
from notification_live_restart import run_restart_lifecycle
from notification_live_arguments import parse_arguments
from notification_live_outer import run_outer, successful_inner_result


MARKER = "QINDAQT_NOTIFICATION_LIVE="
COMPOSITOR_SERVICE = "org.qindaqt.Compositor"
HOST_SERVICE = "org.freedesktop.Notifications"
SETTINGS_SERVICE = "org.qindaqt.Settings1"
SHELL_EVIDENCE_SERVICE = "org.qindaqt.ShellDevelopment"


def _object(value: Any, location: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise RuntimeError(f"{location} must be a JSON object")
    return value


def _extract_probe(stdout: str, phase: str) -> dict[str, Any]:
    markers = [
        line.removeprefix(MARKER)
        for line in stdout.splitlines()
        if line.startswith(MARKER)
    ]
    if len(markers) != 1:
        raise RuntimeError(f"{phase}: expected one probe marker, got {len(markers)}")
    result = _object(json.loads(markers[0]), f"{phase} result")
    if result.get("passed") is not True or result.get("phase") != phase:
        raise RuntimeError(f"{phase} failed: {result.get('failure', result)}")
    return result


def _run_probe(
    arguments: argparse.Namespace,
    phase: str,
    compositor_pid: int,
    host_pid: int,
    settings_pid: int,
    shell_pid: int,
    logical_width: int,
    logical_height: int,
    scale: float,
    resident_notification_id: int = 0,
    resident_owner_pid: int = 0,
) -> dict[str, Any]:
    command = [
        str(arguments.probe),
        "--phase",
        phase,
        "--compositor-pid",
        str(compositor_pid),
        "--notification-host-pid",
        str(host_pid),
        "--settings-pid",
        str(settings_pid),
        "--shell-pid",
        str(shell_pid),
        "--logical-width",
        str(logical_width),
        "--logical-height",
        str(logical_height),
        "--scale",
        f"{scale:.12g}",
        "--resident-notification-id",
        str(resident_notification_id),
        "--resident-owner-pid",
        str(resident_owner_pid),
    ]
    completed = subprocess.run(
        command, text=True, capture_output=True, timeout=45, check=False
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"{phase} probe exited {completed.returncode}:\n"
            + completed.stdout
            + completed.stderr
        )
    return _extract_probe(completed.stdout, phase)


def _scenario_profile(path: Path) -> tuple[str, str]:
    document = _object(json.loads(path.read_text(encoding="utf-8")), "scenario")
    profile = document.get("profile")
    theme = document.get("theme")
    if not isinstance(profile, str) or not profile or not isinstance(theme, str) or not theme:
        raise RuntimeError("scenario must name a profile and theme")
    return profile, theme


def _write_session_wrapper(
    root: Path, artifacts: dict[str, Path], profile: str, theme: str
) -> Path:
    wrapper = root / "qindaqt-notification-live-session"
    # Paths originate from the validated staged prefix and JSON supplies only
    # existing catalog ids. POSIX shell quoting must suppress every expansion;
    # JSON double quotes are not a shell-escaping mechanism.
    words = [
        str(artifacts["session"]),
        "--notification-host",
        str(artifacts["notification_host"]),
        "--shell",
        str(artifacts["shell"]),
        "--profile",
        profile,
        "--theme",
        theme,
    ]
    wrapper.write_text(
        "#!/bin/sh\nexec " + " ".join(shlex.quote(word) for word in words) + "\n",
        encoding="utf-8",
    )
    wrapper.chmod(stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)
    return wrapper


def _write_private_locker_policy(runtime_root: Path, config_home: Path) -> None:
    # AGENT-GUARD: D-Bus Lock is an immediate lock in KScreenLocker. Disable
    # password authentication only inside this disposable XDG tree so nested
    # compositor input can exercise the actual unlock transition safely.
    # KScreenLocker v6.6.5 settings/kscreenlockersettings.kcfg owns this exact
    # file, [Daemon] group, and RequirePassword key; do not invent aliases.
    root = runtime_root.resolve(strict=True)
    config = config_home.resolve(strict=True)
    if root not in config.parents:
        raise RuntimeError("private locker policy escaped the harness temp root")
    contents = "[Daemon]\nRequirePassword=false\n"
    destination = config / "kscreenlockerrc"
    destination.write_text(contents, encoding="utf-8")
    if destination.resolve().parent != config or destination.read_text(
        encoding="utf-8"
    ) != contents:
        raise RuntimeError("private locker policy did not round-trip exactly")


def _validate_inner_environment() -> None:
    if (
        os.environ.get("QINDAQT_NOTIFICATION_LIVE_PRIVATE_BUS") != "1"
        or not os.environ.get("DBUS_SESSION_BUS_ADDRESS")
        or os.environ.get("DISPLAY")
        or os.environ.get("WAYLAND_DISPLAY")
    ):
        raise RuntimeError("inner driver refused inherited host bus/display state")


def _run_settings_lifecycle(
    arguments: argparse.Namespace,
    artifacts: dict[str, Path],
    processes: InnerProcesses,
    outcomes: list[dict[str, Any]],
    compositor_pid: int,
    host_pid: int,
    settings_pid: int,
    shell_pid: int,
    spec: VirtualOutputSpec,
    settings_directory: Path,
    settings_log_path: Path,
) -> int:
    """Exercise persistence failure, uncertain loss, outage, and recovery."""
    settings_directory.chmod(0o500)
    try:
        outcomes.append(
            _run_probe(
                arguments,
                "settings-rejected",
                compositor_pid,
                host_pid,
                settings_pid,
                shell_pid,
                spec.logical_width,
                spec.logical_height,
                spec.scale,
            )
        )
    finally:
        settings_directory.chmod(0o700)

    outcomes.append(
        _run_probe(
            arguments,
            "settings-uncertain",
            compositor_pid,
            host_pid,
            settings_pid,
            shell_pid,
            spec.logical_width,
            spec.logical_height,
            spec.scale,
        )
    )
    if processes.settings is None:
        raise RuntimeError("private Settings1 process disappeared before wait")
    processes.settings.wait(timeout=5)
    outcomes.append(
        _run_probe(
            arguments,
            "settings-outage",
            compositor_pid,
            host_pid,
            0,
            shell_pid,
            spec.logical_width,
            spec.logical_height,
            spec.scale,
        )
    )
    processes.settings = start_logged_process(
        artifacts["settings_service"], settings_log_path
    )
    replacement_pid = await_service_pid(
        arguments.busctl,
        SETTINGS_SERVICE,
        expected=processes.settings.pid,
    )
    return replacement_pid


def _run_inner(arguments: argparse.Namespace, artifacts: dict[str, Path]) -> dict[str, Any]:
    _validate_inner_environment()
    # AGENT-CONTRACT: SettingsRouteLauncher consumes this exact staged path
    # only under the already authenticated private development boundary. It
    # starts a normal child instead of a detached process so group teardown
    # remains authoritative after every success or failure.
    os.environ["QINDAQT_NOTIFICATION_LIVE_SETTINGS_APP"] = str(artifacts["settings_app"])
    spec = load_virtual_spec(arguments.scenario)
    if spec.output_count != 1:
        raise RuntimeError("notification-live qualification requires one virtual output")
    profile, theme = _scenario_profile(arguments.scenario)
    config_home = Path(os.environ["XDG_CONFIG_HOME"])
    write_virtual_output_config(config_home, spec)
    _write_private_locker_policy(Path(arguments.runtime_root), config_home)
    wrapper = _write_session_wrapper(Path(arguments.runtime_root), artifacts, profile, theme)
    processes = InnerProcesses()
    log_path = Path(arguments.runtime_root) / "nested-session.log"
    settings_log_path = Path(arguments.runtime_root) / "settings-service.log"
    outcomes: list[dict[str, Any]] = []
    settings_directory = config_home / "qindaqt"
    try:
        processes.settings = start_logged_process(artifacts["settings_service"], settings_log_path)
        settings_pid = await_service_pid(
            arguments.busctl, SETTINGS_SERVICE, expected=processes.settings.pid
        )
        with log_path.open("w", encoding="utf-8") as log:
            processes.compositor = subprocess.Popen(
                [
                    str(artifacts["launcher"]),
                    "--virtual",
                    "--width",
                    str(spec.logical_width),
                    "--height",
                    str(spec.logical_height),
                    "--scale",
                    f"{spec.scale:.12g}",
                    "--output-count",
                    "1",
                    "--test-scenario",
                    str(arguments.scenario),
                    "--session",
                    str(wrapper),
                ],
                text=True,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
        compositor_pid = await_service_pid(
            arguments.busctl,
            COMPOSITOR_SERVICE,
            expected=processes.compositor.pid,
        )
        host_pid = await_service_pid(arguments.busctl, HOST_SERVICE)
        shell_pid = await_service_pid(arguments.busctl, SHELL_EVIDENCE_SERVICE)
        outcomes.append(
            _run_probe(
                arguments,
                "primary",
                compositor_pid,
                host_pid,
                settings_pid,
                shell_pid,
                spec.logical_width,
                spec.logical_height,
                spec.scale,
            )
        )

        replacement_settings_pid = _run_settings_lifecycle(
            arguments,
            artifacts,
            processes,
            outcomes,
            compositor_pid,
            host_pid,
            settings_pid,
            shell_pid,
            spec,
            settings_directory,
            settings_log_path,
        )

        replacement_shell_pid = run_restart_lifecycle(
            arguments,
            processes,
            outcomes,
            compositor_pid,
            host_pid,
            replacement_settings_pid,
            shell_pid,
            spec,
            log_path,
            _run_probe,
        )
        return successful_inner_result(
            spec,
            compositor_pid,
            host_pid,
            shell_pid,
            replacement_shell_pid,
            outcomes,
        )
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        # The outer temp root is intentionally deleted even on failure. Carry
        # the bounded nested log into the failure before teardown so a shell or
        # staged child startup error remains actionable afterward.
        diagnostic = ""
        if log_path.is_file():
            diagnostic = log_path.read_text(
                encoding="utf-8", errors="replace"
            )[-16_384:]
        raise RuntimeError(
            f"{error}\nnested-session.log (tail):\n{diagnostic}"
        ) from error
    finally:
        if settings_directory.exists():
            settings_directory.chmod(0o700)
        terminate(processes.compositor)
        terminate(processes.settings)
        terminate(processes.resident_owner)


def main() -> int:
    arguments = parse_arguments(__doc__)
    try:
        if arguments.inner:
            result = _run_inner(arguments, arguments.artifacts)
        else:
            if arguments.repeat < 1 or arguments.repeat > 10:
                raise RuntimeError("repeat must be from 1 through 10")
            result = run_outer(arguments, Path(__file__).resolve())
    except (
        OSError,
        RuntimeError,
        ScenarioCoverageError,
        subprocess.TimeoutExpired,
        json.JSONDecodeError,
    ) as error:
        print(f"notification-live qualification failed: {error}", file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
