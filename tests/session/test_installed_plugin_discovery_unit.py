# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise install containment with real CMake and disposable destinations."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from test_installed_plugin_discovery import installed_artifact_path, stage_install


class InstalledPluginStageTests(unittest.TestCase):
    def test_relative_and_absolute_install_destinations_are_contained(self):
        cmake = shutil.which("cmake")
        self.assertIsNotNone(cmake)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            source = root / "source"
            build = root / "build"
            absolute = root / "outside"
            source.mkdir()
            (source / "payload").write_text("fixture\n")
            # The hostile absolute target is disposable even with the old
            # broken driver; the regression must never attempt host /etc.
            (source / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.20)\n'
                'project(StageFixture NONE)\n'
                'install(FILES payload DESTINATION bin RENAME launcher)\n'
                'install(FILES payload DESTINATION lib/plugins RENAME compositor)\n'
                'install(FILES payload DESTINATION lib/decoration RENAME decoration)\n'
                f'install(FILES payload DESTINATION "{absolute.as_posix()}")\n'
            )
            subprocess.run([cmake, "-S", str(source), "-B", str(build)],
                           check=True, capture_output=True, text=True)
            stage = build / "stage"
            args = argparse.Namespace(
                cmake=cmake, build_directory=build, install_prefix=stage,
                configuration="", launcher_relative=Path("bin/launcher"),
                compositor_plugin_relative=Path("lib/plugins/compositor"),
                decoration_relative=Path("lib/decoration/decoration"))
            inherited = root / "inherited-destdir"
            with patch.dict(os.environ, {"DESTDIR": str(inherited)}):
                artifacts = stage_install(args)
            self.assertEqual(artifacts, tuple(stage / path for path in (
                "bin/launcher", "lib/plugins/compositor", "lib/decoration/decoration")))
            self.assertEqual((stage / absolute.relative_to("/") / "payload").read_text(),
                             "fixture\n")
            self.assertFalse(absolute.exists())
            self.assertFalse(inherited.exists())

    def test_cleanup_rejects_build_root_and_external_prefix(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            build = root / "build"
            build.mkdir()
            for prefix in (build, root / "outside"):
                with self.subTest(prefix=prefix), patch("shutil.rmtree") as remove:
                    with self.assertRaisesRegex(RuntimeError, "child of its build"):
                        stage_install(argparse.Namespace(build_directory=build,
                                                        install_prefix=prefix))
                    remove.assert_not_called()

    def test_artifact_rejects_absolute_parent_and_symlink_escape(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            stage = root / "stage"
            stage.mkdir()
            outside = root / "outside"
            outside.write_text("fixture")
            (stage / "link").symlink_to(outside)
            for relative in (outside, Path("../outside"), Path("link")):
                with self.subTest(relative=relative):
                    with self.assertRaises(RuntimeError):
                        installed_artifact_path(stage, relative, "fixture")


if __name__ == "__main__":
    unittest.main()
