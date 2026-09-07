# SPDX-License-Identifier: GPL-3.0-or-later
"""Run installed daily controls only in the manager-assigned private lane."""

from __future__ import annotations

import argparse
import json
import os
import secrets
import signal
import subprocess
import sys
import time
from pathlib import Path, PurePosixPath

from desktop_session_host_tools import library_search_roots, sandbox_path_for, system_mounts
from desktop_session_sandbox import (
    PrivateLaneLock, ReadOnlyMount, SandboxContractError, SandboxSpec,
    build_bwrap_argv, create_run_root, remove_run_root, sandbox_environment,
    write_command_evidence,
)
from desktop_session_stage import install_stage, resolve_stage

SKIP_CODE = 77
LANE = "daily-controls"


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inner", action="store_true")
    parser.add_argument("--build-root", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--cmake", type=Path, required=True)
    parser.add_argument("--bwrap", type=Path, required=True)
    parser.add_argument("--python", type=Path, required=True)
    parser.add_argument("--dbus-daemon", type=Path, required=True)
    parser.add_argument("--kwin-wayland", type=Path, required=True)
    parser.add_argument("--pipewire", type=Path, required=True)
    parser.add_argument("--wireplumber", type=Path, required=True)
    parser.add_argument("--pw-cli", type=Path, required=True)
    parser.add_argument("--spectacle", type=Path, required=True)
    parser.add_argument("--pipewire-config", type=Path, required=True)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--bin-directory", required=True)
    parser.add_argument("--plugin-relative", required=True)
    parser.add_argument("--decoration-relative", required=True)
    parser.add_argument("--settings-service-directory", required=True)
    parser.add_argument("--audio-service-directory", required=True)
    parser.add_argument("--stage-root", type=Path)
    return parser


def _run(command: list[str], environment: dict[str, str], label: str) -> subprocess.Popen[str]:
    log = Path("/var/log/qindaqt-desktop") / f"{label}.log"
    stream = log.open("w", encoding="utf-8")
    return subprocess.Popen(command, env=environment, stdout=stream, stderr=subprocess.STDOUT,
                            text=True, start_new_session=True)


def _stop(processes: list[subprocess.Popen[str]]) -> None:
    for process in reversed(processes):
        if process.poll() is None:
            os.killpg(process.pid, signal.SIGTERM)
    deadline = time.monotonic() + 5
    for process in reversed(processes):
        if process.poll() is None:
            try:
                process.wait(timeout=max(0.1, deadline - time.monotonic()))
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait(timeout=2)


def _inner(arguments: argparse.Namespace) -> int:
    if os.environ.get("QINDAQT_DAILY_CONTROLS_PRIVATE_BUS") != "1":
        raise SandboxContractError("inner runner refused a non-private daily-controls bus")
    stage = resolve_stage(arguments.stage_root, bin_directory=arguments.bin_directory,
                          plugin_relative=arguments.plugin_relative,
                          decoration_relative=arguments.decoration_relative,
                          settings_service_directory=arguments.settings_service_directory,
                          audio_service_directory=arguments.audio_service_directory)
    controls = stage.executables["session"].with_name("qindaqt-desktop-controls")
    if not controls.is_file() or not os.access(controls, os.X_OK):
        raise SandboxContractError("stage omitted qindaqt-desktop-controls")
    environment = dict(os.environ)
    environment.update({"QINDAQT_DEVELOPMENT_CONTROL": "1", "PIPEWIRE_RUNTIME_DIR": environment["XDG_RUNTIME_DIR"]})
    runtime = Path(environment["XDG_RUNTIME_DIR"])
    processes: list[subprocess.Popen[str]] = []
    try:
        processes.append(_run([str(arguments.dbus_daemon), "--session", "--nofork", "--nopidfile",
                               f"--address={environment['DBUS_SESSION_BUS_ADDRESS']}"], environment, "bus"))
        processes.append(_run([str(arguments.pipewire), "-c", str(arguments.pipewire_config)], environment, "pipewire"))
        time.sleep(1)
        processes.append(_run([str(arguments.wireplumber), "-p", "policy"], environment, "wireplumber"))
        time.sleep(1)
        created = subprocess.run([str(arguments.pw_cli), "create-node", "adapter",
            '{ factory.name = support.null-audio-sink node.name = qindaqt.daily.output node.description = "QindaQt Daily Output" media.class = Audio/Sink object.linger = true audio.position = [ FL FR ] }'],
            env=environment, capture_output=True, text=True, check=False, timeout=10)
        if created.returncode:
            raise RuntimeError("private default output creation failed: " + created.stderr)
        processes.append(_run([str(stage.executables["settings-service"])], environment, "settings"))
        processes.append(_run([str(stage.executables["audio-service"])], environment, "audio"))
        wrapper = runtime / "qindaqt-daily-session"
        wrapper.write_text("#!/bin/sh\nexec " + str(stage.executables["session"]) +
                           " --notification-host " + str(stage.executables["notification"]) +
                           " --shell " + str(stage.executables["shell"]) + "\n", encoding="utf-8")
        wrapper.chmod(0o700)
        scenario = Path("/opt/qindaqt-source/tests/scenarios/single-1080p.json")
        processes.append(_run([str(stage.executables["launcher"]), "--virtual", "--width", "1920", "--height", "1080",
                               "--scale", "1", "--output-count", "1", "--test-scenario", str(scenario),
                               "--session", str(wrapper)], environment, "kwin"))
        probe = subprocess.run([str(arguments.probe)], env=environment, capture_output=True,
                               text=True, check=False, timeout=60)
        if probe.returncode:
            raise RuntimeError("daily controls probe failed:\n" + probe.stdout + probe.stderr)
        print(probe.stdout.strip())
        return 0
    finally:
        _stop(processes)


def _outer(arguments: argparse.Namespace) -> int:
    if os.environ.get("QINDAQT_PRIVATE_RUNTIME_LANE") != LANE:
        return SKIP_CODE
    root = arguments.build_root.resolve(strict=True)
    stage_root = root / "tests/session/daily-controls-stage"
    install_stage(arguments.cmake, root, stage_root)
    run_id = secrets.token_hex(16)
    paths = create_run_root(root, run_id)
    try:
        tools = [arguments.python, arguments.dbus_daemon, arguments.kwin_wayland,
                 arguments.pipewire, arguments.wireplumber, arguments.pw_cli, arguments.spectacle]
        mounts = system_mounts(tools)
        inside = {tool: sandbox_path_for(tool, mounts) for tool in tools}
        libraries, plugins, qml = library_search_roots(tools)
        environment = sandbox_environment(run_id=run_id, uid=os.getuid(),
            stage_bin=f"/opt/qindaqt/{arguments.bin_directory}",
            system_path=sorted({str(PurePosixPath(path).parent) for path in inside.values()}),
            library_path=libraries, qt_plugin_path=plugins, qml_import_path=qml)
        environment["QINDAQT_DAILY_CONTROLS_PRIVATE_BUS"] = "1"
        spec = SandboxSpec(arguments.bwrap, run_id, os.getuid(), paths,
            ReadOnlyMount(stage_root, PurePosixPath("/opt/qindaqt")),
            ReadOnlyMount(arguments.source_root, PurePosixPath("/opt/qindaqt-source")),
            ReadOnlyMount(arguments.probe, PurePosixPath("/opt/qindaqt-tools/daily-controls-probe")),
            mounts, environment,
            (inside[arguments.python], "/opt/qindaqt-source/tests/session/test_daily_controls_live.py", "--inner",
             "--stage-root", "/opt/qindaqt", "--build-root", "/tmp", "--source-root", "/opt/qindaqt-source",
             "--cmake", inside[arguments.python], "--bwrap", inside[arguments.python], "--python", inside[arguments.python],
             "--dbus-daemon", inside[arguments.dbus_daemon], "--kwin-wayland", inside[arguments.kwin_wayland],
             "--pipewire", inside[arguments.pipewire], "--wireplumber", inside[arguments.wireplumber], "--pw-cli", inside[arguments.pw_cli],
             "--spectacle", inside[arguments.spectacle], "--pipewire-config", sandbox_path_for(arguments.pipewire_config, mounts),
             "--probe", "/opt/qindaqt-tools/daily-controls-probe", "--bin-directory", arguments.bin_directory,
             "--plugin-relative", arguments.plugin_relative, "--decoration-relative", arguments.decoration_relative,
             "--settings-service-directory", arguments.settings_service_directory, "--audio-service-directory", arguments.audio_service_directory))
        write_command_evidence(paths.artifacts / "daily-controls-command.json", spec)
        with PrivateLaneLock():
            completed = subprocess.run(build_bwrap_argv(spec), capture_output=True, text=True, timeout=170)
        if completed.returncode:
            raise RuntimeError("daily-controls private row failed:\n" + completed.stdout + completed.stderr)
        print(completed.stdout.strip())
        return 0
    finally:
        remove_run_root(paths, root, run_id)


def main() -> int:
    arguments = _parser().parse_args()
    try:
        return _inner(arguments) if arguments.inner else _outer(arguments)
    except (OSError, RuntimeError, SandboxContractError, subprocess.TimeoutExpired) as error:
        print(f"daily-controls qualification failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
