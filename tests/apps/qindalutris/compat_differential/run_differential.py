#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Differential test: both compat-db-v1 validators judge every mutation case.

    run_differential.py --judge PATH --repo ROOT --work DIR [--write-expected]

Generates the cases (mkcases.py), judges each with the C++ loader (the judge
executable) and the Python validator (tools/qindalutris-compat/validate.py)
under the same fixed clock, and fails when the two disagree on any case or
either differs from expected.txt. --write-expected rewrites expected.txt
from the verdicts when both sides agree (review the diff before committing).
"""

from __future__ import annotations

import argparse
import contextlib
import io
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
NOW = "2026-09-25T00:00:00Z"
HEADER = ("# SPDX-License-Identifier: GPL-3.0-or-later\n"
          f"# Verdicts both compat-db-v1 validators must reach (clock {NOW}).\n"
          "# Regenerate with run_differential.py --write-expected and review the diff.\n")


def python_verdicts(repo: Path, cases: list[Path]) -> dict[str, str]:
    sys.path.insert(0, str(repo / "tools" / "qindalutris-compat"))
    import validate  # noqa: E402
    verdicts = {}
    for case in cases:
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            status = validate.main(["--now", NOW, str(case)])
        verdicts[case.name] = {0: "ok", 1: "refused"}.get(status, f"status-{status}")
    return verdicts


def cpp_verdicts(judge: str, cases: list[Path]) -> dict[str, str]:
    done = subprocess.run([judge, "--now", NOW, *map(str, cases)], capture_output=True,
                          text=True, check=True)
    verdicts = {}
    for line in done.stdout.splitlines():
        verdict, _, path = line.partition(" ")
        verdicts[Path(path).name] = verdict
    return verdicts


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--judge", required=True)
    parser.add_argument("--repo", required=True, type=Path)
    parser.add_argument("--work", required=True, type=Path)
    parser.add_argument("--write-expected", action="store_true")
    args = parser.parse_args()
    subprocess.run([sys.executable, str(HERE / "mkcases.py"), str(args.work), str(args.repo)],
                   check=True)
    cases = sorted(args.work.iterdir())
    cpp = cpp_verdicts(args.judge, cases)
    py = python_verdicts(args.repo, cases)
    expected_file = HERE / "expected.txt"
    problems = [f"{c.name}: C++ {cpp.get(c.name)} but Python {py.get(c.name)}"
                for c in cases if cpp.get(c.name) != py.get(c.name)]
    if args.write_expected and not problems:
        expected_file.write_text(HEADER + "".join(f"{c.name} {py[c.name]}\n" for c in cases))
    expected = dict(line.split(" ", 1) for line in expected_file.read_text().splitlines()
                    if line and not line.startswith("#"))
    for case in cases:
        want = expected.get(case.name)
        if want is None:
            problems.append(f"{case.name}: no expected verdict")
        elif py.get(case.name) != want or cpp.get(case.name) != want:
            problems.append(f"{case.name}: expected {want}, C++ {cpp.get(case.name)}, "
                            f"Python {py.get(case.name)}")
    problems += [f"{name}: expected but not generated" for name in expected
                 if name not in {c.name for c in cases}]
    for problem in problems:
        print(problem)
    print(f"{len(cases)} cases, {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())
