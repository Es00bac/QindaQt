#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later

"""Reject forbidden runtime/platform dependencies and prove each poison fires."""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil


POISONS = {
    "runtime-private": "#include <qindaqt/shell/runtime/shellruntimeapplication.h>",
    "ambient-session-bus": "QDBusConnection::sessionBus()",
    "kwin-private": "#include <kwin/workspace.h>",
    "qml-authority": "#include <QtQml/QQmlEngine>",
}


def violations(root: Path) -> list[str]:
    found: list[str] = []
    for path in sorted(root.rglob("*")):
        if path.suffix not in {".h", ".cpp"}:
            continue
        text = path.read_text(encoding="utf-8")
        for name, poison in POISONS.items():
            if poison in text:
                found.append(f"{path}:{name}")
    registrar_sources = root / "registrar"
    for path in sorted(registrar_sources.rglob("*.cpp")):
        if path.name != "appmenu_registrar.cpp" and "registerService(" in path.read_text(
            encoding="utf-8"
        ):
            found.append(f"{path}:name-ownership-outside-composition-root")
    return found


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--fixture-root", required=True, type=Path)
    args = parser.parse_args()

    clean_findings = violations(args.source_root)
    if clean_findings:
        raise SystemExit("production boundary violations: " + ", ".join(clean_findings))

    if args.fixture_root.exists():
        shutil.rmtree(args.fixture_root)
    args.fixture_root.mkdir(parents=True)
    for name, poison in POISONS.items():
        fixture = args.fixture_root / name
        fixture.mkdir()
        (fixture / "poison.cpp").write_text(poison + "\n", encoding="utf-8")
        if not violations(fixture):
            raise SystemExit(f"poison was not rejected: {name}")
    shutil.rmtree(args.fixture_root)
    print(f"global-menu transport boundary clean; rejected {len(POISONS)} poisons")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
