# SPDX-License-Identifier: GPL-3.0-or-later
"""Parse Linux process accounting for bounded desktop PSS/CPU evidence."""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Mapping


class MeasurementContractError(ValueError):
    """A procfs measurement is malformed, incomplete, or contradictory."""


PSS_PATTERN = re.compile(r"^Pss:\s+([0-9]+)\s+kB$")


@dataclass(frozen=True)
class ProcessSample:
    pid: int
    pss_kib: int
    user_ticks: int
    system_ticks: int

    @property
    def cpu_ticks(self) -> int:
        return self.user_ticks + self.system_ticks


def parse_smaps_rollup(contents: str) -> int:
    matches = [
        match
        for line in contents.splitlines()
        if (match := PSS_PATTERN.fullmatch(line)) is not None
    ]
    if len(matches) != 1:
        raise MeasurementContractError("smaps_rollup must contain exactly one Pss line")
    value = int(matches[0].group(1), 10)
    if value < 0:
        raise MeasurementContractError("Pss cannot be negative")
    return value


def parse_proc_stat(contents: str) -> tuple[int, int]:
    closing = contents.rfind(")")
    if closing < 2:
        raise MeasurementContractError("proc stat omitted the command delimiter")
    fields = contents[closing + 2 :].split()
    # fields starts at documented field 3; utime/stime are fields 14/15.
    if len(fields) < 13:
        raise MeasurementContractError("proc stat was truncated")
    try:
        user_ticks = int(fields[11], 10)
        system_ticks = int(fields[12], 10)
    except ValueError as error:
        raise MeasurementContractError("proc stat CPU ticks are malformed") from error
    if user_ticks < 0 or system_ticks < 0:
        raise MeasurementContractError("proc stat CPU ticks cannot be negative")
    return user_ticks, system_ticks


def read_process_sample(pid: int, *, proc_root: Path = Path("/proc")) -> ProcessSample:
    if pid <= 1:
        raise MeasurementContractError("measurement PID must be greater than 1")
    try:
        process_root = proc_root / str(pid)
        pss = parse_smaps_rollup(
            (process_root / "smaps_rollup").read_text(encoding="ascii")
        )
        user, system = parse_proc_stat(
            (process_root / "stat").read_text(encoding="ascii")
        )
    except OSError as error:
        raise MeasurementContractError(f"could not sample PID {pid}: {error}") from error
    return ProcessSample(pid, pss, user, system)


def aggregate_pss_kib(samples: Iterable[ProcessSample]) -> int:
    values = list(samples)
    if len({item.pid for item in values}) != len(values):
        raise MeasurementContractError("PSS sample PIDs must be unique")
    return sum(item.pss_kib for item in values)


def cpu_percent(
    before: Mapping[int, ProcessSample],
    after: Mapping[int, ProcessSample],
    *,
    elapsed_seconds: float,
    clock_ticks_per_second: int,
) -> float:
    if elapsed_seconds <= 0 or clock_ticks_per_second <= 0:
        raise MeasurementContractError("CPU sampling interval and clock rate must be positive")
    if set(before) != set(after) or not before:
        raise MeasurementContractError("CPU samples must cover the same nonempty PID set")
    delta = 0
    for pid, initial in before.items():
        final = after[pid]
        if final.cpu_ticks < initial.cpu_ticks:
            raise MeasurementContractError(f"CPU ticks regressed for PID {pid}")
        delta += final.cpu_ticks - initial.cpu_ticks
    return 100.0 * delta / (clock_ticks_per_second * elapsed_seconds)
