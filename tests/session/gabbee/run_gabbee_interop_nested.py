# SPDX-License-Identifier: GPL-3.0-or-later
"""Lane-gated Gabbee interop driver: outer bubblewrap host, inner nested session.

OUTER (run by root with the private-runtime lane allocated): validates the
exact ``QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop``
acknowledgement, takes the cross-worktree private-session lock, stages the
Terminal application component into a private prefix, and launches the
bubblewrap sandbox exactly like the desktop.virtual rows — plus read-only
mounts for the Gabbee checkout and the terminal stage.

INNER (inside the sandbox): boots the private session bus (probe-owned
service dir only), the nested QindaQt compositor, the shell session, the
Text Editor and Terminal applications; runs the GlobalShortcuts portal chain
and the Gabbee dictation/focus/grouped-member probe against the live nested
desktop; archives every artifact under the run's evidence root.

AGENT-CONTRACT: no microphone, no typing tool, no uinput, no host input
injection.  All processes live inside the bubblewrap PID/network namespace;
the only host-visible effect is the locked, per-user private-session lane.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import time
import uuid
from pathlib import Path
from types import SimpleNamespace

HERE = Path(__file__).resolve().parent

LANE_ENVIRONMENT = "QINDAQT_PRIVATE_RUNTIME_LANE"
LANE_VALUE = "interactive-virtual-desktop"

AT_SPI_SERVICE = """[D-BUS Service]
Name=org.a11y.Bus
Exec=/usr/libexec/at-spi-bus-launcher --launch-immediately
"""


def _outer() -> int:
    if os.environ.get(LANE_ENVIRONMENT) != LANE_VALUE:
        print(
            f"skip: set {LANE_ENVIRONMENT}={LANE_VALUE} after the manager allocates "
            "the private-runtime lane",
            file=sys.stderr,
        )
        return 77

    parser = argparse.ArgumentParser(description="outer Gabbee interop lane driver")
    parser.add_argument("--build-root", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=HERE.parents[3])
    parser.add_argument("--bwrap", default=shutil.which("bwrap"))
    parser.add_argument("--python", default=sys.executable)
    parser.add_argument("--dbus-daemon", default=shutil.which("dbus-daemon"))
    parser.add_argument("--kwin-wayland", required=True)
    parser.add_argument("--gabbee-root", type=Path, required=True)
    parser.add_argument(
        "--bin-directory",
        "--bin-dir",
        dest="bin_directory",
        default="bin",
    )
    parser.add_argument("--plugin-relative", required=True)
    parser.add_argument("--decoration-relative", required=True)
    parser.add_argument("--settings-service-directory", default="share/dbus-1/services")
    parser.add_argument("--audio-service-directory", default="share/dbus-1/services")
    parser.add_argument("--result-root", type=Path, default=None)
    arguments = parser.parse_args()

    sys.path.insert(0, str(HERE.parent))
    from desktop_session_sandbox import (
        PrivateLaneLock,
        build_bwrap_argv,
        create_run_root,
        remove_run_root,
    )
    from gabbee_probe_support import gabbee_venv_python

    gabbee_root = arguments.gabbee_root.resolve(strict=True)
    venv_python = gabbee_venv_python(gabbee_root / ".venv/bin/python")

    stage_root = arguments.build_root / "tests/session/desktop-session-stage"
    if not stage_root.is_dir():
        raise SystemExit(
            f"desktop stage is missing: {stage_root} — run "
            f"ctest --test-dir {arguments.build_root} --fixtures-setup desktop_virtual_stage first"
        )

    result_root = (arguments.result_root or arguments.build_root / "tests/session/gabbee-results").resolve()
    result_root.mkdir(parents=True, exist_ok=True)
    run_id = f"gabbee{uuid.uuid4().hex[:20]}"
    run_dir = result_root / run_id
    run_dir.mkdir()
    terminal_stage = run_dir / "terminal-stage"
    install = subprocess.run(
        [
            "cmake",
            "--install",
            str(arguments.build_root),
            "--component",
            "Terminal",
            "--prefix",
            str(terminal_stage),
        ],
        capture_output=True,
        text=True,
    )
    if install.returncode != 0:
        raise SystemExit(f"Terminal component install failed: {install.stderr[-2000:]}")

    with PrivateLaneLock():
        paths = create_run_root(arguments.build_root, run_id)
        try:
            spec = _outer_spec(arguments, gabbee_root, terminal_stage, paths, run_id)
            command = build_bwrap_argv(spec)
            (run_dir / "bwrap-argv.json").write_text(json.dumps(command, indent=2), "utf-8")
            completed = subprocess.run(command, capture_output=True, text=True, timeout=600)
            (run_dir / "inner-stdout.log").write_text(completed.stdout, "utf-8")
            (run_dir / "inner-stderr.log").write_text(completed.stderr, "utf-8")
            evidence = paths.artifacts
            for item in evidence.glob("*"):
                shutil.copy2(item, run_dir / item.name)
            return completed.returncode
        finally:
            remove_run_root(paths, arguments.build_root, run_id)


def _outer_spec(
    arguments: argparse.Namespace,
    gabbee_root: Path,
    terminal_stage: Path,
    paths: SimpleNamespace,
    run_id: str,
):
    from desktop_session_host_tools import library_search_roots, sandbox_path_for, system_mounts
    from desktop_session_sandbox import ReadOnlyMount, SandboxSpec, sandbox_environment
    from pathlib import PurePosixPath

    def posix(value: str) -> PurePosixPath:
        return PurePosixPath(value)

    tools = [
        Path(arguments.python).resolve(strict=True),
        Path(arguments.dbus_daemon).resolve(strict=True),
        Path(arguments.kwin_wayland).resolve(strict=True),
    ]
    mounts = list(system_mounts(tools))
    mounts.extend(
        [
            ReadOnlyMount(gabbee_root, posix("/opt/gabbee")),
            ReadOnlyMount(terminal_stage.resolve(strict=True), posix("/opt/qindaqt-terminal")),
        ]
    )
    python = sandbox_path_for(tools[0], tuple(mounts))
    dbus_daemon = sandbox_path_for(tools[1], tuple(mounts))
    kwin_wayland = sandbox_path_for(tools[2], tuple(mounts))
    system_path = sorted({posix(entry).parent for entry in (python, dbus_daemon, kwin_wayland)})
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
    terminal_bin = "/opt/qindaqt-terminal/" + arguments.bin_directory
    environment["PATH"] = ":".join(
        [terminal_bin, environment["PATH"]]
    )
    environment["QT_LINUX_ACCESSIBILITY_ALWAYS_ON"] = "1"
    environment["GABBEE_VENV_PYTHON"] = "/opt/gabbee/.venv/bin/python"
    command = (
        python,
        "/opt/qindaqt-source/tests/session/gabbee/run_gabbee_interop_nested.py",
        "--inner",
        "--stage-root",
        "/opt/qindaqt",
        "--bin-directory",
        arguments.bin_directory,
        "--plugin-relative",
        arguments.plugin_relative,
        "--decoration-relative",
        arguments.decoration_relative,
        "--settings-service-directory",
        arguments.settings_service_directory,
        "--audio-service-directory",
        arguments.audio_service_directory,
        "--dbus-daemon",
        dbus_daemon,
        "--kwin-wayland",
        kwin_wayland,
        "--gabbee-root",
        "/opt/gabbee",
        "--result-path",
        "/var/lib/qindaqt-evidence/gabbee-interop-result.json",
    )
    return SandboxSpec(
        bwrap=Path(arguments.bwrap),
        run_id=run_id,
        uid=os.getuid(),
        paths=paths,
        stage=ReadOnlyMount(
            (arguments.build_root / "tests/session/desktop-session-stage").resolve(strict=True),
            posix("/opt/qindaqt"),
        ),
        tests=ReadOnlyMount(
            arguments.source_root.resolve(strict=True), posix("/opt/qindaqt-source")
        ),
        probe=ReadOnlyMount(
            (HERE / "gabbee_interop_probe.py").resolve(strict=True),
            posix("/opt/qindaqt-tools/gabbee_interop_probe.py"),
        ),
        system_mounts=tuple(mounts),
        environment=environment,
        command=command,
    )


def _inner() -> int:
    sys.path.insert(0, str(HERE.parent))
    from desktop_session_launch import _configure_private_session, _virtual_spec
    from desktop_session_process import RuntimeState, spawn_logged_process, wait_for_path
    from desktop_session_stage import resolve_stage
    from gabbee_probe_support import (
        gabbee_probe_python_environment,
        stage_portals_configuration,
        write_fake_backend_service,
    )

    parser = argparse.ArgumentParser(description="inner Gabbee interop session")
    parser.add_argument("--inner", action="store_true")
    parser.add_argument("--stage-root", type=Path, required=True)
    parser.add_argument("--bin-directory", required=True)
    parser.add_argument("--plugin-relative", required=True)
    parser.add_argument("--decoration-relative", required=True)
    parser.add_argument("--settings-service-directory", required=True)
    parser.add_argument("--audio-service-directory", required=True)
    parser.add_argument("--dbus-daemon", required=True)
    parser.add_argument("--kwin-wayland", required=True)
    parser.add_argument("--gabbee-root", type=Path, required=True)
    parser.add_argument("--result-path", type=Path, required=True)
    arguments = parser.parse_args()

    stage = resolve_stage(
        arguments.stage_root,
        bin_directory=arguments.bin_directory,
        plugin_relative=arguments.plugin_relative,
        decoration_relative=arguments.decoration_relative,
        settings_service_directory=arguments.settings_service_directory,
        audio_service_directory=arguments.audio_service_directory,
    )
    environment = dict(os.environ)
    state = RuntimeState()
    exit_code = 1
    try:
        runtime = Path(environment["XDG_RUNTIME_DIR"])
        service_dir = runtime / "bus-services"
        service_dir.mkdir(parents=True, exist_ok=True)
        bus_config = runtime / "bus.conf"
        from gabbee_probe_support import BUS_CONFIG_TEMPLATE

        bus_config.write_text(
            BUS_CONFIG_TEMPLATE.format(
                address=f"unix:path={runtime / 'bus'}", service_dir=service_dir
            ),
            encoding="utf-8",
        )
        environment["DBUS_SESSION_BUS_ADDRESS"] = f"unix:path={runtime / 'bus'}"
        (service_dir / "org.a11y.Bus.service").write_text(AT_SPI_SERVICE, encoding="utf-8")
        write_fake_backend_service(
            service_dir,
            venv_python=Path(environment["GABBEE_VENV_PYTHON"]),
            fake_script=Path("/opt/qindaqt-source/tests/session/gabbee/gabbee_portal_fake.py"),
        )
        bus = spawn_logged_process(
            "dbus-daemon",
            [arguments.dbus_daemon, "--config-file", str(bus_config), "--nofork", "--nopidfile"],
            environment,
        )
        state.track(bus, [arguments.dbus_daemon])
        wait_for_path(runtime / "bus", state, 10)

        for role in ("settings-service", "audio-service"):
            child = spawn_logged_process(role, [str(stage.executables[role])], environment)
            state.track(child, [stage.executables[role]])

        socket_name = _configure_private_session(environment, None)
        virtual = _virtual_spec(None)
        compositor_environment = dict(environment)
        compositor_environment["QT_FORCE_STDERR_LOGGING"] = "1"
        compositor = spawn_logged_process(
            "compositor",
            [
                str(stage.executables["launcher"]),
                "--plugin-root", str(stage.compositor_plugin.parents[2]),
                "--kwin", arguments.kwin_wayland, "--virtual",
                "--width", str(virtual.logical_width),
                "--height", str(virtual.logical_height),
                "--scale", str(virtual.scale), "--output-count", str(virtual.output_count),
                "--socket", socket_name,
                "--test-scenario", "/opt/qindaqt-source/tests/scenarios/single-1080p.json",
                "--session", str(stage.executables["session"]),
            ],
            compositor_environment,
        )
        state.track(compositor, [stage.executables["launcher"], arguments.kwin_wayland])
        wait_for_path(runtime / socket_name, state, 20)

        app_environment = dict(environment)
        app_environment["WAYLAND_DISPLAY"] = socket_name
        app_environment["QT_LINUX_ACCESSIBILITY_ALWAYS_ON"] = "1"
        for role, executable in (
            ("editor-app", stage.executables["editor-app"]),
            ("terminal-app", Path("/opt/qindaqt-terminal") / arguments.bin_directory / "qindaqt-terminal"),
        ):
            child = spawn_logged_process(role, [str(executable)], app_environment)
            state.track(child, [executable])

        stage_portals_configuration(
            Path("/opt/qindaqt-source/src/services/portal/data/qindaqt-portals.conf"),
            Path(environment["XDG_CONFIG_HOME"]) / "xdg-desktop-portal/portals.conf",
        )

        probe_environment = dict(app_environment)
        probe_environment.update(
            gabbee_probe_python_environment(
                source_root=arguments.gabbee_root,
                venv_python=Path(environment["GABBEE_VENV_PYTHON"]),
                system_site_packages=Path("/usr/lib/python3.14/site-packages"),
            )
        )
        chain_environment = dict(environment)
        chain_environment.pop("WAYLAND_DISPLAY", None)
        evidence_root = Path("/var/lib/qindaqt-evidence")
        evidence_root.mkdir(parents=True, exist_ok=True)

        def run_step(name: str, command: list[str], env: dict[str, str]) -> int:
            try:
                step = subprocess.run(
                    command, env=env, capture_output=True, text=True, timeout=300
                )
                output, code = step.stdout + step.stderr, step.returncode
            except subprocess.TimeoutExpired as expired:
                output = f"TIMEOUT after 300s: {expired}"
                code = 1
            (evidence_root / f"{name}-stdout.log").write_text(output, encoding="utf-8")
            return code

        chain_code = run_step(
            "portal-chain",
            [
                sys.executable,
                "/opt/qindaqt-source/tests/session/gabbee/run_gabbee_portal_chain.py",
                "--run-root", str(runtime / "portal-chain"),
                "--existing-bus-address", environment["DBUS_SESSION_BUS_ADDRESS"],
                "--gabbee-root", str(arguments.gabbee_root),
                "--gabbee-venv-python", environment["GABBEE_VENV_PYTHON"],
                "--source-root", "/opt/qindaqt-source",
                "--result-path", str(evidence_root / "gabbee-portal-chain.json"),
            ],
            chain_environment,
        )

        time.sleep(5)  # let editor/terminal windows map before probing
        probe_code = run_step(
            "probe",
            [
                environment["GABBEE_VENV_PYTHON"],
                "/opt/qindaqt-source/tests/session/gabbee/gabbee_interop_probe.py",
                "--result-path", str(arguments.result_path),
                "--run-id", environment.get("QINDAQT_SESSION_RUN_ID", "nested"),
            ],
            probe_environment,
        )
        exit_code = 0 if chain_code == 0 and probe_code == 0 else 1
    finally:
        for process in reversed(state.processes):
            process.terminate()
        for process in state.processes:
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
    return exit_code


def main() -> int:
    if "--inner" in sys.argv:
        return _inner()
    return _outer()


if __name__ == "__main__":
    raise SystemExit(main())
