# SPDX-License-Identifier: GPL-3.0-or-later
"""Check the production KWin TabBox package in the staged DesktopVirtual tree."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from desktop_session_package_contract import (
    PackagePayloadError,
    WINDOW_SWITCHER_FILES,
    authenticate_window_switcher_package,
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage-root", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        files = authenticate_window_switcher_package(arguments.stage_root)
    except (OSError, PackagePayloadError) as error:
        print(f"DesktopVirtual TabBox stage failed: {error}", file=sys.stderr)
        return 1
    if len(files) != len(WINDOW_SWITCHER_FILES):
        print("DesktopVirtual TabBox stage has an incomplete payload", file=sys.stderr)
        return 1
    print(f"DesktopVirtual TabBox stage passed: {len(files)} production files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
