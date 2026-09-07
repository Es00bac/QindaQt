# SPDX-License-Identifier: GPL-3.0-or-later
"""Outer bubblewrap lane driver for the Terminal PTY proof.

Owns only the host side of the run: the private-runtime lane
acknowledgement, the cross-worktree session lock, staging the Terminal
component into a private prefix, the sandbox specification (including the
parent Weston prefix and the read-only Gabbee checkout the real sink is
imported from), and evidence collection. It never speaks to the portal or
the Terminal itself; that is the inner session's work.

AGENT-CONTRACT: no microphone, no typing tool, no uinput, no host input
injection, no host D-Bus or Wayland socket. Every process lives inside the
bubblewrap PID/network namespace; the only host-visible effect is the
locked, per-user private-session lane.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import uuid
from pathlib import Path

HERE = Path(__file__).resolve().parent

LANE_ENVIRONMENT = "QINDAQT_PRIVATE_RUNTIME_LANE"
LANE_VALUE = "interactive-virtual-desktop"


def outer_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="outer Gabbee Terminal PTY proof lane driver")
    parser.add_argument("--build-root", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, default=HERE.parents[2])
    parser.add_argument("--bwrap", default=shutil.which("bwrap"))
    parser.add_argument("--python", default=sys.executable)
    parser.add_argument("--dbus-daemon", default=shutil.which("dbus-daemon"))
    parser.add_argument("--kwin-wayland", required=True)
    # AGENT-CONTRACT: the real portal path needs a parent Wayland compositor
    # (see gabbee_terminal_boot). This is the same private Weston prefix the
    # accepted interactive audit configuration uses.
    parser.add_argument(
        "--weston",
        type=Path,
        default=Path("/home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-arch-665/root/usr/bin/weston"),
    )
    parser.add_argument("--gabbee-root", type=Path, required=True)
    parser.add_argument("--bin-directory", "--bin-dir", dest="bin_directory", default="bin")
    parser.add_argument("--plugin-relative", required=True)
    parser.add_argument("--decoration-relative", required=True)
    parser.add_argument("--settings-service-directory", default="share/dbus-1/services")
    parser.add_argument("--audio-service-directory", default="share/dbus-1/services")
    parser.add_argument("--result-root", type=Path, default=None)
    return parser


def run_outer() -> int:
    if os.environ.get(LANE_ENVIRONMENT) != LANE_VALUE:
        print(
            f"skip: set {LANE_ENVIRONMENT}={LANE_VALUE} after the manager allocates "
            "the private-runtime lane",
            file=sys.stderr,
        )
        return 77

    arguments = outer_parser().parse_args()

    sys.path.insert(0, str(HERE.parent))
    from desktop_session_sandbox import PrivateLaneLock, build_bwrap_argv, create_run_root, remove_run_root

    gabbee_root = arguments.gabbee_root.resolve(strict=True)
    if not (gabbee_root / "src/gabbee/agent_input.py").is_file():
        raise SystemExit(
            f"Gabbee source root is unusable (no src/gabbee/agent_input.py): {gabbee_root}"
        )

    stage_root = arguments.build_root / "tests/session/desktop-session-stage"
    if not stage_root.is_dir():
        raise SystemExit(
            f"desktop stage is missing: {stage_root} — run "
            f"ctest --test-dir {arguments.build_root} --fixtures-setup desktop_virtual_stage first"
        )

    result_root = (arguments.result_root or arguments.build_root / "tests/session/gabbee-results").resolve()
    result_root.mkdir(parents=True, exist_ok=True)
    run_id = uuid.uuid4().hex
    run_dir = result_root / run_id
    run_dir.mkdir()
    terminal_stage = run_dir / "terminal-stage"
    install = subprocess.run(
        ["cmake", "--install", str(arguments.build_root), "--component", "Terminal",
         "--prefix", str(terminal_stage)],
        capture_output=True, text=True,
    )
    if install.returncode != 0:
        raise SystemExit(f"Terminal component install failed: {install.stderr[-2000:]}")

    with PrivateLaneLock():
        paths = create_run_root(arguments.build_root, run_id)
        try:
            spec = outer_spec(arguments, gabbee_root, terminal_stage, paths, run_id)
            command = with_render_node(build_bwrap_argv(spec))
            (run_dir / "bwrap-argv.json").write_text(json.dumps(command, indent=2), "utf-8")
            completed = subprocess.run(command, capture_output=True, text=True, timeout=600)
            (run_dir / "inner-stdout.log").write_text(completed.stdout, "utf-8")
            (run_dir / "inner-stderr.log").write_text(completed.stderr, "utf-8")
            for item in paths.artifacts.glob("*"):
                shutil.copy2(item, run_dir / item.name)
            print(f"runId={run_id}", file=sys.stderr)
            print(f"runDir={run_dir}", file=sys.stderr)
            return completed.returncode
        finally:
            remove_run_root(paths, arguments.build_root, run_id)


def with_render_node(argv: list[str]) -> list[str]:
    """Add the host's GPU render node on top of the shared sandbox's bare ``/dev``.

    AGENT-CONTRACT: desktop_session_sandbox.py (shared by every other private
    session test) deliberately gives no GPU device. Binding the render-only
    node (no KMS/display control) is scoped to this script's own bwrap
    invocation and never to the shared sandbox module other tests use. It is
    a defensive addition: the proof passes on the software pixman parent, so
    absence of the node is not treated as fatal.
    """

    render_node = Path("/dev/dri/renderD128")
    if not render_node.exists():
        return argv
    augmented = list(argv)
    try:
        index = augmented.index("--dev") + 2
    except ValueError:
        return argv
    augmented[index:index] = ["--dev-bind", str(render_node), str(render_node)]
    return augmented


def outer_spec(
    arguments: argparse.Namespace, gabbee_root: Path, terminal_stage: Path, paths, run_id: str
):
    from desktop_session_host_tools import (
        library_search_roots, sandbox_path_for, system_mounts, weston_module_map,
    )
    from desktop_session_sandbox import ReadOnlyMount, SandboxSpec, sandbox_environment
    from pathlib import PurePosixPath

    def posix(value: str) -> PurePosixPath:
        return PurePosixPath(value)

    tools = [
        Path(arguments.python).resolve(strict=True),
        Path(arguments.dbus_daemon).resolve(strict=True),
        Path(arguments.kwin_wayland).resolve(strict=True),
    ]
    weston_host = Path(arguments.weston).resolve(strict=True)
    # AGENT-NOTE: the whole real portal stack (pipewire, xdg-desktop-portal,
    # xdg-desktop-portal-kde, qindaqt-agent-input, gdbus, at-spi-bus-launcher)
    # lives under /usr on this lane, so mounting the python/dbus/kwin tool
    # prefixes already exposes every binary this proof needs.
    mounts = list(system_mounts(tools + [weston_host]))
    mounts.append(ReadOnlyMount(terminal_stage.resolve(strict=True), posix("/opt/qindaqt-terminal")))
    # The real Gabbee sink is imported from this read-only mount; see
    # gabbee_terminal_sink.import_real_sink.
    mounts.append(ReadOnlyMount(gabbee_root, posix("/opt/gabbee")))
    python = sandbox_path_for(tools[0], tuple(mounts))
    dbus_daemon = sandbox_path_for(tools[1], tuple(mounts))
    kwin_wayland = sandbox_path_for(tools[2], tuple(mounts))
    weston = sandbox_path_for(weston_host, tuple(mounts))
    system_path = sorted(
        {str(posix(entry).parent) for entry in (python, dbus_daemon, kwin_wayland, weston)}
    )
    # AGENT-GUARD: only the compositor's own prefix may shape the sandbox-wide
    # library/plugin search paths. Exporting the parent Weston prefix globally
    # makes the system KWin load a private-prefix libkwin and reject the
    # release-matched plugin ("mismatching plugin version"); Weston gets its
    # libraries through LD_LIBRARY_PATH in its own environment only.
    library_path, qt_plugin_path, qml_import_path = library_search_roots(tools)
    environment = sandbox_environment(
        run_id=run_id, uid=os.getuid(), stage_bin=f"/opt/qindaqt/{arguments.bin_directory}",
        system_path=system_path, library_path=library_path,
        qt_plugin_path=qt_plugin_path, qml_import_path=qml_import_path,
    )
    terminal_bin = "/opt/qindaqt-terminal/" + arguments.bin_directory
    environment["PATH"] = ":".join([terminal_bin, environment["PATH"]])
    environment["QT_LINUX_ACCESSIBILITY_ALWAYS_ON"] = "1"
    # AGENT-NOTE: gabbee.agent_input imports only gabbee.models, which is
    # stdlib-only, so the sink needs the checkout's src on PYTHONPATH and
    # nothing else — no PyQt6, dbus-python, or gi inside this sandbox.
    environment["PYTHONPATH"] = "/opt/gabbee/src"
    # Weston resolves its relocatable backend/shell modules through this exact
    # map rather than compiled-in /usr module directories.
    environment["WESTON_MODULE_MAP"] = weston_module_map(weston_host)
    command = (
        python,
        "/opt/qindaqt-source/tests/session/gabbee/run_gabbee_terminal_pty_proof.py",
        "--inner",
        "--weston", weston,
        "--stage-root", "/opt/qindaqt",
        "--bin-directory", arguments.bin_directory,
        "--plugin-relative", arguments.plugin_relative,
        "--decoration-relative", arguments.decoration_relative,
        "--settings-service-directory", arguments.settings_service_directory,
        "--audio-service-directory", arguments.audio_service_directory,
        "--dbus-daemon", dbus_daemon,
        "--kwin-wayland", kwin_wayland,
        "--result-path", "/var/lib/qindaqt-evidence/gabbee-terminal-pty-result.json",
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
        tests=ReadOnlyMount(arguments.source_root.resolve(strict=True), posix("/opt/qindaqt-source")),
        # AGENT-NOTE: SandboxSpec requires a probe mount; this proof invokes no
        # separate probe process, but the file is already reachable read-only
        # inside /opt/qindaqt-source, so re-mounting it here is a harmless,
        # schema-satisfying duplicate rather than new surface area.
        probe=ReadOnlyMount(
            (HERE / "gabbee_interop_probe.py").resolve(strict=True),
            posix("/opt/qindaqt-tools/gabbee_interop_probe.py"),
        ),
        system_mounts=tuple(mounts),
        environment=environment,
        command=command,
    )
