#!/usr/bin/env python3
"""Measure only an installed offscreen editor process; never attach to host UI."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import select
import statistics
import subprocess
import tempfile
import time


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--data-dir", required=True, type=Path)
    parser.add_argument("--startup-limit-ms", required=True, type=int)
    parser.add_argument("--pss-limit-kib", required=True, type=int)
    return parser.parse_args()


def read_startup_line(process: subprocess.Popen[str], timeout_seconds: float) -> int:
    deadline = time.monotonic() + timeout_seconds
    pattern = re.compile(r"^startup-first-frame-ms=(\d+)$")
    assert process.stdout is not None
    while time.monotonic() < deadline:
        remaining = max(0.0, deadline - time.monotonic())
        readable, _, _ = select.select([process.stdout], [], [], remaining)
        if not readable:
            break
        line = process.stdout.readline().strip()
        match = pattern.match(line)
        if match:
            return int(match.group(1))
        if process.poll() is not None:
            break
    return_code = process.poll()
    stderr = (
        process.stderr.read()
        if return_code is not None and process.stderr is not None
        else ""
    )
    raise RuntimeError(
        f"installed editor did not report its first frame; "
        f"return={return_code} stderr={stderr!r}"
    )


def read_pss_kib(pid: int) -> int:
    rollup = Path(f"/proc/{pid}/smaps_rollup").read_text(encoding="utf-8")
    for line in rollup.splitlines():
        if line.startswith("Pss:"):
            return int(line.split()[1])
    raise RuntimeError("smaps_rollup did not contain Pss")


def main() -> int:
    arguments = parse_arguments()
    if arguments.startup_limit_ms <= 0 or arguments.pss_limit_kib <= 0:
        raise ValueError("measurement limits must be positive")

    environment = os.environ.copy()
    environment["QT_QPA_PLATFORM"] = "offscreen"
    environment["XDG_DATA_DIRS"] = str(arguments.data_dir)

    with tempfile.TemporaryDirectory(prefix="qindaqt-editor-probe-") as root:
        document = Path(root) / "probe.txt"
        document.write_text("QindaQt editor private runtime probe\n", encoding="utf-8")
        process = subprocess.Popen(
            [
                str(arguments.executable),
                "--theme",
                "qinda-dark",
                "--report-startup",
                str(document),
            ],
            env=environment,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        try:
            startup_ms = read_startup_line(process, timeout_seconds=10.0)
            samples: list[int] = []
            for _ in range(5):
                samples.append(read_pss_kib(process.pid))
                time.sleep(0.1)
            pss_median_kib = int(statistics.median(samples))
            if startup_ms > arguments.startup_limit_ms:
                raise RuntimeError(
                    f"first frame {startup_ms} ms exceeds "
                    f"{arguments.startup_limit_ms} ms"
                )
            if pss_median_kib > arguments.pss_limit_kib:
                raise RuntimeError(
                    f"median PSS {pss_median_kib} KiB exceeds "
                    f"{arguments.pss_limit_kib} KiB"
                )
            print(
                json.dumps(
                    {
                        "startupFirstFrameMs": startup_ms,
                        "pssSamplesKiB": samples,
                        "pssMedianKiB": pss_median_kib,
                    },
                    sort_keys=True,
                )
            )
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5.0)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5.0)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
