#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real private folder/env composition under current UID; no Portage import."""
import os
from pathlib import Path
import tempfile
import unittest
from portage_environment import prepare_environment


class EnvironmentCompositionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="audio-env-control-")
        self.root = Path(self.temporary.name)
        self.installed = {"CFLAGS": "-O2", "CXXFLAGS": "-O2", "LDFLAGS": "-Wl,-O1",
            "USE": "alsa bluetooth", "CHOST": "x86_64-pc-linux-gnu", "CBUILD": "x86_64-pc-linux-gnu"}
        self.features = {"sandbox", "usersandbox", "binpkg-signing", "network-sandbox",
                         "pid-sandbox", "ipc-sandbox", "userpriv"}

    def tearDown(self):
        self.temporary.cleanup()

    def prepare(self, *, asan, tests):
        distdir = self.root / "distfiles"; distdir.mkdir()
        return prepare_environment(self.root, os.getgid(), os.getuid(), self.installed,
            self.root / "overlay", self.root / "gentoo", distdir, self.features,
            "DISPLAY DBUS_SESSION_BUS_ADDRESS XAUTHORITY DBUS_SYSTEM_BUS_ADDRESS",
            asan=asan, upstream_tests=tests)

    def test_normal_preserves_signing_flags_and_sandboxes(self):
        environment, tests = self.prepare(asan=False, tests=False)
        self.assertFalse(tests)
        self.assertEqual(environment["CFLAGS"], "-O2")
        self.assertEqual(environment["USE"], "-* alsa bluetooth")
        self.assertTrue(self.features.issubset(environment["FEATURES"].split()))
        self.assertNotIn("noauto", environment["FEATURES"].split())
        self.assertNotIn("LD_PRELOAD", environment)

    def test_asan_retains_strict_tests_and_only_private_addresses(self):
        environment, tests = self.prepare(asan=True, tests=False)
        self.assertTrue(tests)
        self.assertIn("-fsanitize=address", environment["CFLAGS"])
        self.assertIn("test", environment["USE"].split())
        self.assertIn("test", environment["FEATURES"].split())
        self.assertNotIn("sandbox", environment["FEATURES"].split())
        self.assertIn("network-sandbox", environment["FEATURES"].split())
        self.assertEqual(environment["ENV_UNSET"], "DISPLAY XAUTHORITY -DBUS_SESSION_BUS_ADDRESS -DBUS_SYSTEM_BUS_ADDRESS")
        for kind in ("SESSION", "SYSTEM"):
            key = f"DBUS_{kind}_BUS_ADDRESS"
            self.assertEqual(environment[key], f"unix:path={self.root}/absent-{kind.lower()}")
        self.assertEqual(environment["PIPEWIRE_CORE"], "pipewire-0")
        self.assertEqual(environment["PIPEWIRE_REMOTE"], "pipewire-0")

    def test_private_xdg_paths_are_owned_compact_and_process_local(self):
        environment, _ = self.prepare(asan=False, tests=True)
        self.assertEqual(environment["MAKEOPTS"], "-j8 -l8")
        for key in ("XDG_RUNTIME_DIR", "XDG_CONFIG_HOME", "XDG_STATE_HOME", "XDG_CACHE_HOME", "XDG_DATA_HOME"):
            path = Path(environment[key])
            self.assertTrue(path.is_relative_to(self.root))
            self.assertEqual(path.stat().st_mode & 0o777, 0o700)
            self.assertEqual(path.stat().st_uid, os.getuid())
        self.assertEqual(self.installed["USE"], "alsa bluetooth")
        self.assertNotIn("test", self.features)


if __name__ == "__main__":
    unittest.main()
