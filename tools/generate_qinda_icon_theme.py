#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate QindaQt's source-controlled scalable freedesktop icon theme.

AGENT-CONTRACT: every written name comes from qinda_icon_catalog.CANON or
.ALIASES, defined across the tools/qinda_icon_catalog_*.py modules. Add a new
name there, in its semantic group, with real artwork -- never here as an
index-based or group-level fallback shape. Do not write a hicolor index:
QindaQt only inherits the system hicolor theme (see
docs/wiki/shell/icon-theme.md); packaging it is a separate worker's install
concern.
"""
import argparse
import filecmp
import shutil
import sys
import tempfile
from pathlib import Path

from qinda_icon_catalog import ALIASES, CANON, GROUPS, GROUP_CONTEXT
from qinda_icon_shapes import render

ROOT = Path(__file__).resolve().parents[1] / "data/icons/QindaQt"


def _write(root: Path, group: str, name: str, icon) -> None:
    target = root / "scalable" / group
    target.mkdir(parents=True, exist_ok=True)
    (target / f"{name}.svg").write_text(render(icon, symbolic=False), encoding="utf-8")
    # `-symbolic` resolves before the plain name; keep it a real silhouette so
    # the runtime's source-in recolor (see qinda_icon_shapes.py) stays correct.
    (target / f"{name}-symbolic.svg").write_text(render(icon, symbolic=True), encoding="utf-8")


def generate(root: Path) -> None:
    # Regenerate from a clean slate so a name dropped from the catalog cannot
    # leave a stray, unvalidated file behind on disk.
    if (root / "scalable").is_dir():
        shutil.rmtree(root / "scalable")
    root.mkdir(parents=True, exist_ok=True)
    for name, icon in CANON.items():
        _write(root, icon.group, name, icon)
    for alias, (target, group) in ALIASES.items():
        _write(root, group, alias, CANON[target])

    directories = [f"scalable/{group}" for group in GROUPS]
    sections = "\n\n".join(
        f"[scalable/{group}]\nContext={GROUP_CONTEXT[group]}\nSize=64\nType=Scalable\nMinSize=16\nMaxSize=256"
        for group in GROUPS
    )
    (root / "index.theme").write_text(
        "[Icon Theme]\nName=QindaQt\nComment=Rounded QindaPunk geometry for QindaQt\n"
        "Inherits=hicolor\nDirectories=" + ",".join(directories) + "\n\n" + sections + "\n",
        encoding="utf-8",
    )


def _differences(expected: Path, actual: Path) -> list[str]:
    comparison = filecmp.dircmp(expected, actual)
    out = [f"missing {name}" for name in comparison.left_only] + [f"stray {name}" for name in comparison.right_only]
    _, mismatch, errors = filecmp.cmpfiles(expected, actual, comparison.common_files, shallow=False)
    out += [f"stale {name}" for name in mismatch + errors]
    for name in comparison.common_dirs:
        out += [f"{name}/{item}" for item in _differences(expected / name, actual / name)]
    return out


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="verify data/icons/QindaQt is current without writing it")
    if not parser.parse_args().check:
        generate(ROOT)
        return
    with tempfile.TemporaryDirectory(prefix="qindaqt-icons-") as temp:
        expected = Path(temp) / "QindaQt"
        generate(expected)
        problems = _differences(expected, ROOT)
    if problems:
        print("data/icons/QindaQt is out of date; run tools/generate_qinda_icon_theme.py:", file=sys.stderr)
        print("\n".join(sorted(problems)[:40]), file=sys.stderr)
        raise SystemExit(1)
    print("data/icons/QindaQt matches the catalog.")


if __name__ == "__main__":
    main()
