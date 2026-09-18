#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Every nested-session driver must be committed executable.

KWin runs a row's driver through `--exit-with-session <driver>`; a driver
committed 100644 cannot be exec'd from a fresh checkout, and KWin then idles
to the row's timeout with an empty stdout. A local `chmod +x` hides that from
the author, so the check reads the git index mode, not the working tree.
Twice in one wave a candidate shipped that way (O14-MVP P0-2, O9 84d8777d).
"""

from __future__ import annotations

import subprocess
import sys
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent


def has_shebang(path: Path) -> bool:
    with path.open("rb") as handle:
        return handle.read(2) == b"#!"


def nested_drivers() -> list[Path]:
    """The `--exit-with-session` programs: driver-named scripts that carry a
    shebang. A helper driven through `python3 <file>` (no shebang) is not one."""
    return sorted(path for path in HERE.rglob("*driver.py")
                  if path.name != Path(__file__).name and has_shebang(path))


def index_mode(path: Path) -> str | None:
    completed = subprocess.run(
        ["git", "-C", str(HERE), "ls-files", "-s", "--", str(path)],
        capture_output=True, text=True, check=False)
    if completed.returncode != 0 or not completed.stdout.strip():
        return None
    return completed.stdout.split()[0]


class NestedDriverModesTest(unittest.TestCase):
    def test_every_driver_is_committed_executable(self) -> None:
        drivers = nested_drivers()
        self.assertGreaterEqual(len(drivers), 2, "expected the nested drivers under tests/session")
        problems = []
        for driver in drivers:
            mode = index_mode(driver)
            if mode is None:
                problems.append(f"{driver.relative_to(HERE)}: not in the git index (uncommitted?)")
            elif mode != "100755":
                problems.append(f"{driver.relative_to(HERE)}: committed as {mode}, not 100755")
            elif not driver.stat().st_mode & 0o111:
                problems.append(f"{driver.relative_to(HERE)}: not executable on disk")
        self.assertEqual(problems, [], "\n".join(problems))

    def test_every_driver_has_a_python_shebang(self) -> None:
        for driver in nested_drivers():
            with self.subTest(driver=driver.name):
                first = driver.read_text(encoding="utf-8").splitlines()[0]
                self.assertTrue(first.startswith("#!") and "python" in first, first)


if __name__ == "__main__":
    sys.exit(unittest.main())
