#!/usr/bin/env python3
"""Verify every script-driven editor Qt row makes warnings fatal."""

from __future__ import annotations

import argparse
import json
import subprocess


REQUIRED_ROWS = {
    "qindaqt.editor-cli-multiple-paths",
    "qindaqt.editor-cli-hostile-argv",
    "qindaqt.editor-installed-metadata-and-runtime",
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ctest", required=True)
    parser.add_argument("--build-directory", required=True)
    arguments = parser.parse_args()
    completed = subprocess.run(
        [
            arguments.ctest,
            "--test-dir",
            arguments.build_directory,
            "--show-only=json-v1",
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    registry = json.loads(completed.stdout)
    rows = {test["name"]: test for test in registry["tests"]}
    for name in sorted(REQUIRED_ROWS):
        if name not in rows:
            raise SystemExit(f"missing registered editor row: {name}")
        properties = {
            prop["name"]: prop["value"]
            for prop in rows[name].get("properties", [])
        }
        environment = properties.get("ENVIRONMENT", [])
        if "QT_FATAL_WARNINGS=1" not in environment:
            raise SystemExit(f"{name} does not register QT_FATAL_WARNINGS=1")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
