#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Two real ordinary-Wayland prompts; only disposable collections and input."""
import os
import pathlib
import subprocess
import sys
import time
import unittest
import secretstorage
import test_secret_service as protocol

PROMPT = pathlib.Path(sys.argv.pop(1)).resolve()
COMPOSITOR = pathlib.Path(sys.argv.pop(1)).resolve()

class RequestReviewJourney(unittest.TestCase):
    setUp = protocol.SecretServiceTest.setUp
    cleanup = protocol.SecretServiceTest.cleanup
    stop = protocol.SecretServiceTest.stop
    call = protocol.SecretServiceTest.call
    native = protocol.SecretServiceTest.native

    def start(self):
        if not COMPOSITOR.is_file():
            self.skipTest("private production compositor unavailable")
        for key in ("DISPLAY", "WAYLAND_DISPLAY", "WAYLAND_SOCKET", "QT_WAYLAND_SHELL_INTEGRATION"):
            self.env.pop(key, None)
        self.env.update(HOME=str(self.root), XDG_CONFIG_HOME=str(self.root / "config"),
                        XDG_CACHE_HOME=str(self.root / "cache"), XDG_STATE_HOME=str(self.root / "state"),
                        DBUS_SYSTEM_BUS_ADDRESS="unix:path=" + str(self.root / "no-system-bus"),
                        QT_QPA_PLATFORM="wayland", QT_QUICK_BACKEND="software", KWIN_COMPOSE="Q",
                        QINDAQT_JOURNEY_PHASE=str(self.root / "phase"),
                        QINDAQT_JOURNEY_PROMPT_AUDIT=str(self.root / "prompt.audit"))
        (self.root / "phase").write_text("0")
        self.compositor = subprocess.Popen(
            [str(COMPOSITOR), "--virtual", "--width", "800", "--height", "600",
             "--socket", "qindaqt-7", "--no-global-shortcuts"], env=self.env,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.addCleanup(self.stop_compositor)
        deadline = time.monotonic() + 5
        while not (self.runtime / "qindaqt-7").exists() and self.compositor.poll() is None and time.monotonic() < deadline:
            time.sleep(.05)
        self.assertTrue((self.runtime / "qindaqt-7").exists(), "private compositor did not start")
        self.env["QINDAQT_JOURNEY_COMPOSITOR_PID"] = str(self.compositor.pid)
        previous = protocol.HELPER
        protocol.HELPER = PROMPT
        try:
            protocol.SecretServiceTest.start(self)
        finally:
            protocol.HELPER = previous

    def stop_compositor(self):
        if self.compositor.poll() is None:
            self.compositor.terminate()
        try:
            self.compositor.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.compositor.kill()
            self.compositor.wait(timeout=3)

    def test_review_then_password_creates_and_unlocks_on_private_wayland(self):
        self.assertTrue(self.native("AttachSessionWithDisplay", "s", "qindaqt-7")[0])
        collection = secretstorage.create_collection(self.connection, "Synthetic request review", "default")
        item = collection.create_item("Synthetic value", {}, b"synthetic-only")
        collection.lock()
        self.assertFalse(collection.unlock())
        self.assertEqual(item.get_secret(), b"synthetic-only")
        lines = (self.root / "prompt.audit").read_text().splitlines()
        self.assertEqual(len(lines), 2)
        self.assertTrue(all("visible-review visible-approval" in line for line in lines))
        self.assertTrue(lines[0].endswith(" create"))
        self.assertTrue(lines[1].endswith(" unlock"))
        self.assertIsNone(self.compositor.poll())

if __name__ == "__main__":
    unittest.main()
