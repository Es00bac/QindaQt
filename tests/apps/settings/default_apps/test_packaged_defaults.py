#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the installed desktop policy using xdg-mime in private XDG roots."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


CORE_TYPES = {
    "org.qindaqt.FileManager.desktop": ["inode/directory"],
    "org.qindaqt.TextEditor.desktop": ["text/plain"],
    "org.qindaqt.Viewer.desktop": [
        "application/pdf", "image/jpeg", "image/png", "image/gif", "image/webp",
        "image/bmp", "image/tiff", "image/svg+xml",
    ],
    "org.qindaqt.QQMpv.desktop": [
        "video/mp4", "video/x-matroska", "video/webm", "video/mpeg",
        "video/x-msvideo", "audio/mpeg", "audio/flac", "audio/ogg",
    ],
}


class PackagedDefaultsTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="qindaqt-mime-policy-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.config = self.root / "config"
        self.admin = self.root / "admin"
        self.data = self.root / "user-data"
        self.system = self.root / "system-data"
        for path in (self.config, self.admin, self.data / "applications",
                     self.system / "applications", self.root / "home"):
            path.mkdir(parents=True)
        shutil.copyfile(INSTALLED_POLICY, self.system / "applications/qindaqt-mimeapps.list")
        self.environment = {
            "PATH": os.environ["PATH"], "HOME": str(self.root / "home"),
            "XDG_CONFIG_HOME": str(self.config), "XDG_CONFIG_DIRS": str(self.admin),
            "XDG_DATA_HOME": str(self.data), "XDG_DATA_DIRS": str(self.system),
            "XDG_CURRENT_DESKTOP": "QindaQt", "DESKTOP_SESSION": "qindaqt",
            "DBUS_SESSION_BUS_ADDRESS": "unix:path=/nonexistent",
            "DBUS_SYSTEM_BUS_ADDRESS": "unix:path=/nonexistent", "LC_ALL": "C",
        }
        # Desktop entries are controlled lookup fixtures, never launched.
        for desktop_id, types in CORE_TYPES.items():
            self.add_application(desktop_id, types)
        self.add_application("alternative.desktop", ["application/pdf", "audio/mpeg"])

    def add_application(self, desktop_id, types):
        (self.system / "applications" / desktop_id).write_text(
            "[Desktop Entry]\nType=Application\nName=Default-policy fixture\n"
            f"Exec=/bin/true %U\nMimeType={';'.join(types)};\n", encoding="utf-8")

    def query(self, mime_type):
        result = subprocess.run([XDG_MIME, "query", "default", mime_type],
                                env=self.environment, text=True, capture_output=True,
                                timeout=10, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout.strip()

    def test_all_packaged_core_mime_types(self):
        for desktop_id, types in CORE_TYPES.items():
            for mime_type in types:
                with self.subTest(mime_type=mime_type):
                    self.assertEqual(self.query(mime_type), desktop_id)
        self.assertFalse((self.config / "mimeapps.list").exists())

    def test_optional_browser_and_mail_fall_back_to_installed_ids(self):
        browser_types = ["text/html", "x-scheme-handler/http", "x-scheme-handler/https"]
        self.add_application("org.mozilla.firefox.desktop", browser_types)
        self.add_application("org.mozilla.Thunderbird.desktop", ["x-scheme-handler/mailto"])
        for mime_type in browser_types:
            self.assertEqual(self.query(mime_type), "org.mozilla.firefox.desktop")
        self.assertEqual(self.query("x-scheme-handler/mailto"), "org.mozilla.Thunderbird.desktop")
        self.add_application("firefox.desktop", browser_types)
        self.assertEqual(self.query("text/html"), "firefox.desktop")

    def test_user_choices_override_packaged_defaults(self):
        (self.config / "mimeapps.list").write_text(
            "[Default Applications]\napplication/pdf=alternative.desktop;\n"
            "audio/mpeg=alternative.desktop;\n", encoding="utf-8")
        self.assertEqual(self.query("application/pdf"), "alternative.desktop")
        self.assertEqual(self.query("audio/mpeg"), "alternative.desktop")
        self.assertEqual(self.query("image/png"), "org.qindaqt.Viewer.desktop")

    def test_admin_and_desktop_user_precedence(self):
        (self.admin / "mimeapps.list").write_text(
            "[Default Applications]\napplication/pdf=alternative.desktop;\n", encoding="utf-8")
        self.assertEqual(self.query("application/pdf"), "alternative.desktop")
        (self.config / "mimeapps.list").write_text(
            "[Default Applications]\napplication/pdf=org.qindaqt.Viewer.desktop;\n", encoding="utf-8")
        self.assertEqual(self.query("application/pdf"), "org.qindaqt.Viewer.desktop")
        (self.config / "qindaqt-mimeapps.list").write_text(
            "[Default Applications]\napplication/pdf=alternative.desktop;\n", encoding="utf-8")
        self.assertEqual(self.query("application/pdf"), "alternative.desktop")

    def test_missing_handler_falls_through_to_lower_preference(self):
        (self.config / "mimeapps.list").write_text(
            "[Default Applications]\napplication/pdf=not-installed.desktop;\n", encoding="utf-8")
        self.assertEqual(self.query("application/pdf"), "org.qindaqt.Viewer.desktop")

    def test_policy_is_scoped_to_qindaqt(self):
        (self.system / "applications/mimeapps.list").write_text(
            "[Default Applications]\napplication/pdf=alternative.desktop;\n", encoding="utf-8")
        self.assertEqual(self.query("application/pdf"), "org.qindaqt.Viewer.desktop")
        self.environment["XDG_CURRENT_DESKTOP"] = "Other"
        self.assertEqual(self.query("application/pdf"), "alternative.desktop")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-directory", required=True)
    parser.add_argument("--install-datadir", required=True)
    parser.add_argument("--xdg-mime", required=True)
    args = parser.parse_args()
    XDG_MIME = args.xdg_mime
    with tempfile.TemporaryDirectory(prefix="qindaqt-mime-install-") as stage:
        # Execute the actual session-local install rules. Its unrelated
        # desktop-controls static library is outside this package-data proof.
        subprocess.run(["cmake", "-DCMAKE_INSTALL_LOCAL_ONLY=TRUE",
                        f"-DCMAKE_INSTALL_PREFIX={stage}", "-DCMAKE_INSTALL_COMPONENT=QindaQt",
                        "-P", str(Path(args.build_directory) / "src/session/cmake_install.cmake")],
                       check=True, capture_output=True, text=True, timeout=30)
        INSTALLED_POLICY = Path(stage) / args.install_datadir / "applications/qindaqt-mimeapps.list"
        if not INSTALLED_POLICY.is_file():
            raise SystemExit(f"Missing installed MIME policy: {INSTALLED_POLICY}")
        unittest.main(argv=[__file__], verbosity=2)
