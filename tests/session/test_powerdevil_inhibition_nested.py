# SPDX-License-Identifier: GPL-3.0-or-later
"""Run the real installed idle-inhibition paths inside one private nested session."""

from __future__ import annotations

import argparse
import json
import os
import re
import secrets
import subprocess
import sys
import time
from pathlib import Path, PurePosixPath
from typing import Mapping

from desktop_session_host_tools import (
    library_search_roots,
    sandbox_path_for,
    system_mounts,
)
from desktop_session_process import (
    RuntimeState,
    capture_process_identity,
    spawn_logged_process,
    terminate_processes,
    wait_for_path,
)
from desktop_session_sandbox import (
    FORBIDDEN_ENVIRONMENT,
    PrivateLaneLock,
    ReadOnlyMount,
    SandboxContractError,
    SandboxSpec,
    build_bwrap_argv,
    create_run_root,
    remove_run_root,
    sandbox_environment,
    write_command_evidence,
)
from desktop_session_stage import resolve_stage
from nested_session_scenario import VirtualOutputSpec, write_virtual_output_config


SKIP_CODE = 77
LANE_ENVIRONMENT = "QINDAQT_PRIVATE_RUNTIME_LANE"
LANE_VALUE = "powerdevil-inhibition"
EXPECTED_RELEASE = "6.6.6"
ATTEMPT_TIMEOUT_SECONDS = 225
SERVICE_TIMEOUT_SECONDS = 20.0


def _exact_release_inputs(arguments: argparse.Namespace) -> dict[str, str]:
    """Read exact package/version inputs without starting a graphical process."""

    requested = (
        "kde-plasma/kwin",
        "kde-plasma/kscreenlocker",
        "kde-plasma/libkscreen",
        "kde-plasma/powerdevil",
        "kde-plasma/xdg-desktop-portal-kde",
    )
    completed = subprocess.run(
        [str(arguments.qlist), "-Iv", *requested],
        check=False,
        capture_output=True,
        text=True,
        timeout=10,
    )
    if completed.returncode:
        raise SandboxContractError("Gentoo package identity query failed")
    installed = {}
    for line in completed.stdout.splitlines():
        match = re.fullmatch(r"(kde-plasma/[a-z0-9-]+)-([0-9][^-\s]*)(?:-r\d+)?", line.strip())
        if match:
            installed[match.group(1)] = match.group(2)
    if not set(requested).issubset(installed):
        raise SandboxContractError(f"exact power stack is incomplete: {installed!r}")
    mismatched = {atom: installed[atom] for atom in requested
                  if installed[atom] != EXPECTED_RELEASE}
    if mismatched:
        raise SandboxContractError(f"power stack is not release {EXPECTED_RELEASE}: {mismatched!r}")
    kwin = subprocess.run(
        [str(arguments.kwin_wayland), "--version"],
        check=False,
        capture_output=True,
        text=True,
        timeout=10,
    )
    if kwin.returncode or f"kwin {EXPECTED_RELEASE}" not in (kwin.stdout + kwin.stderr):
        raise SandboxContractError("configured KWin executable is not 6.6.6")
    return {atom: installed[atom] for atom in requested}


def _make_spec(arguments: argparse.Namespace, run_id: str, paths: object) -> SandboxSpec:
    tools = [
        arguments.python,
        arguments.dbus_daemon,
        arguments.kwin_wayland,
        arguments.powerdevil,
        arguments.portal_frontend,
        arguments.portal_kde,
    ]
    mounts = system_mounts(tools)
    sandbox_tools = {path: sandbox_path_for(path, mounts) for path in tools}
    system_path = sorted({str(PurePosixPath(path).parent)
                          for path in sandbox_tools.values()})
    library_path, qt_plugin_path, qml_import_path = library_search_roots(tools)
    environment = sandbox_environment(
        run_id=run_id,
        uid=os.getuid(),
        stage_bin=f"/opt/qindaqt/{arguments.bin_directory}",
        system_path=system_path,
        library_path=library_path,
        qt_plugin_path=qt_plugin_path,
        qml_import_path=qml_import_path,
    )
    environment["QT_FORCE_STDERR_LOGGING"] = "1"
    environment["QT_LOGGING_RULES"] = (
        "kf5idletime_wayland.debug=true;org.kde.powerdevil.debug=true"
    )
    command = (
        sandbox_tools[arguments.python],
        "/opt/qindaqt-source/tests/session/test_powerdevil_inhibition_nested.py",
        "--inner",
        "--stage-root", "/opt/qindaqt",
        "--bin-directory", arguments.bin_directory,
        "--plugin-relative", arguments.plugin_relative,
        "--decoration-relative", arguments.decoration_relative,
        "--settings-service-directory", arguments.settings_service_directory,
        "--audio-service-directory", arguments.audio_service_directory,
        "--probe", "/opt/qindaqt-tools/qindaqt-powerdevil-inhibition-probe",
        "--dbus-daemon", sandbox_tools[arguments.dbus_daemon],
        "--kwin-wayland", sandbox_tools[arguments.kwin_wayland],
        "--powerdevil", sandbox_tools[arguments.powerdevil],
        "--portal-frontend", sandbox_tools[arguments.portal_frontend],
        "--portal-kde", sandbox_tools[arguments.portal_kde],
    )
    return SandboxSpec(
        bwrap=arguments.bwrap,
        run_id=run_id,
        uid=os.getuid(),
        paths=paths,
        stage=ReadOnlyMount(arguments.stage_root, PurePosixPath("/opt/qindaqt")),
        tests=ReadOnlyMount(arguments.source_root, PurePosixPath("/opt/qindaqt-source")),
        probe=ReadOnlyMount(
            arguments.probe,
            PurePosixPath("/opt/qindaqt-tools/qindaqt-powerdevil-inhibition-probe"),
        ),
        system_mounts=mounts,
        environment=environment,
        command=command,
    )


def _configure_private_runtime(environment: Mapping[str, str]) -> str:
    config = Path(environment["XDG_CONFIG_HOME"])
    config.mkdir(parents=True, exist_ok=True)
    write_virtual_output_config(
        config, VirtualOutputSpec("powerdevil-1080p", 1, 1920, 1080, 1920, 1080, 1.0)
    )
    (config / "kscreenlockerrc").write_text(
        "[Daemon]\nAutolock=false\nLockOnResume=false\n", encoding="utf-8"
    )
    profile = []
    for name in ("AC", "Battery", "LowBattery"):
        profile.extend([
            f"[{name}][Display]",
            "DimDisplayWhenIdle=false",
            "TurnOffDisplayWhenIdle=true",
            "TurnOffDisplayIdleTimeoutSec=30",
            "TurnOffDisplayIdleTimeoutWhenLockedSec=30",
            "LockBeforeTurnOffDisplay=false",
            "",
            f"[{name}][SuspendAndShutdown]",
            "AutoSuspendAction=0",
            "AutoSuspendIdleTimeoutSec=0",
            "",
        ])
    (config / "powerdevilrc").write_text("\n".join(profile), encoding="utf-8")
    portal = config / "xdg-desktop-portal" / "portals.conf"
    portal.parent.mkdir(parents=True, exist_ok=True)
    portal.write_text(
        "[preferred]\n"
        "default=none\n"
        "org.freedesktop.impl.portal.Inhibit=kde\n",
        encoding="utf-8",
    )
    return f"qindaqt-powerdevil-{environment['QINDAQT_SESSION_RUN_ID'][:12]}"


def _owner_pid(environment: Mapping[str, str], service: str) -> int | None:
    completed = subprocess.run(
        ["/usr/bin/gdbus", "call", "--session", "--dest", "org.freedesktop.DBus",
         "--object-path", "/org/freedesktop/DBus", "--method",
         "org.freedesktop.DBus.GetConnectionUnixProcessID", service],
        env=dict(environment), check=False, capture_output=True, text=True, timeout=3,
    )
    match = re.search(r"uint32 (\d+)", completed.stdout)
    return int(match.group(1)) if completed.returncode == 0 and match else None


def _await_owner(
    environment: Mapping[str, str], service: str, expected_pid: int | None = None
) -> int:
    deadline = time.monotonic() + SERVICE_TIMEOUT_SECONDS
    last_pid = None
    while time.monotonic() < deadline:
        last_pid = _owner_pid(environment, service)
        if last_pid is not None and (expected_pid is None or last_pid == expected_pid):
            return last_pid
        time.sleep(0.1)
    raise RuntimeError(
        f"private service {service} owner mismatch: expected={expected_pid}, observed={last_pid}"
    )


def _await_interface(
    environment: Mapping[str, str], service: str, interface: str, expected_pid: int
) -> int:
    deadline = time.monotonic() + SERVICE_TIMEOUT_SECONDS
    last_detail = "service did not acquire its name"
    while time.monotonic() < deadline:
        owner_pid = _owner_pid(environment, service)
        if owner_pid == expected_pid:
            completed = subprocess.run(
                ["/usr/bin/gdbus", "introspect", "--session", "--dest", service,
                 "--object-path", "/org/freedesktop/portal/desktop"],
                env=dict(environment), check=False, capture_output=True, text=True, timeout=3,
            )
            if completed.returncode == 0 and f"interface {interface}" in completed.stdout:
                return owner_pid
            last_detail = completed.stderr.strip() or f"{interface} absent"
        elif owner_pid is not None:
            last_detail = f"owner {owner_pid}, expected {expected_pid}"
        time.sleep(0.1)
    raise RuntimeError(f"private portal {service} did not expose {interface}: {last_detail}")


def _run_probe(
    arguments: argparse.Namespace, environment: Mapping[str, str], mode: str
) -> dict[str, object]:
    completed = subprocess.run(
        [str(arguments.probe), "--mode", mode],
        env=dict(environment), check=False, capture_output=True, text=True, timeout=82,
    )
    lines = [line for line in completed.stdout.splitlines() if line.strip()]
    if completed.returncode or not lines:
        raise RuntimeError(
            f"{mode} inhibition probe failed with {completed.returncode}: "
            f"{completed.stderr.strip()} {completed.stdout.strip()}"
        )
    document = json.loads(lines[-1])
    if document.get("outcome") != "success" or document.get("offTransitions") != 1:
        raise RuntimeError(f"{mode} inhibition evidence was not successful: {document!r}")
    print(
        "QINDAQT_POWERDEVIL_MODE_EVIDENCE=" + json.dumps(document, sort_keys=True),
        flush=True,
    )
    return document


def _cleanup(state: RuntimeState) -> None:
    if not state.identities:
        return
    terminate_processes(reversed(state.identities), orderly_roles=("compositor",))


def run_inner(arguments: argparse.Namespace) -> int:
    if FORBIDDEN_ENVIRONMENT.intersection(os.environ):
        raise SandboxContractError("inner process inherited a forbidden host endpoint")
    stage = resolve_stage(
        arguments.stage_root,
        bin_directory=arguments.bin_directory,
        plugin_relative=arguments.plugin_relative,
        decoration_relative=arguments.decoration_relative,
        settings_service_directory=arguments.settings_service_directory,
        audio_service_directory=arguments.audio_service_directory,
    )
    environment = dict(os.environ)
    socket_name = _configure_private_runtime(environment)
    state = RuntimeState()
    evidence: dict[str, object] = {"schemaVersion": 1, "providers": {}, "modes": []}
    try:
        bus = spawn_logged_process(
            "powerdevil-private-bus",
            [str(arguments.dbus_daemon), "--session", "--nofork", "--nopidfile",
             f"--address={environment['DBUS_SESSION_BUS_ADDRESS']}"],
            environment,
        )
        state.track(bus, [arguments.dbus_daemon])
        wait_for_path(Path(environment["XDG_RUNTIME_DIR"]) / "bus", state, 5)

        compositor = spawn_logged_process(
            "compositor",
            [str(stage.executables["launcher"]), "--plugin-root",
             str(stage.compositor_plugin.parents[2]), "--kwin", str(arguments.kwin_wayland),
             "--virtual", "--width", "1920", "--height", "1080", "--scale", "1",
             "--output-count", "1", "--socket", socket_name, "--no-xwayland",
             "--no-global-shortcuts", "--session="],
            environment,
        )
        state.track(compositor, [stage.executables["launcher"], arguments.kwin_wayland])
        wait_for_path(Path(environment["XDG_RUNTIME_DIR"]) / socket_name, state, 15)
        app_environment = dict(environment)
        app_environment["WAYLAND_DISPLAY"] = socket_name

        powerdevil = spawn_logged_process(
            "powerdevil", [str(arguments.powerdevil)], app_environment
        )
        state.track(powerdevil, [arguments.powerdevil])
        providers = {
            "powerdevil": _await_owner(
                app_environment, "org.kde.Solid.PowerManagement", powerdevil.pid
            ),
            "policyAgent": _await_owner(
                app_environment, "org.kde.Solid.PowerManagement.PolicyAgent", powerdevil.pid
            ),
            "screenSaver": _await_owner(app_environment, "org.freedesktop.ScreenSaver"),
        }

        backend_environment = dict(app_environment)
        backend_environment["XDG_CURRENT_DESKTOP"] = "KDE"
        portal_kde = spawn_logged_process(
            "portal-kde", [str(arguments.portal_kde)], backend_environment
        )
        state.track(portal_kde, [arguments.portal_kde])
        providers["portalKde"] = _await_interface(
            app_environment,
            "org.freedesktop.impl.portal.desktop.kde",
            "org.freedesktop.impl.portal.Inhibit",
            portal_kde.pid,
        )

        portal_frontend = spawn_logged_process(
            "portal-frontend", [str(arguments.portal_frontend), "--verbose"], app_environment
        )
        state.track(portal_frontend, [arguments.portal_frontend])
        providers["portalFrontend"] = _await_interface(
            app_environment,
            "org.freedesktop.portal.Desktop",
            "org.freedesktop.portal.Inhibit",
            portal_frontend.pid,
        )
        evidence["providers"] = providers
        evidence["processes"] = {
            "compositor": compositor.pid,
            "powerdevil": powerdevil.pid,
            "portalKde": portal_kde.pid,
            "portalFrontend": portal_frontend.pid,
        }
        for mode in ("native", "portal", "legacy"):
            evidence["modes"].append(_run_probe(arguments, app_environment, mode))
    finally:
        _cleanup(state)
    print("QINDAQT_POWERDEVIL_INHIBITION_EVIDENCE=" + json.dumps(evidence, sort_keys=True))
    return 0


def run_outer(arguments: argparse.Namespace) -> int:
    if os.environ.get(LANE_ENVIRONMENT) != LANE_VALUE:
        print(
            f"PowerDevil qualification skipped: {LANE_ENVIRONMENT}={LANE_VALUE!r} is required",
            file=sys.stderr,
        )
        return SKIP_CODE
    package_versions = _exact_release_inputs(arguments)
    run_id = secrets.token_hex(16)
    paths = create_run_root(arguments.build_root, run_id)
    try:
        spec = _make_spec(arguments, run_id, paths)
        write_command_evidence(paths.artifacts / "powerdevil-sandbox-command.json", spec)
        with PrivateLaneLock():
            completed = subprocess.run(
                build_bwrap_argv(spec), check=False, capture_output=True, text=True,
                timeout=ATTEMPT_TIMEOUT_SECONDS,
            )
        if completed.stdout:
            print(completed.stdout, end="" if completed.stdout.endswith("\n") else "\n")
        if completed.returncode:
            if completed.stderr:
                print(completed.stderr, file=sys.stderr, end="")
            for log_path in sorted(paths.logs.glob("*.log")):
                lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()
                print(f"--- {log_path.name} (last 30 lines) ---", file=sys.stderr)
                print("\n".join(lines[-30:]), file=sys.stderr)
            return completed.returncode
        print("QINDAQT_POWERDEVIL_RELEASE_INPUTS=" + json.dumps(package_versions, sort_keys=True))
        return 0
    finally:
        remove_run_root(paths, arguments.build_root, run_id)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--outer", action="store_true")
    mode.add_argument("--inner", action="store_true")
    parser.add_argument("--build-root", type=Path)
    parser.add_argument("--source-root", type=Path)
    parser.add_argument("--stage-root", type=Path, required=True)
    parser.add_argument("--bin-directory", required=True)
    parser.add_argument("--plugin-relative", required=True)
    parser.add_argument("--decoration-relative", required=True)
    parser.add_argument("--settings-service-directory", required=True)
    parser.add_argument("--audio-service-directory", required=True)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--bwrap", type=Path)
    parser.add_argument("--python", type=Path, default=Path(sys.executable))
    parser.add_argument("--dbus-daemon", type=Path, required=True)
    parser.add_argument("--kwin-wayland", type=Path, required=True)
    parser.add_argument("--powerdevil", type=Path, required=True)
    parser.add_argument("--portal-frontend", type=Path, required=True)
    parser.add_argument("--portal-kde", type=Path, required=True)
    parser.add_argument("--qlist", type=Path)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        if arguments.outer and (
            arguments.build_root is None or arguments.source_root is None
            or arguments.bwrap is None or arguments.qlist is None
        ):
            raise SandboxContractError("outer mode requires build/source/bwrap/qlist inputs")
        return run_outer(arguments) if arguments.outer else run_inner(arguments)
    except (json.JSONDecodeError, OSError, RuntimeError, subprocess.SubprocessError,
            ValueError) as error:
        print(f"PowerDevil inhibition qualification failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
