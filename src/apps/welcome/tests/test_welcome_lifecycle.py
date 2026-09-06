# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the real Welcome QML root and its first-launch opt-out."""

from __future__ import annotations

import argparse
import os
import subprocess
import tempfile
from pathlib import Path


def run(welcome: Path, theme_directory: Path, config: Path,
        *arguments: str) -> subprocess.CompletedProcess[str]:
    environment = {
        "HOME": str(config.parent),
        "XDG_CONFIG_HOME": str(config),
        "XDG_CACHE_HOME": str(config.parent / "cache"),
        "XDG_DATA_HOME": str(config.parent / "data"),
        "PATH": os.environ.get("PATH", ""),
        "LANG": "C.UTF-8",
        "LC_ALL": "C.UTF-8",
        "QT_QPA_PLATFORM": "offscreen",
        "QT_QUICK_BACKEND": "software",
        "QT_NO_XDG_DESKTOP_PORTAL": "1",
        "DBUS_SESSION_BUS_ADDRESS": "unix:path=/nonexistent",
    }
    return subprocess.run(
        [str(welcome), "--theme-directory", str(theme_directory), *arguments],
        env=environment, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, timeout=10, check=False,
    )


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--welcome", type=Path, required=True)
    parser.add_argument("--theme-directory", type=Path, required=True)
    arguments = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="qindaqt-welcome-") as root:
        config = Path(root) / "config"
        first = run(arguments.welcome, arguments.theme_directory, config,
                    "--first-launch", "--check-ui-contract")
        require(first.returncode == 0 and "show-next=true" in first.stdout,
                f"fresh first launch did not open: {first.stderr}")

        opt_out = run(arguments.welcome, arguments.theme_directory, config,
                      "--first-launch", "--check-ui-contract", "--exercise-opt-out")
        require(opt_out.returncode == 0 and "show-next=false" in opt_out.stdout,
                f"checkbox did not persist opt-out: {opt_out.stderr}")

        later = run(arguments.welcome, arguments.theme_directory, config,
                    "--first-launch", "--check-ui-contract")
        require(later.returncode == 0 and "welcome-ui-ready" not in later.stdout,
                f"opted-out first-launch mode still opened: {later.stderr}")

        manual = run(arguments.welcome, arguments.theme_directory, config,
                     "--check-ui-contract")
        require(manual.returncode == 0 and "show-next=false" in manual.stdout,
                f"manual reopen was blocked by opt-out: {manual.stderr}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
