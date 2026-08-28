# SPDX-License-Identifier: GPL-3.0-or-later
"""Stage and authenticate every production artifact needed by desktop S1."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from desktop_session_stage import (
    StageContractError,
    install_stage,
    resolve_stage,
    write_stage_evidence,
)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cmake", type=Path, required=True)
    parser.add_argument("--build-root", type=Path, required=True)
    parser.add_argument("--stage-root", type=Path, required=True)
    parser.add_argument("--bin-directory", required=True)
    parser.add_argument("--plugin-relative", required=True)
    parser.add_argument("--decoration-relative", required=True)
    parser.add_argument("--settings-service-directory", required=True)
    parser.add_argument("--audio-service-directory", required=True)
    parser.add_argument("--configuration", default="")
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        stage_root = install_stage(
            arguments.cmake,
            arguments.build_root,
            arguments.stage_root,
            configuration=arguments.configuration,
            component="DesktopVirtual",
        )
        stage = resolve_stage(
            stage_root,
            bin_directory=arguments.bin_directory,
            plugin_relative=arguments.plugin_relative,
            decoration_relative=arguments.decoration_relative,
            settings_service_directory=arguments.settings_service_directory,
            audio_service_directory=arguments.audio_service_directory,
        )
        write_stage_evidence(
            arguments.build_root
            / "tests/session/desktop-session-package-evidence.json",
            stage,
        )
    except (OSError, StageContractError) as error:
        print(f"desktop session package contract failed: {error}", file=sys.stderr)
        return 1
    print("desktop session package contract passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
