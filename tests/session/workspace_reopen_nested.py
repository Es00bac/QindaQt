# SPDX-License-Identifier: GPL-3.0-or-later
"""Save a workspace in one nested compositor session and reopen it in a fresh one.

Both sessions share one disposable XDG root (so the saved document survives),
but each runs its own private D-Bus daemon and compositor process tree via
running_private_session_bus. Between the sessions the harness removes the
phantom fixture desktop entry from its own private applications directory, so
the second session's production Reopen dialog genuinely faces a missing
application.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any

from nested_session_scenario import (
    ScenarioCoverageError,
    isolated_environment,
    load_virtual_spec,
    running_private_session_bus,
    write_virtual_output_config,
)
from workspace_reopen_fixture import (
    PHANTOM_APP_ID,
    clear_ksycoca,
    extract_reopen_result,
    load_workspace_documents,
    nested_environment_variables,
    remove_fixture_application,
    validate_reopen_evidence,
    validate_save_evidence,
    write_fixture_applications,
)

SESSION_TIMEOUT_SECONDS = 120


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("launcher", help="qindaqt-wm executable (staged or build tree)")
    parser.add_argument("probe", help="qindaqt-workspace-reopen-probe executable")
    parser.add_argument("scenario", type=Path)
    parser.add_argument("--plugin-root", type=Path, required=True)
    return parser.parse_args()


def run_session(
    arguments: argparse.Namespace,
    environment: dict[str, str],
    root: Path,
    dbus_daemon: Path,
    phase: str,
    width: int,
    height: int,
) -> dict[str, Any]:
    session_environment = dict(environment)
    session_environment["QINDAQT_WREOPEN_PHASE"] = phase
    command = [
        arguments.launcher,
        "--plugin-root",
        str(arguments.plugin_root),
        "--virtual",
        "--width",
        str(width),
        "--height",
        str(height),
        "--scale",
        "1",
        "--output-count",
        "1",
        # The scenario marker enables the development control endpoint and its
        # gated input device; nothing here touches host input or host portals.
        "--test-scenario",
        str(arguments.scenario),
        "--no-lockscreen",
        # Global shortcuts stay enabled: Meta+Ctrl+W is a production KGlobalAccel
        # action owned by the nested KWin on the private bus.
        "--session",
        arguments.probe,
    ]
    with running_private_session_bus(root, dbus_daemon, session_environment):
        completed = subprocess.run(
            command,
            env=session_environment,
            text=True,
            capture_output=True,
            timeout=SESSION_TIMEOUT_SECONDS,
            check=False,
        )
    if completed.returncode != 0:
        print(completed.stdout, file=sys.stderr)
        print(completed.stderr, file=sys.stderr)
        raise RuntimeError(f"{phase} session exited with {completed.returncode}")
    return extract_reopen_result(completed.stdout, phase)


def main() -> int:
    arguments = parse_arguments()
    dbus_daemon = shutil.which("dbus-daemon")
    if not dbus_daemon:
        print("private nested run requires dbus-daemon", file=sys.stderr)
        return 2
    try:
        spec = load_virtual_spec(arguments.scenario)
    except ScenarioCoverageError as error:
        print(f"scenario is not representable: {error}", file=sys.stderr)
        return 2
    with tempfile.TemporaryDirectory(prefix="qindaqt-workspace-reopen-") as directory:
        root = Path(directory)
        environment = nested_environment_variables(isolated_environment(root))
        write_virtual_output_config(Path(environment["XDG_CONFIG_HOME"]), spec)
        write_fixture_applications(environment["XDG_DATA_HOME"])
        save_result = run_session(
            arguments, environment, root, Path(dbus_daemon), "save",
            spec.logical_width, spec.logical_height,
        )
        saved = load_workspace_documents(environment["XDG_DATA_HOME"])
        if len(saved) != 1:
            print(f"expected exactly one saved workspace, found {len(saved)}", file=sys.stderr)
            return 1
        validate_save_evidence(save_result, saved[0])

        remove_fixture_application(environment["XDG_DATA_HOME"], PHANTOM_APP_ID)
        clear_ksycoca(environment["XDG_CACHE_HOME"])

        reopen_result = run_session(
            arguments, environment, root, Path(dbus_daemon), "reopen",
            spec.logical_width, spec.logical_height,
        )
        updated = load_workspace_documents(environment["XDG_DATA_HOME"])
        if len(updated) != 1:
            print(f"expected the saved workspace to persist, found {len(updated)}", file=sys.stderr)
            return 1
        validate_reopen_evidence(reopen_result, save_result, updated[0])
    print(
        json.dumps(
            {
                "workspaceReopenTwoSession": True,
                "workspaceId": reopen_result["workspaceId"],
                "restoredContainerId": reopen_result["containerId"],
                "scenarioCoverage": spec.coverage,
            },
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
