# SPDX-License-Identifier: GPL-3.0-or-later
"""Hostile unit checks for the DesktopVirtual static closure parser."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from desktop_session_stage_closure import (
    StageClosureError,
    _authenticate_qmldirs,
    parse_dynamic_output,
    qml_imports,
)


class StageClosureUnitTests(unittest.TestCase):
    def test_dynamic_tags_are_exact_and_ordered(self) -> None:
        parsed = parse_dynamic_output(
            """
 0x1 (NEEDED) Shared library: [libfirst.so]
 0x1 (NEEDED) Shared library: [libsecond.so]
 0xf (RPATH) Library rpath: [$ORIGIN/one:/usr/lib64]
 0x1d (RUNPATH) Library runpath: [$ORIGIN/two]
"""
        )
        self.assertEqual(parsed.needed, ("libfirst.so", "libsecond.so"))
        self.assertEqual(parsed.rpath, ("$ORIGIN/one", "/usr/lib64"))
        self.assertEqual(parsed.runpath, ("$ORIGIN/two",))

    def test_qml_imports_derive_only_product_modules(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "Panel.qml"
            source.write_text(
                "import QtQuick\nimport QindaQt.Shell.AudioApplet 1.0 as Audio\n"
                "import QindaQt.Controls\n",
                encoding="utf-8",
            )
            self.assertEqual(
                qml_imports((source,)),
                ("QindaQt.Controls", "QindaQt.Shell.AudioApplet"),
            )

    def test_qmldir_must_be_regular_and_present(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "qml"
            module = root / "QindaQt/Shell/AudioApplet"
            module.mkdir(parents=True)
            (module / "qmldir").write_text("module QindaQt.Shell.AudioApplet\n")
            _authenticate_qmldirs(root, ("QindaQt.Shell.AudioApplet",))
            (module / "qmldir").unlink()
            with self.assertRaisesRegex(StageClosureError, "has no qmldir"):
                _authenticate_qmldirs(root, ("QindaQt.Shell.AudioApplet",))

    def test_embedded_qml_exemptions_must_name_an_import(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "qml"
            root.mkdir()
            _authenticate_qmldirs(
                root,
                ("QindaQt.SettingsApp.PowerBackend",),
                ("QindaQt.SettingsApp.PowerBackend",),
            )
            with self.assertRaisesRegex(StageClosureError, "not imported"):
                _authenticate_qmldirs(
                    root,
                    ("QindaQt.SettingsApp.PowerBackend",),
                    ("QindaQt.SettingsApp.Customize",),
                )


if __name__ == "__main__":
    unittest.main()
