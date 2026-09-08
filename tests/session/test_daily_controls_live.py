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
    parser.add_argument("--wpctl", type=Path, required=True)
    parser.add_argument("--kbuildsycoca", type=Path, required=True)
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


def _with_render_device(argv: list[str], render_node: Path = Path("/dev/dri/renderD128"),
                        sysfs: Path = Path("/sys")) -> list[str]:
    """Scope KWin's EGL resource to the screenshot row without KMS access."""
    if os.environ.get("CI"):
        raise SandboxContractError("KWin virtual EGL is not valid under CI's card1 workaround")
    if not render_node.is_char_device() or not os.access(render_node, os.R_OK | os.W_OK):
        raise SandboxContractError("required DRM render node is unavailable")
    if not sysfs.is_dir():
        raise SandboxContractError("required read-only sysfs discovery root is unavailable")
    try:
        index = argv.index("--dev") + 2
    except ValueError as error:
        raise SandboxContractError("sandbox has no private /dev mount") from error
    return [*argv[:index], "--dev-bind", str(render_node), str(render_node),
            "--dir", "/sys", "--ro-bind", str(sysfs), "/sys", *argv[index:]]


def _wait_for_path(path: Path, label: str) -> None:
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        if path.exists():
            return
        time.sleep(0.05)
    raise RuntimeError(f"{label} did not become ready: {path}")


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


def _prepare_service_cache(arguments: argparse.Namespace, environment: dict[str, str]) -> None:
    """Expose staged desktop entries through the private KService cache."""
    menus = Path(environment["XDG_CONFIG_HOME"]) / "menus"
    menus.mkdir(parents=True, exist_ok=True)
    (menus / "applications.menu").write_text(
        '<!DOCTYPE Menu PUBLIC "-//freedesktop//DTD Menu 1.0//EN" '
        '"http://www.freedesktop.org/standards/menu-spec/1.0/menu.dtd">'
        "<Menu><Name>Applications</Name><DefaultAppDirs/><Include><All/></Include></Menu>",
        encoding="utf-8")
    cache = subprocess.run([str(arguments.kbuildsycoca), "--noincremental"],
                           env=environment, capture_output=True, text=True,
                           check=False, timeout=15)
    if cache.returncode:
        raise RuntimeError("private KService cache setup failed: " + cache.stderr)


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
    environment.update({
        "QINDAQT_DEVELOPMENT_CONTROL": "1",
        "PIPEWIRE_RUNTIME_DIR": environment["XDG_RUNTIME_DIR"],
        # KWin's ScreenShot2 plugin requires its EglBackend. The shared private
        # harness defaults to QPainter for protocol rows, so this capture row
        # explicitly selects software OpenGL and proves the public backend type.
        "KWIN_COMPOSE": "O2",
        "LIBGL_ALWAYS_SOFTWARE": "1",
    })
    runtime = Path(environment["XDG_RUNTIME_DIR"])
    processes: list[subprocess.Popen[str]] = []
    try:
        processes.append(_run([str(arguments.dbus_daemon), "--session", "--nofork", "--nopidfile",
                               f"--address={environment['DBUS_SESSION_BUS_ADDRESS']}"], environment, "bus"))
        _wait_for_path(runtime / "bus", "private session bus")
        processes.append(_run([str(arguments.pipewire), "-c", str(arguments.pipewire_config)], environment, "pipewire"))
        time.sleep(1)
        processes.append(_run([str(arguments.wireplumber), "-p", "policy"], environment, "wireplumber"))
        time.sleep(1)
        created = subprocess.run([str(arguments.pw_cli), "create-node", "adapter",
            '{ factory.name = support.null-audio-sink node.name = qindaqt.daily.output node.description = "QindaQt Daily Output" media.class = Audio/Sink object.linger = true audio.volume = 0.5 audio.position = [ FL FR ] }'],
            env=environment, capture_output=True, text=True, check=False, timeout=10)
        if created.returncode:
            raise RuntimeError("private default output creation failed: " + created.stderr)
        lowered = subprocess.run([str(arguments.wpctl), "set-volume", "@DEFAULT_AUDIO_SINK@", "0.5"],
                                 env=environment, capture_output=True, text=True, check=False, timeout=10)
        if lowered.returncode:
            raise RuntimeError("private default output volume setup failed: " + lowered.stderr)
        # KWin resolves restricted interfaces through KApplicationTrader. The
        # private root deliberately omits host /etc, so provide the smallest
        # normal XDG applications menu before rebuilding the private cache.
        _prepare_service_cache(arguments, environment)
        processes.append(_run([str(stage.executables["settings-service"])], environment, "settings"))
        processes.append(_run([str(stage.executables["audio-service"])], environment, "audio"))
        time.sleep(0.2)
        with (Path("/var/log/qindaqt-desktop") / "bus-state.log").open("w", encoding="utf-8") as log:
            for label, process in (("settings", processes[-2]), ("audio", processes[-1])):
                log.write(f"{label} poll={process.poll()}\n")
            state = subprocess.run(["/usr/bin/busctl", "--address", environment["DBUS_SESSION_BUS_ADDRESS"], "list"],
                                   env=environment, capture_output=True, text=True, check=False, timeout=5)
            log.write(f"busctl exit={state.returncode}\n{state.stdout}{state.stderr}")
        wrapper = runtime / "qindaqt-daily-session"
        screenshot_directory = str(Path(environment["XDG_DATA_HOME"]) / "qindaqt-evidence")
        Path(screenshot_directory).mkdir(parents=True, exist_ok=True)
        controls_wrapper = runtime / "qindaqt-daily-controls"
        controls_wrapper.write_text("#!/usr/bin/sh\nexec " + str(controls)
                                    + " > /var/log/qindaqt-desktop/desktop-controls.log 2>&1\n",
                                    encoding="utf-8")
        # Spectacle owns the installed Print action. These documented settings
        # are private to this disposable XDG tree: choose its rectangular UI
        # and autosave its actual image into the evidence bind mount.
        (Path(environment["XDG_CONFIG_HOME"]) / "spectaclerc").write_text(
            "[General]\nprintKeyRunningAction=0\nuseReleaseToCapture=true\n"
            "autoSaveImage=true\n[GuiConfig]\ncaptureMode=0\n[ImageSave]\n"
            "imageSaveLocation=file://" + screenshot_directory + "\n"
            "lastImageSaveLocation=file://" + screenshot_directory + "\n", encoding="utf-8")
        controls_wrapper.chmod(0o700)
        # Keep the normal session supervisor; only the host polkit default is
        # suppressed. The production controls binary gets normal Spectacle CLI
        # output configuration for a private, decoded capture artifact.
        # KWin owns org.kde.kglobalaccel in this virtual Wayland session. Do not
        # launch kglobalacceld beside it: it exits and obscures the real provider.
        wrapper.write_text("#!/usr/bin/sh\nexec " + str(stage.executables["session"])

                           + " --notification-host " + str(stage.executables["notification"])
                           + " --shell " + str(stage.executables["shell"])
                           + " --desktop-controls " + str(controls_wrapper)
                           + " --no-polkit-agent\n", encoding="utf-8")
        wrapper.chmod(0o700)
        scenario = Path("/opt/qindaqt-source/tests/scenarios/single-1080p.json")
        processes.append(_run([str(stage.executables["launcher"]), "--virtual", "--width", "1920", "--height", "1080",
                               "--scale", "1", "--output-count", "1", "--test-scenario", str(scenario),
                               "--session", str(wrapper)], environment, "kwin"))
        environment["QINDAQT_DAILY_CONTROLS_SCREENSHOT_DIRECTORY"] = screenshot_directory
        probe = subprocess.run([str(arguments.probe)], env=environment, capture_output=True,
                               text=True, check=False, timeout=60)
        # The optional child has no dedicated D-Bus name. Preserve a private
        # process and owner snapshot whether the behavioral probe passes or
        # fails, so an absent child is not mistaken for a shortcut failure.
        with (Path("/var/log/qindaqt-desktop") / "post-probe-state.log").open("w", encoding="utf-8") as log:
            process_state = subprocess.run(["/usr/bin/ps", "-eo", "pid,ppid,stat,args"], env=environment,
                                           capture_output=True, text=True, check=False, timeout=5)
            accel_state = subprocess.run(["/usr/bin/busctl", "--address", environment["DBUS_SESSION_BUS_ADDRESS"],
                                          "--no-pager", "status", "org.kde.kglobalaccel"],
                                         env=environment, capture_output=True, text=True, check=False, timeout=5)
            log.write(f"ps exit={process_state.returncode}\n{process_state.stdout}{process_state.stderr}")
            log.write(f"kglobalaccel exit={accel_state.returncode}\n{accel_state.stdout}{accel_state.stderr}")
        if probe.returncode:
            raise RuntimeError("daily controls probe failed:\n" + probe.stdout + probe.stderr)
        print(probe.stdout.strip())
        return 0
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        # Run roots are deleted after every attempt. Preserve only bounded
        # private child diagnostics in the outer failure before teardown.
        diagnostics = []
        for log in sorted(Path("/var/log/qindaqt-desktop").glob("*.log")):
            diagnostics.append(f"{log.name}:\n{log.read_text(encoding='utf-8', errors='replace')[-4096:]}")
        raise RuntimeError(f"{error}\nprivate child logs:\n" + "\n".join(diagnostics)) from error
    finally:
        _stop(processes)


def _outer(arguments: argparse.Namespace) -> int:
    if os.environ.get("QINDAQT_PRIVATE_RUNTIME_LANE") != LANE:
        return SKIP_CODE
    root = arguments.build_root.resolve(strict=True)
    # The release manager may supply an already staged combined candidate.
    # It is mounted read-only and never reinstalled or mutated by this row.
    stage_root = (arguments.stage_root.resolve(strict=True)
                  if arguments.stage_root is not None
                  else install_stage(arguments.cmake, root,
                                     root / "tests/session/daily-controls-stage"))
    run_id = secrets.token_hex(16)
    paths = create_run_root(root, run_id)
    try:
        tools = [arguments.python, arguments.dbus_daemon, arguments.kwin_wayland,
                 arguments.pipewire, arguments.wireplumber, arguments.pw_cli, arguments.wpctl,
                 arguments.kbuildsycoca, arguments.spectacle]
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
             "--wpctl", inside[arguments.wpctl], "--kbuildsycoca", inside[arguments.kbuildsycoca],
             "--spectacle", inside[arguments.spectacle], "--pipewire-config", sandbox_path_for(arguments.pipewire_config, mounts),
             "--probe", "/opt/qindaqt-tools/daily-controls-probe", "--bin-directory", arguments.bin_directory,
             "--plugin-relative", arguments.plugin_relative, "--decoration-relative", arguments.decoration_relative,
             "--settings-service-directory", arguments.settings_service_directory, "--audio-service-directory", arguments.audio_service_directory))
        write_command_evidence(paths.artifacts / "daily-controls-command.json", spec)
        command = _with_render_device(build_bwrap_argv(spec))
        with PrivateLaneLock():
            completed = subprocess.run(command, capture_output=True, text=True, timeout=170)
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
