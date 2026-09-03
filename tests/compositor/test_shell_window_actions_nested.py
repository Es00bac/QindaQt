# SPDX-License-Identifier: GPL-3.0-or-later
"""Run the authenticated shell window-action proof in private virtual KWin."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path


MARKER = "QINDAQT_SHELL_WINDOW_ACTIONS_LIVE="


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("launcher", type=Path)
    parser.add_argument("probe", type=Path)
    parser.add_argument("dbus_runner", type=Path)
    parser.add_argument("kwin", type=Path)
    parser.add_argument("--plugin-root", type=Path, required=True)
    parser.add_argument("--scratch-root", type=Path, required=True)
    return parser.parse_args()


def private_environment(root: Path, kwin: Path) -> dict[str, str]:
    environment = dict(os.environ)
    for key in ("DBUS_SESSION_BUS_ADDRESS", "DISPLAY", "WAYLAND_DISPLAY"):
        environment.pop(key, None)
    for name in ("config", "data", "cache", "state"):
        (root / name).mkdir(parents=True)
    runtime = root / "runtime"
    runtime.mkdir(mode=0o700)
    private_usr = kwin.parent.parent
    private_library = private_usr / "lib"
    private_plugins = private_library / "qt6" / "plugins"
    inherited_libraries = environment.get("LD_LIBRARY_PATH", "")
    inherited_plugins = environment.get("QT_PLUGIN_PATH", "")
    environment.update(
        {
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
            "LD_LIBRARY_PATH": ":".join(
                value for value in (str(private_library), inherited_libraries) if value
            ),
            "QT_PLUGIN_PATH": ":".join(
                value for value in (str(private_plugins), inherited_plugins) if value
            ),
            "XDG_DATA_DIRS": f"{private_usr / 'share'}:/usr/share",
        }
    )
    return environment


def main() -> int:
    arguments = parse_arguments()
    for path, description in (
        (arguments.launcher, "qindaqt-wm"),
        (arguments.probe, "window-action probe"),
        (arguments.dbus_runner, "dbus-run-session"),
        (arguments.kwin, "pinned KWin 6.6.5"),
    ):
        if not path.is_file():
            print(f"{description} unavailable: {path}", file=sys.stderr)
            return 2
    if not arguments.plugin_root.is_dir():
        print(f"plugin root unavailable: {arguments.plugin_root}", file=sys.stderr)
        return 2

    # Wayland's sockaddr path is capped at 108 bytes; the lane build root is
    # isolated already, so keep this deterministic suffix deliberately short.
    scratch = arguments.scratch_root / "swa"
    shutil.rmtree(scratch, ignore_errors=True)
    scratch.mkdir(parents=True)
    command = [
        str(arguments.dbus_runner),
        "--",
        str(arguments.launcher),
        "--plugin-root",
        str(arguments.plugin_root),
        "--kwin",
        str(arguments.kwin),
        "--virtual",
        "--width",
        "1280",
        "--height",
        "720",
        "--scale",
        "1",
        "--output-count",
        "1",
        "--no-lockscreen",
        "--no-global-shortcuts",
        "--session",
        str(arguments.probe),
    ]
    try:
        completed = subprocess.run(
            command,
            env=private_environment(scratch, arguments.kwin),
            text=True,
            capture_output=True,
            timeout=30,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        print(f"nested action proof timed out: {error}", file=sys.stderr)
        return 1
    if completed.returncode != 0:
        print(completed.stdout, file=sys.stderr)
        print(completed.stderr, file=sys.stderr)
        return completed.returncode or 1
    markers = [
        line.removeprefix(MARKER)
        for line in completed.stdout.splitlines()
        if line.startswith(MARKER)
    ]
    if len(markers) != 1:
        print(f"expected one live marker, observed {len(markers)}", file=sys.stderr)
        print(completed.stdout, file=sys.stderr)
        return 1
    try:
        result = json.loads(markers[0])
    except json.JSONDecodeError as error:
        print(f"invalid live marker: {error}", file=sys.stderr)
        return 1
    if result.get("passed") is not True:
        print(f"live proof failed: {result.get('failure', '')}", file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
