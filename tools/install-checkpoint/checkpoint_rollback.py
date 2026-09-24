#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Render guarded state-capture and rollback scripts for a later live change."""

from __future__ import annotations

import re
import shlex
from pathlib import Path
from typing import Any, Sequence

from checkpoint_contract import ROLLBACK_CONFIRMATION, SNAPSHOT_CONFIRMATION

def render_snapshot_script(paths: Sequence[Path], archive: Path, checksum: Path) -> str:
    lines = [
        "#!/bin/sh",
        "set -eu",
        f"archive={shlex.quote(str(archive))}",
        f"checksum={shlex.quote(str(checksum))}",
        f"expected={shlex.quote(SNAPSHOT_CONFIRMATION)}",
        'if [ "${1:-}" != "$expected" ]; then',
        '  printf "Refusing state copy. Pass the exact confirmation token shown in the checkpoint.\\n" >&2',
        "  exit 64",
        "fi",
        'umask 077',
        'mkdir -p "$(dirname "$archive")"',
        'tmp="$archive.tmp.$$"',
        'trap \'rm -f "$tmp"\' EXIT HUP INT TERM',
        "set --",
    ]
    for path in paths:
        quoted = shlex.quote(str(path))
        lines.append(f"if [ -e {quoted} ] || [ -L {quoted} ]; then set -- \"$@\" {quoted}; fi")
    lines.extend(
        [
            'if [ "$#" -gt 0 ]; then',
            '  tar -cpf "$tmp" --absolute-names -- "$@"',
            "else",
            '  tar -cpf "$tmp" --files-from /dev/null',
            "fi",
            'chmod 600 "$tmp"',
            'mv -f "$tmp" "$archive"',
            'sha256sum "$archive" > "$checksum"',
            'chmod 600 "$checksum"',
            'trap - EXIT HUP INT TERM',
            'printf "Saved selected Settings1/Audio1 state files to %s\\n" "$archive"',
        ]
    )
    return "\n".join(lines) + "\n"


def render_rollback_script(
    package: dict[str, Any],
    active_units: Sequence[str],
    archive: Path,
    checksum: Path,
    execution_host: str,
) -> str:
    if not package.get("rollbackAvailable"):
        raise ValueError("an exact prior Portage package and source archive are required to render rollback")
    if not execution_host or "\n" in execution_host:
        raise ValueError("the rollback recipe must be bound to one execution host")
    atom = package["atom"]
    repo_ebuild = package["repositoryEbuild"]
    source_archive = package["sourceArchive"]
    source_hash = package["sourceArchiveSha256"]
    ebuild_hash = package.get("repositoryEbuildSha256")
    if not isinstance(ebuild_hash, str) or not re.fullmatch(r"[0-9a-f]{64}", ebuild_hash):
        raise ValueError("the exact prior repository ebuild SHA-256 is required to render rollback")
    if not isinstance(source_hash, str) or not re.fullmatch(r"[0-9a-f]{64}", source_hash):
        raise ValueError("the exact prior source archive SHA-256 is required to render rollback")
    unit_lines = [f"systemctl --user stop {shlex.quote(unit)}" for unit in active_units]
    start_lines = [f"systemctl --user start {shlex.quote(unit)}" for unit in active_units]
    lines = [
        "#!/bin/sh",
        "set -eu",
        f"expected={shlex.quote(ROLLBACK_CONFIRMATION)}",
        'if [ "${1:-}" != "$expected" ]; then',
        '  printf "Refusing rollback. Pass the exact confirmation token shown in the checkpoint.\\n" >&2',
        "  exit 64",
        "fi",
        f"expected_host={shlex.quote(execution_host)}",
        'actual_host=$(hostname)',
        '[ "$actual_host" = "$expected_host" ] || { printf "This rollback recipe belongs to host %s, not %s\\n" "$expected_host" "$actual_host" >&2; exit 66; }',
        f"test -r {shlex.quote(repo_ebuild)} || {{ printf 'Prior ebuild is unavailable: %s\\n' {shlex.quote(repo_ebuild)} >&2; exit 66; }}",
        f"test -r {shlex.quote(source_archive)} || {{ printf 'Prior source archive is unavailable: %s\\n' {shlex.quote(source_archive)} >&2; exit 66; }}",
        f"test -r {shlex.quote(str(archive))} || {{ printf 'State snapshot is missing; run capture-live-state.sh first.\\n' >&2; exit 66; }}",
        f"test -r {shlex.quote(str(checksum))} || {{ printf 'State snapshot checksum is missing.\\n' >&2; exit 66; }}",
        f"expected_ebuild_sha={shlex.quote(ebuild_hash)}",
        f"actual_ebuild_sha=$(sha256sum {shlex.quote(repo_ebuild)} | cut -d ' ' -f 1)",
        '[ "$actual_ebuild_sha" = "$expected_ebuild_sha" ] || { printf "Prior repository ebuild hash changed\\n" >&2; exit 66; }',
        f"expected_sha={shlex.quote(source_hash)}",
        f"actual_sha=$(sha256sum {shlex.quote(source_archive)} | cut -d ' ' -f 1)",
        '[ "$actual_sha" = "$expected_sha" ] || { printf "Prior source archive hash changed\\n" >&2; exit 66; }',
        f"state_expected=$(cut -d ' ' -f 1 {shlex.quote(str(checksum))})",
        f"state_actual=$(sha256sum {shlex.quote(str(archive))} | cut -d ' ' -f 1)",
        '[ "$state_actual" = "$state_expected" ] || { printf "State snapshot hash check failed\\n" >&2; exit 66; }',
        f"sudo emerge --ask --oneshot {shlex.quote(atom)}",
        "restart_services() {",
        "  result=$?",
        "  trap - EXIT HUP INT TERM",
        "  systemctl --user daemon-reload || result=1",
        *[f"  systemctl --user start {shlex.quote(unit)} || result=1" for unit in active_units],
        '  exit "$result"',
        "}",
        "trap restart_services EXIT",
        "trap 'exit 129' HUP",
        "trap 'exit 130' INT",
        "trap 'exit 143' TERM",
        *unit_lines,
        f"tar -xpf {shlex.quote(str(archive))} --absolute-names -C /",
        "systemctl --user daemon-reload",
        *start_lines,
        "trap - EXIT HUP INT TERM",
        'printf "Previous package and captured Settings1/Audio1 state restored. Log out and back in to reload the shell/session.\\n"',
    ]
    return "\n".join(lines) + "\n"
