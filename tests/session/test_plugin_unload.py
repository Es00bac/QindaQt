# SPDX-License-Identifier: GPL-3.0-or-later
"""Prove runtime plugin unload restores grouped clients before teardown."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


def isolated_environment(root: Path) -> dict[str, str]:
    environment = dict(os.environ)
    for key in ("DBUS_SESSION_BUS_ADDRESS", "DISPLAY", "WAYLAND_DISPLAY"):
        environment.pop(key, None)
    for name in ("home", "config", "data", "cache", "state"):
        (root / name).mkdir()
    runtime = root / "runtime"
    runtime.mkdir(mode=0o700)
    environment.update(
        {
            "HOME": str(root / "home"),
            "XDG_CONFIG_HOME": str(root / "config"),
            "XDG_DATA_HOME": str(root / "data"),
            "XDG_CACHE_HOME": str(root / "cache"),
            "XDG_STATE_HOME": str(root / "state"),
            "XDG_RUNTIME_DIR": str(runtime),
            "XDG_CURRENT_DESKTOP": "QindaQt",
            "XDG_SESSION_DESKTOP": "qindaqt",
            "KWIN_COMPOSE": "Q",
            "QT_QPA_PLATFORM": "wayland",
            "QT_QUICK_BACKEND": "software",
        }
    )
    return environment


def extract_result(stdout: str) -> dict[str, Any]:
    prefix = "QINDAQT_PLUGIN_UNLOAD="
    marker = next(
        (line.removeprefix(prefix) for line in stdout.splitlines() if line.startswith(prefix)),
        None,
    )
    if marker is None:
        raise RuntimeError("plugin-unload probe result marker was missing")
    result = json.loads(marker)
    if not isinstance(result, dict):
        raise RuntimeError("plugin-unload probe result must be an object")
    return result


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("launcher")
    parser.add_argument("probe")
    parser.add_argument("dbus_runner")
    parser.add_argument("scenario", type=Path)
    parser.add_argument("--plugin-root", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    with tempfile.TemporaryDirectory(prefix="qindaqt-plugin-unload-") as directory:
        environment = isolated_environment(Path(directory))
        command = [
            arguments.dbus_runner,
            "--",
            arguments.launcher,
            "--plugin-root",
            str(arguments.plugin_root),
            "--virtual",
            "--width",
            "1920",
            "--height",
            "1080",
            "--scale",
            "1",
            "--output-count",
            "1",
            "--test-scenario",
            str(arguments.scenario),
            "--no-xwayland",
            "--no-lockscreen",
            "--no-global-shortcuts",
            "--session",
            arguments.probe,
        ]
        completed = subprocess.run(
            command,
            env=environment,
            text=True,
            capture_output=True,
            timeout=20,
            check=False,
        )
    if completed.returncode != 0:
        print(completed.stdout, file=sys.stderr)
        print(completed.stderr, file=sys.stderr)
        return completed.returncode or 1
    try:
        result = extract_result(completed.stdout)
    except (RuntimeError, json.JSONDecodeError) as error:
        print(f"plugin-unload validation failed: {error}", file=sys.stderr)
        print(completed.stdout, file=sys.stderr)
        return 1
    required = (
        "grouped",
        "unloadCallSucceeded",
        "serviceRemoved",
        "pluginRemoved",
        "framesRestored",
        "clientsUsable",
    )
    if not all(result.get(field) is True for field in required):
        print(f"plugin-unload proof was incomplete: {json.dumps(result, sort_keys=True)}", file=sys.stderr)
        return 1
    if "QindaQt plugin unload could not release container" in completed.stderr:
        print("plugin unload logged a container release failure", file=sys.stderr)
        print(completed.stderr, file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
