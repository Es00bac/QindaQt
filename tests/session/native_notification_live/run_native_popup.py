#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual production shell mapping and native privacy on an isolated broker."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from nested_session_scenario import isolated_environment, running_private_session_bus
from notification_live_process import run_private_process_group, terminate


def await_compositor_owner(environment: dict[str, str], child: subprocess.Popen) -> None:
    """A listening socket precedes plugin/name readiness; await actual daemon PID."""
    deadline = time.monotonic() + 15
    while child.poll() is None and time.monotonic() < deadline:
        result = subprocess.run(["/usr/bin/busctl", "--address=" + environment["DBUS_SESSION_BUS_ADDRESS"],
            "--timeout=1", "call", "org.freedesktop.DBus", "/org/freedesktop/DBus",
            "org.freedesktop.DBus", "GetConnectionUnixProcessID", "s", "org.qindaqt.Compositor"],
            env=environment, capture_output=True, text=True, timeout=2, check=False)
        if result.returncode == 0 and result.stdout.strip() == f"u {child.pid}":
            return
        time.sleep(.05)
    raise RuntimeError(f"actual compositor bus owner was not ready (exit={child.poll()})")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("probe", "compositor", "launcher", "plugin-root", "shell", "host", "settings", "scenario", "artifacts"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--dbus-daemon", type=Path, default=Path("/usr/bin/dbus-daemon"))
    arguments = parser.parse_args()
    for name in ("probe", "compositor", "launcher", "shell", "host", "settings", "scenario", "dbus_daemon"):
        if not getattr(arguments, name).is_file():
            raise RuntimeError(f"required exact artifact absent: {name}")
    # Qualified fork690 is built with KDE_INSTALL_FULL_BINDIR=/usr/bin.
    # This fixed guard cannot be redirected to a convenient absent file.
    locker = Path("/usr/bin/qindaqt-lock")
    if locker.exists():
        raise RuntimeError("refused real native locker/PAM executable; this row requires clientless black fallback")
    plugin = arguments.plugin_root / "qindaqt-kwin" / "plugins" / "qindaqt_compositor.so"
    if not plugin.is_file():
        raise RuntimeError("compatible production compositor plugin absent")
    artifacts = arguments.artifacts.resolve()
    artifacts.mkdir(parents=True, exist_ok=True)
    # Preserve every invocation without overwriting earlier failures.
    # AF_UNIX has a small pathname limit. Logs live in the build tree, while
    # the disposable transport root uses the system's short temporary path.
    with tempfile.TemporaryDirectory(prefix="qindaqt-native-popup-") as tmp:
        root = Path(tmp)
        log_destination = artifacts / (root.name + "-evidence")
        environment = isolated_environment(root)
        for key in ("WAYLAND_SOCKET", "DBUS_SYSTEM_BUS_ADDRESS", "QT_PLUGIN_PATH", "LD_LIBRARY_PATH", "QT_FATAL_WARNINGS", "QT_QPA_PLATFORMTHEME"):
            environment.pop(key, None)
        environment.update(DBUS_SYSTEM_BUS_ADDRESS="unix:path=" + str(root / "unavailable-system-bus"),
            QINDAQT_NATIVE_POPUP_PRIVATE_BUS="1", QINDAQT_DEVELOPMENT_CONTROL="1",
            QINDAQT_OSK_DESKTOP_FILE=str(root / "unavailable-osk.desktop"),
            QINDAQT_TEST_SCENARIO=str(arguments.scenario.resolve()))
        fork_prefix = arguments.compositor.resolve().parent.parent
        environment["LD_LIBRARY_PATH"] = str(fork_prefix / "lib64")
        environment["QT_PLUGIN_PATH"] = str(arguments.plugin_root.resolve()) + os.pathsep + str(fork_prefix / "lib64/qt6/plugins")
        child = None
        try:
            with running_private_session_bus(root, arguments.dbus_daemon, environment):
                with (root / "compositor.log").open("w") as log:
                    child = subprocess.Popen([str(arguments.launcher.resolve()), "--kwin", str(arguments.compositor.resolve()),
                        "--virtual", "--width", "1920", "--height", "1080", "--scale", "1", "--output-count", "1",
                        "--socket", "qindaqt-popup", "--no-global-shortcuts", "--test-scenario", str(arguments.scenario.resolve()),
                        "--plugin-root", str(arguments.plugin_root.resolve())], env=environment, stdout=log, stderr=subprocess.STDOUT)
                deadline = time.monotonic() + 20
                while not (root / "runtime/qindaqt-popup").exists() and child.poll() is None and time.monotonic() < deadline:
                    time.sleep(.05)
                if not (root / "runtime/qindaqt-popup").exists():
                    raise RuntimeError("actual ordinary compositor socket did not appear")
                await_compositor_owner(environment, child)
                environment.update(WAYLAND_DISPLAY="qindaqt-popup", QINDAQT_NATIVE_POPUP_COMPOSITOR_PID=str(child.pid),
                    QINDAQT_NATIVE_POPUP_LOG_ROOT=str(root), QINDAQT_NATIVE_POPUP_LOCKER=str(locker),
                    QINDAQT_NATIVE_POPUP_SHELL=str(arguments.shell.resolve()), QINDAQT_NATIVE_POPUP_HOST=str(arguments.host.resolve()),
                    QINDAQT_NATIVE_POPUP_SETTINGS=str(arguments.settings.resolve()), QT_FATAL_WARNINGS="1")
                result = run_private_process_group([str(arguments.probe.resolve())], environment, 75)
                (root / "fixture.log").write_text(result.stdout + result.stderr)
                print(result.stdout, end="")
                print(result.stderr, end="", file=sys.stderr)
                return result.returncode
        except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
            (root / "runner-error.log").write_text(str(error))
            raise
        finally:
            terminate(child)
            log_destination.mkdir()
            for log in root.glob("*.log"):
                shutil.copy2(log, log_destination / log.name)
            print(f"native popup evidence: {log_destination}", file=sys.stderr)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"native popup qualification failed: {error}", file=sys.stderr)
        sys.exit(1)
