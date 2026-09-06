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
import shutil
from pathlib import Path

from qinda_icon_catalog import ALIASES, CANON, GROUPS, GROUP_CONTEXT
from qinda_icon_shapes import render

ROOT = Path(__file__).resolve().parents[1] / "data/icons/QindaQt"


def _write(group: str, name: str, icon) -> None:
    target = ROOT / "scalable" / group
    target.mkdir(parents=True, exist_ok=True)
    (target / f"{name}.svg").write_text(render(icon, symbolic=False), encoding="utf-8")
    # `-symbolic` resolves before the plain name; keep it a real silhouette so
    # the runtime's source-in recolor (see qinda_icon_shapes.py) stays correct.
    (target / f"{name}-symbolic.svg").write_text(render(icon, symbolic=True), encoding="utf-8")


def main() -> None:
    # Regenerate from a clean slate so a name dropped from the catalog cannot
    # leave a stray, unvalidated file behind on disk.
    if (ROOT / "scalable").is_dir():
        shutil.rmtree(ROOT / "scalable")
    ROOT.mkdir(parents=True, exist_ok=True)
    for name, icon in CANON.items():
        _write(icon.group, name, icon)
    for alias, (target, group) in ALIASES.items():
        _write(group, alias, CANON[target])

    directories = [f"scalable/{group}" for group in GROUPS]
    sections = "\n\n".join(
        f"[scalable/{group}]\nContext={GROUP_CONTEXT[group]}\nSize=64\nType=Scalable\nMinSize=16\nMaxSize=256"
        for group in GROUPS
    )
    (ROOT / "index.theme").write_text(
        "[Icon Theme]\nName=QindaQt\nComment=Soft mineral geometry for QindaQt\n"
        "Inherits=hicolor\nDirectories=" + ",".join(directories) + "\n\n" + sections + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
