# SPDX-License-Identifier: GPL-3.0-or-later
"""CLI adapter for the source-shape checker."""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import asdict
from pathlib import Path
from typing import Sequence

from .checker import check_repository
from .config import ConfigurationError, load_config


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CONFIG = REPOSITORY_ROOT / "tools" / "source-shape.json"


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Reject source files that need modular decomposition.")
    parser.add_argument("--root", type=Path, default=REPOSITORY_ROOT)
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG)
    parser.add_argument("--largest", type=int, default=10, help="number of largest files to report")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--warnings-as-errors", action="store_true")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = _parser().parse_args(argv)
    if arguments.largest < 0:
        print("check-source-shape: error: --largest cannot be negative", file=sys.stderr)
        return 2
    try:
        config = load_config(arguments.config.resolve())
        report = check_repository(arguments.root, config, arguments.largest)
    except ConfigurationError as error:
        print(f"check-source-shape: error: {error}", file=sys.stderr)
        return 2
    if arguments.json:
        payload = {
            "checked_files": report.checked_files,
            "skipped_files": report.skipped_files,
            "issues": [issue.as_dict() for issue in report.issues],
            "largest_files": [asdict(shape) for shape in report.largest_files],
        }
        print(json.dumps(payload, indent=2, sort_keys=True))
    else:
        for issue in report.issues:
            print(f"{issue.severity.upper()}: {issue.path}: {issue.rule}: {issue.message}")
        print(f"Checked {report.checked_files} source files; skipped {report.skipped_files} allowlisted files.")
        if report.largest_files:
            print("Largest hand-written sources (non-blank lines):")
            for shape in report.largest_files:
                print(f"  {shape.nonblank_lines:>4}  {shape.path}")
    failed = bool(report.errors) or (arguments.warnings_as_errors and bool(report.issues))
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
