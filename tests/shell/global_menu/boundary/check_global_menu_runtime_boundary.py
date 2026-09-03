#!/usr/bin/env python3
"""Prove the runtime consumes only the shared shell authority client."""

from __future__ import annotations

import argparse
import pathlib
import shutil
import sys


FORBIDDEN = (
    "qt_shell_window_actions_transport.h",
    "QtShellWindowActionsTransport",
    "src/compositor/kwin",
    "org.qindaqt.CompositorShell1",
    "QDBusInterface",
)
REQUIRED = (
    "ShellWindowActionsClient::ShellWindowActionsClient &",
    "identityChanged",
    "identitySnapshot()",
)


def validate(header: pathlib.Path, source: pathlib.Path) -> list[str]:
    text = header.read_text(encoding="utf-8") + "\n" + source.read_text(encoding="utf-8")
    failures = [f"forbidden boundary token: {token}" for token in FORBIDDEN if token in text]
    failures.extend(f"missing shared-client token: {token}" for token in REQUIRED if token not in text)
    return failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runtime-source", type=pathlib.Path, required=True)
    parser.add_argument("--runtime-header", type=pathlib.Path, required=True)
    parser.add_argument("--fixture-root", type=pathlib.Path, required=True)
    args = parser.parse_args()

    failures = validate(args.runtime_header, args.runtime_source)
    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1

    if args.fixture_root.exists():
        shutil.rmtree(args.fixture_root)
    args.fixture_root.mkdir(parents=True)
    poisoned_header = args.fixture_root / "globalmenuappletcomposition.h"
    poisoned_source = args.fixture_root / "globalmenuappletcomposition.cpp"
    poisoned_header.write_text(args.runtime_header.read_text(encoding="utf-8"), encoding="utf-8")
    poisoned_source.write_text(
        args.runtime_source.read_text(encoding="utf-8")
        + "\n// src/compositor/kwin/private-window-authority.h\n",
        encoding="utf-8",
    )
    if not validate(poisoned_header, poisoned_source):
        print("poison fixture was not rejected", file=sys.stderr)
        return 1
    print("global-menu runtime boundary and hostile poison control passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
