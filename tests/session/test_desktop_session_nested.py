# SPDX-License-Identifier: GPL-3.0-or-later
"""Build or execute the contained 1920x1080 QindaQt desktop boot row."""

from __future__ import annotations

import argparse
import json
import os
import secrets
import shutil
import subprocess
import sys
from pathlib import Path, PurePosixPath
from typing import Any

from desktop_session_process import capture_process_identity, terminate_processes
from desktop_session_runtime import run_inner
from desktop_session_sandbox import (
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


SKIP_CODE = 77
LANE_ENVIRONMENT = "QINDAQT_PRIVATE_RUNTIME_LANE"
LANE_VALUE = "interactive-virtual-desktop"


def _tool_root(executable: Path) -> Path:
    resolved = executable.resolve(strict=True)
    parts = resolved.parts
    if ".linuxbrew" in parts:
        index = parts.index(".linuxbrew")
        return Path(*parts[: index + 1])
    if len(parts) >= 3 and parts[1] == "usr":
        return Path("/usr")
    return resolved.parent.parent


def _system_mounts(tools: list[Path]) -> tuple[ReadOnlyMount, ...]:
    roots = sorted({_tool_root(tool) for tool in tools}, key=str)
    return tuple(ReadOnlyMount(root, PurePosixPath(str(root))) for root in roots)


def _sandbox_path_for(source: Path, mounts: tuple[ReadOnlyMount, ...]) -> str:
    resolved = source.resolve(strict=True)
    for mount in mounts:
        root = mount.source.resolve(strict=True)
        if resolved == root or root in resolved.parents:
            return str(mount.destination / resolved.relative_to(root))
    raise SandboxContractError(f"tool is not covered by a system mount: {source}")


def _make_spec(arguments: argparse.Namespace, run_id: str, paths: Any) -> SandboxSpec:
    tools = [arguments.python, arguments.dbus_daemon, arguments.kwin_wayland]
    mounts = _system_mounts(tools)
    python = _sandbox_path_for(arguments.python, mounts)
    dbus_daemon = _sandbox_path_for(arguments.dbus_daemon, mounts)
    kwin_wayland = _sandbox_path_for(arguments.kwin_wayland, mounts)
    system_path = sorted(
        {str(PurePosixPath(_sandbox_path_for(tool, mounts)).parent) for tool in tools}
    )
    environment = sandbox_environment(
        run_id=run_id,
        uid=os.getuid(),
        stage_bin=f"/opt/qindaqt/{arguments.bin_directory}",
        system_path=system_path,
    )
    command = (
        python,
        "/opt/qindaqt-source/tests/session/test_desktop_session_nested.py",
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
        "--probe",
        "/opt/qindaqt-tools/qindaqt-desktop-session-probe",
        "--dbus-daemon",
        dbus_daemon,
        "--kwin-wayland",
        kwin_wayland,
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
            PurePosixPath("/opt/qindaqt-tools/qindaqt-desktop-session-probe"),
        ),
        system_mounts=mounts,
        environment=environment,
        command=command,
    )


def _copy_artifacts(paths: Any, build_root: Path, output: str) -> None:
    destination = build_root / "tests/session/desktop-session-last"
    destination.mkdir(parents=True, exist_ok=True)
    (destination / "sandbox.log").write_text(output, encoding="utf-8")
    for source in paths.artifacts.glob("*"):
        if source.is_file() and not source.is_symlink():
            shutil.copy2(source, destination / source.name)


def run_outer(arguments: argparse.Namespace) -> int:
    if not arguments.build_root or not arguments.source_root or not arguments.bwrap:
        raise SandboxContractError("outer mode requires build/source roots and bubblewrap")
    run_id = arguments.run_id or secrets.token_hex(16)
    paths = create_run_root(arguments.build_root, run_id)
    try:
        spec = _make_spec(arguments, run_id, paths)
        write_command_evidence(paths.artifacts / "sandbox-command.json", spec)
        if arguments.print_command_json:
            print(json.dumps({"argv": build_bwrap_argv(spec)}, sort_keys=True))
            return 0
        if os.environ.get(LANE_ENVIRONMENT) != LANE_VALUE:
            print(
                f"desktop runtime skipped: {LANE_ENVIRONMENT}={LANE_VALUE!r} is required",
                file=sys.stderr,
            )
            return SKIP_CODE
        with PrivateLaneLock():
            process = subprocess.Popen(
                build_bwrap_argv(spec),
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                start_new_session=True,
            )
            identity = capture_process_identity("sandbox", process.pid, [arguments.bwrap])
            try:
                output, _ = process.communicate(timeout=55)
            except subprocess.TimeoutExpired:
                terminate_processes([identity])
                output, _ = process.communicate(timeout=2)
                output += "\nsandbox exceeded its 55 second deadline\n"
                result = 1
            else:
                result = process.returncode
            _copy_artifacts(paths, arguments.build_root, output)
            if output:
                print(output, end="" if output.endswith("\n") else "\n")
            return result
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
    parser.add_argument("--run-id", default="")
    parser.add_argument("--print-command-json", action="store_true")
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        return run_outer(arguments) if arguments.outer else run_inner(arguments)
    except (
        json.JSONDecodeError,
        OSError,
        RuntimeError,
        subprocess.SubprocessError,
        ValueError,
    ) as error:
        print(f"desktop session qualification failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
