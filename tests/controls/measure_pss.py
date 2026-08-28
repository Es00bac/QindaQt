#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Measure, but never threshold, QindaQt.Controls resident PSS overhead."""

from __future__ import annotations

import argparse
import json
import os
import selectors
import statistics
import subprocess
import time
from pathlib import Path


def read_pss_kib(pid: int) -> int:
    rollup = Path(f"/proc/{pid}/smaps_rollup")
    for line in rollup.read_text(encoding="utf-8").splitlines():
        if line.startswith("Pss:"):
            return int(line.split()[1])
    raise RuntimeError(f"Pss not found in {rollup}")


def measure(executable: Path) -> int:
    environment = os.environ.copy()
    environment["QT_QPA_PLATFORM"] = "offscreen"
    environment["QT_QUICK_BACKEND"] = "software"
    process = subprocess.Popen(
        [str(executable)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        env=environment,
    )
    try:
        selector = selectors.DefaultSelector()
        assert process.stdout is not None
        selector.register(process.stdout, selectors.EVENT_READ)
        if not selector.select(timeout=8):
            raise RuntimeError(f"{executable.name} did not become ready")
        line = process.stdout.readline().strip()
        expected = f"READY {process.pid}"
        if line != expected:
            stderr = process.stderr.read() if process.stderr else ""
            raise RuntimeError(f"unexpected probe output {line!r}; stderr={stderr!r}")
        samples = []
        for _ in range(5):
            samples.append(read_pss_kib(process.pid))
            time.sleep(0.05)
        return int(statistics.median(samples))
    finally:
        process.terminate()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=3)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bare", type=Path, required=True)
    parser.add_argument("--controls", type=Path, required=True)
    arguments = parser.parse_args()

    rows = []
    for _ in range(3):
        bare = measure(arguments.bare)
        controls = measure(arguments.controls)
        rows.append({"bare_kib": bare, "controls_kib": controls, "delta_kib": controls - bare})

    result = {
        "schema": 1,
        "unit": "KiB PSS",
        "samples": rows,
        "median_bare_kib": int(statistics.median(row["bare_kib"] for row in rows)),
        "median_controls_kib": int(statistics.median(row["controls_kib"] for row in rows)),
        "median_delta_kib": int(statistics.median(row["delta_kib"] for row in rows)),
        "threshold": None,
    }
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
