# SPDX-License-Identifier: GPL-3.0-or-later
"""Build or execute the contained 1920x1080 QindaQt desktop boot row."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import secrets
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Any

from desktop_session_process import capture_process_identity, terminate_processes
from desktop_session_host_tools import (
    library_search_roots,
    resolved_system_input,
    sandbox_path_for,
    system_mounts,
    weston_module_map,
)
from desktop_session_lifecycle import LIFECYCLE_TRACE_ENVIRONMENT
from desktop_session_runtime import run_inner
from desktop_session_sandbox import (
    PrivateLaneLock,
    ReadOnlyMount,
    SandboxContractError,
    SandboxSpec,
    build_bwrap_argv,
    authenticate_run_root,
    create_run_root,
    remove_run_root,
    sandbox_environment,
    write_command_evidence,
)


SKIP_CODE = 77
LANE_ENVIRONMENT = "QINDAQT_PRIVATE_RUNTIME_LANE"
LANE_VALUE = "interactive-virtual-desktop"


@dataclass(frozen=True)
class AttemptResult:
    outcome: str
    return_code: int | None
    timed_out: bool
    failure: str | None
    started_unix_ns: int
    finished_unix_ns: int

    def document(self, run_id: str) -> dict[str, object]:
        return {
            "schemaVersion": 1,
            "runId": run_id,
            "outcome": self.outcome,
            "returnCode": self.return_code,
            "timedOut": self.timed_out,
            "failure": self.failure,
            "startedUnixNs": self.started_unix_ns,
            "finishedUnixNs": self.finished_unix_ns,
        }


def _enable_lifecycle_diagnostics(environment: dict[str, str]) -> None:
    if os.environ.get(LIFECYCLE_TRACE_ENVIRONMENT) != "1":
        return
    environment[LIFECYCLE_TRACE_ENVIRONMENT] = "1"
    environment["QT_FORCE_STDERR_LOGGING"] = "1"


def _make_spec(arguments: argparse.Namespace, run_id: str, paths: Any) -> SandboxSpec:
    tools = [arguments.python, arguments.dbus_daemon, arguments.kwin_wayland]
    interactive = getattr(arguments, "interactive", False)
    secondary = interactive and arguments.scenario_id == "dual-1080p-horizontal"
    if interactive:
        tools.extend([arguments.weston, arguments.weston_screenshooter])
    kscreen_doctor: Path | None = None
    kscreen_backend: Path | None = None
    if secondary:
        kscreen_doctor = resolved_system_input(
            arguments.kscreen_doctor, "KScreen selector", executable=True
        )
        kscreen_backend = resolved_system_input(
            arguments.kscreen_wayland_backend,
            "KScreen Wayland backend",
            executable=False,
        )
        tools.append(kscreen_doctor)
    mounted_inputs = list(tools)
    if kscreen_backend is not None:
        mounted_inputs.append(kscreen_backend)
    mounts = system_mounts(mounted_inputs)
    python = sandbox_path_for(arguments.python, mounts)
    dbus_daemon = sandbox_path_for(arguments.dbus_daemon, mounts)
    kwin_wayland = sandbox_path_for(arguments.kwin_wayland, mounts)
    weston = (
        sandbox_path_for(arguments.weston, mounts) if interactive else ""
    )
    screenshooter = (
        sandbox_path_for(arguments.weston_screenshooter, mounts)
        if interactive else ""
    )
    system_path = sorted(
        {str(PurePosixPath(sandbox_path_for(tool, mounts)).parent) for tool in tools}
    )
    library_path, qt_plugin_path, qml_import_path = library_search_roots(
        mounted_inputs
    )
    environment = sandbox_environment(
        run_id=run_id,
        uid=os.getuid(),
        stage_bin=f"/opt/qindaqt/{arguments.bin_directory}",
        system_path=system_path,
        library_path=library_path,
        qt_plugin_path=qt_plugin_path,
        qml_import_path=qml_import_path,
    )
    if interactive:
        # Weston supports relocatable test packages through this exact module
        # map. Do not bind over its compiled /usr module directories.
        environment["WESTON_MODULE_MAP"] = weston_module_map(arguments.weston)
    if os.environ.get(LIFECYCLE_TRACE_ENVIRONMENT) == "1":
        # This exact opt-in exists only to diagnose the essential session child
        # boundary. It carries no host endpoint and is omitted from acceptance
        # rows after the lifecycle defect is identified.
        _enable_lifecycle_diagnostics(environment)
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
        "--scenario-id",
        getattr(arguments, "scenario_id", "single-1080p"),
    )
    if interactive:
        command += (
            "--interactive",
            "--weston", weston,
            "--weston-screenshooter", screenshooter,
        )
    if secondary:
        if kscreen_doctor is None or kscreen_backend is None:
            raise SandboxContractError("dual-output KScreen inputs were not resolved")
        command += (
            "--kscreen-doctor",
            sandbox_path_for(kscreen_doctor, mounts),
            "--kscreen-wayland-backend",
            sandbox_path_for(kscreen_backend, mounts),
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


def _result_sentinel(build_root: Path, run_id: str) -> str:
    digest = hashlib.sha256(str(build_root.resolve()).encode()).hexdigest()
    return f"qindaqt-desktop-result-v1\n{run_id}\n{digest}\n"


def create_result_root(build_root: Path, run_id: str) -> Path:
    """Create one fresh persistent result root; never reuse a prior attempt."""

    if len(run_id) != 32 or any(character not in "0123456789abcdef" for character in run_id):
        raise SandboxContractError("result run id must be 32 lowercase hexadecimal digits")
    build = build_root.resolve(strict=True)
    parent = build / "tests/session/desktop-session-results"
    parent.mkdir(parents=True, exist_ok=True)
    if parent.resolve(strict=True) != parent:
        raise SandboxContractError("result parent must not traverse a symlink")
    root = parent / run_id
    if root.exists() or root.is_symlink():
        raise SandboxContractError(f"result root already exists: {root}")
    root.mkdir(mode=0o700)
    (root / ".qindaqt-desktop-result").write_text(
        _result_sentinel(build, run_id), encoding="ascii"
    )
    (root / "artifacts").mkdir(mode=0o700)
    (root / "logs").mkdir(mode=0o700)
    return root


def _validate_result_root(root: Path, build_root: Path, run_id: str) -> None:
    build = build_root.resolve(strict=True)
    expected_parent = build / "tests/session/desktop-session-results"
    sentinel = root / ".qindaqt-desktop-result"
    if (
        root.parent != expected_parent
        or root.name != run_id
        or root.is_symlink()
        or sentinel.is_symlink()
    ):
        raise SandboxContractError("result root is outside its exact build/run identity")
    try:
        actual = sentinel.read_text(encoding="ascii")
    except OSError as error:
        raise SandboxContractError("result sentinel is missing") from error
    if actual != _result_sentinel(build, run_id):
        raise SandboxContractError("result sentinel does not match this attempt")
    for name in ("artifacts", "logs"):
        child = root / name
        if child.is_symlink() or not child.is_dir():
            raise SandboxContractError(f"result {name} destination is not authenticated")


def _copy_regular_files(source: Path, destination: Path) -> None:
    for item in sorted(source.iterdir(), key=lambda path: path.name):
        if item.is_symlink() or not item.is_file():
            raise SandboxContractError(f"attempt output is not a regular file: {item}")
        target = destination / item.name
        if target.exists() or target.is_symlink():
            raise SandboxContractError(f"fresh result destination was not empty: {target}")
        shutil.copy2(item, target)


def archive_attempt(
    paths: Any,
    result_root: Path,
    build_root: Path,
    run_id: str,
    output: str,
    result: AttemptResult,
) -> None:
    """Copy every artifact/log and exact result metadata before run-root deletion."""

    authenticate_run_root(paths, build_root, run_id)
    _validate_result_root(result_root, build_root, run_id)
    _copy_regular_files(paths.artifacts, result_root / "artifacts")
    _copy_regular_files(paths.logs, result_root / "logs")
    with (result_root / "sandbox.log").open("x", encoding="utf-8") as stream:
        stream.write(output)
    with (result_root / "result.json").open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(result.document(run_id), sort_keys=True, indent=2) + "\n")


def run_outer(arguments: argparse.Namespace) -> int:
    if not arguments.build_root or not arguments.source_root or not arguments.bwrap:
        raise SandboxContractError("outer mode requires build/source roots and bubblewrap")
    run_id = arguments.run_id or secrets.token_hex(16)
    paths = create_run_root(arguments.build_root, run_id)
    result_root: Path | None = None
    output = ""
    started = time.time_ns()
    result = AttemptResult("failure", None, False, "attempt did not complete", started, started)
    try:
        result_root = create_result_root(arguments.build_root, run_id)
        spec = _make_spec(arguments, run_id, paths)
        write_command_evidence(paths.artifacts / "sandbox-command.json", spec)
        if arguments.print_command_json:
            print(json.dumps({"argv": build_bwrap_argv(spec)}, sort_keys=True))
            result = AttemptResult("command-only", 0, False, None, started, time.time_ns())
            return 0
        if os.environ.get(LANE_ENVIRONMENT) != LANE_VALUE:
            print(
                f"desktop runtime skipped: {LANE_ENVIRONMENT}={LANE_VALUE!r} is required",
                file=sys.stderr,
            )
            result = AttemptResult("skipped", SKIP_CODE, False, None, started, time.time_ns())
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
                output, _ = process.communicate(timeout=70)
            except subprocess.TimeoutExpired:
                terminate_processes([identity])
                output, _ = process.communicate(timeout=2)
                output += "\nsandbox exceeded its 70 second deadline\n"
                return_code = 1
                timed_out = True
            else:
                return_code = process.returncode
                timed_out = False
            outcome = "success" if return_code == 0 else ("timeout" if timed_out else "failure")
            failure = None if return_code == 0 else (
                "sandbox exceeded its 70 second deadline"
                if timed_out else f"sandbox exited with status {return_code}"
            )
            result = AttemptResult(
                outcome, return_code, timed_out, failure, started, time.time_ns()
            )
            if output:
                print(output, end="" if output.endswith("\n") else "\n")
            return return_code
    except BaseException as error:
        result = AttemptResult(
            "failure", None, False, f"{type(error).__name__}: {error}",
            started, time.time_ns(),
        )
        raise
    finally:
        try:
            if result_root is not None:
                archive_attempt(
                    paths, result_root, arguments.build_root, run_id, output, result
                )
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
    parser.add_argument("--interactive", action="store_true")
    parser.add_argument("--scenario-id", default="single-1080p")
    parser.add_argument("--weston", type=Path)
    parser.add_argument("--weston-screenshooter", type=Path)
    parser.add_argument("--kscreen-doctor", type=Path)
    parser.add_argument("--kscreen-wayland-backend", type=Path)
    parser.add_argument("--run-id", default="")
    parser.add_argument("--print-command-json", action="store_true")
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        if arguments.interactive and (
            arguments.weston is None
            or arguments.weston_screenshooter is None
        ):
            raise SandboxContractError(
                "interactive mode requires Weston and weston-screenshooter"
            )
        if (
            arguments.interactive
            and arguments.scenario_id == "dual-1080p-horizontal"
            and (
                arguments.kscreen_doctor is None
                or arguments.kscreen_wayland_backend is None
            )
        ):
            raise SandboxContractError(
                "dual-output mode requires KScreen selector and Wayland backend"
            )
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
