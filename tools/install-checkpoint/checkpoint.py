#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Audit and stage the QindaQt Settings/audio/network install checkpoint."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys
from typing import Sequence

from checkpoint_audit import preflight_report
from checkpoint_contract import BUILD, ROOT, STAGE_ROOT
from checkpoint_stage import stage


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    preflight = subparsers.add_parser("preflight", help="read-only live audit and install plan")
    preflight.add_argument("--repo", type=Path, default=ROOT)
    preflight.add_argument("--build-dir", type=Path, default=BUILD)
    preflight.add_argument("--stage-root", type=Path, default=STAGE_ROOT)
    stage_parser = subparsers.add_parser("stage", help="build/install only beneath the ignored DESTDIR stage root")
    stage_parser.add_argument("--repo", type=Path, default=ROOT)
    stage_parser.add_argument("--build-dir", type=Path, default=BUILD)
    stage_parser.add_argument("--stage-root", type=Path, default=STAGE_ROOT)
    args = parser.parse_args(argv)
    try:
        if args.command == "preflight":
            report = preflight_report(args.repo.resolve(), args.build_dir.resolve(), args.stage_root.resolve())
            print(json.dumps(report, indent=2, sort_keys=True))
            return 0 if report["source"]["commit"] and report["makeopts"]["available"] else 2
        report = stage(args.repo.resolve(), args.build_dir.resolve(), args.stage_root.resolve())
        print(json.dumps(report, indent=2, sort_keys=True))
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"install-checkpoint: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
