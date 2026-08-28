# SPDX-License-Identifier: GPL-3.0-or-later
"""Focused tests for the private namespace's synthetic account identity."""

from __future__ import annotations

import os
import shutil
import tempfile
import unittest
from pathlib import Path, PurePosixPath

from desktop_session_sandbox import (
    ReadOnlyMount,
    SandboxContractError,
    SandboxSpec,
    build_bwrap_argv,
    create_run_root,
    remove_run_root,
    sandbox_environment,
)


def _executable(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("fixture\n", encoding="utf-8")
    path.chmod(0o700)


class PrivateIdentityTests(unittest.TestCase):
    def test_identity_is_current_only_and_read_only_bound(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build = root / "build"
            build.mkdir()
            paths = create_run_root(build, "8" * 32)
            bwrap = root / "bwrap"
            probe = root / "probe"
            _executable(bwrap)
            _executable(probe)
            stage = root / "stage"
            source = root / "source"
            system = root / "system"
            for item in (stage, source, system):
                item.mkdir()
            spec = SandboxSpec(
                bwrap=bwrap,
                run_id="8" * 32,
                uid=os.getuid(),
                paths=paths,
                stage=ReadOnlyMount(stage, PurePosixPath("/opt/qindaqt")),
                tests=ReadOnlyMount(source, PurePosixPath("/opt/qindaqt-source")),
                probe=ReadOnlyMount(probe, PurePosixPath("/opt/qindaqt-tools/probe")),
                system_mounts=(ReadOnlyMount(system, PurePosixPath("/usr")),),
                environment=sandbox_environment(
                    run_id="8" * 32,
                    uid=os.getuid(),
                    stage_bin="/opt/qindaqt/bin",
                    system_path=["/usr/bin"],
                ),
                command=("/usr/bin/python3", "--version"),
            )
            self.assertEqual(
                paths.passwd.read_text(encoding="ascii"),
                f"qindaqt:x:{os.getuid()}:{os.getgid()}:QindaQt:/home/qindaqt:"
                "/usr/sbin/nologin\n",
            )
            self.assertEqual(
                paths.group.read_text(encoding="ascii"),
                f"qindaqt:x:{os.getgid()}:\n",
            )
            argv = build_bwrap_argv(spec)
            mounts = [
                argv[index + 1:index + 3]
                for index, value in enumerate(argv)
                if value == "--ro-bind"
            ]
            self.assertIn([str(paths.passwd.resolve()), "/etc/passwd"], mounts)
            self.assertIn([str(paths.group.resolve()), "/etc/group"], mounts)
            remove_run_root(paths, build, "8" * 32)

    def test_tampered_identity_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory) / "build"
            build.mkdir()
            paths = create_run_root(build, "9" * 32)
            paths.passwd.write_text(
                "root:x:0:0:root:/root:/bin/sh\n", encoding="ascii"
            )
            with self.assertRaisesRegex(SandboxContractError, "identity file content"):
                remove_run_root(paths, build, "9" * 32)
            shutil.rmtree(paths.root)


if __name__ == "__main__":
    unittest.main()
