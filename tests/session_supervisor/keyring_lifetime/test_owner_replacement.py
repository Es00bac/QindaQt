#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real resident observer recovery on disposable buses and software Wayland.

No prompt, secret, collection, live service, physical device or installed helper
is used. Each supplied executable is the separately built production/fixture
artifact; this runner never changes daemon admission or lock policy.
"""
import json
import os
from pathlib import Path
import select
import signal
import subprocess
import sys
import tempfile
import time
import unittest

OWNER, DAEMON, COMPOSITOR, SETTINGS, SCHEMAS = map(lambda value: Path(value).resolve(), sys.argv[1:6])
RUN_PRIVATE = "--private-fixture" in sys.argv[6:]
NATIVE = "org.qindaqt.Keyring1"
ROOT = "/org/freedesktop/secrets"

if not RUN_PRIVATE:
    # No installed activation dirs and no inherited session connection. All XDG
    # state belongs to this invocation, including Settings and empty keyring.
    with tempfile.TemporaryDirectory(prefix="qkr-renew-") as directory:
        home = Path(directory)
        env = os.environ.copy()
        for key in ("DBUS_SESSION_BUS_ADDRESS", "DBUS_STARTER_ADDRESS", "DBUS_STARTER_BUS_TYPE",
                    "DISPLAY", "WAYLAND_DISPLAY", "WAYLAND_SOCKET"):
            env.pop(key, None)
        for key, child in (("HOME", "home"), ("XDG_CONFIG_HOME", "config"),
                           ("XDG_DATA_HOME", "data"), ("XDG_CACHE_HOME", "cache"),
                           ("XDG_RUNTIME_DIR", "runtime")):
            target = home / child
            target.mkdir(mode=0o700)
            env[key] = str(target)
        env.update(QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software",
                   DBUS_SYSTEM_BUS_ADDRESS="unix:path=/nonexistent",
                   QINDAQT_SETTINGS_SCHEMA_DIR=str(SCHEMAS))
        command = ["dbus-run-session", "--config-file=" + str(Path(__file__).with_name("private-bus.conf")),
                   "--", sys.executable, str(Path(__file__).resolve()), *map(str, (OWNER, DAEMON, COMPOSITOR, SETTINGS, SCHEMAS)),
                   "--private-fixture"]
        private = subprocess.Popen(command, env=env, start_new_session=True)
        try:
            code = private.wait(timeout=35)
        except subprocess.TimeoutExpired:
            os.killpg(private.pid, signal.SIGTERM)
            try: private.wait(timeout=3)
            except subprocess.TimeoutExpired:
                os.killpg(private.pid, signal.SIGKILL)
                private.wait(timeout=3)
            raise
        raise SystemExit(code)

import dbus

class OwnerReplacementTest(unittest.TestCase):
    def setUp(self):
        self.children = []
        self.bus = dbus.bus.BusConnection(os.environ["DBUS_SESSION_BUS_ADDRESS"])
        self.bus.set_exit_on_disconnect(False)
        self.addCleanup(self.cleanup)
        self.registry = dbus.Interface(self.bus.get_object("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                                          introspect=False), "org.freedesktop.DBus")
        self.root = Path(tempfile.mkdtemp(prefix="case-", dir=os.environ["XDG_DATA_HOME"]))
        self.env = os.environ.copy()
        self.env["XDG_CONFIG_HOME"] = str(self.root / "config")
        self.env["WAYLAND_DISPLAY"] = "qindaqt-7"
        config = self.root / "config/qindaqt/settings-v2.json"
        config.parent.mkdir(mode=0o700, parents=True)
        config.write_text(json.dumps({"schemaVersion": 2, "layer": "user-overrides", "values": {
            "keyring.lockOnScreenLock": True, "keyring.lockAfterIdleMinutes": 1}}))
        self.spawn([SETTINGS])
        self.compositor = self.spawn([COMPOSITOR, "qindaqt-7"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self.assertEqual(self.line(self.compositor), b"ready\n")
        self.daemon = self.start_daemon()
        self.until(lambda: self.state().get("SettingsAvailable", False))
        baseline = self.state()
        self.assertTrue(baseline["LockOnScreenLock"])
        self.assertEqual(baseline["LockAfterIdleMinutes"], 1)
        self.assertFalse(baseline["IdleAvailable"])
        self.assertFalse(baseline["ScreenLockAvailable"])
        self.assertTrue(baseline["ScreenLocked"])
    def cleanup(self):
        for child in reversed(self.children):
            if child.poll() is None:
                child.terminate()
                try: child.wait(timeout=3)
                except subprocess.TimeoutExpired: child.kill(); child.wait(timeout=3)
            for stream in (child.stdin, child.stdout, child.stderr):
                if stream is not None: stream.close()
        self.assertTrue(all(child.poll() is not None for child in self.children))
        print("private_children_reaped=" + str(len(self.children)) + "; survivors=0", flush=True)
        self.bus.close()
    def spawn(self, arguments, **kwargs):
        child = subprocess.Popen(list(map(str, arguments)), env=self.env,
                                 stderr=subprocess.DEVNULL, **kwargs)
        self.children.append(child)
        return child
    def line(self, child):
        self.assertTrue(select.select([child.stdout], [], [], 3)[0], "fixture readiness/command timed out")
        return child.stdout.readline()
    def until(self, condition):
        limit = time.monotonic() + 5
        while time.monotonic() < limit:
            if condition(): return
            time.sleep(0.03)
        self.fail("private owner/policy convergence timed out")
    def owner(self):
        try: return str(self.registry.GetNameOwner(NATIVE, timeout=0.25))
        except dbus.DBusException: return ""
    def start_daemon(self):
        child = self.spawn([DAEMON, "--private-bus", self.env["DBUS_SESSION_BUS_ADDRESS"], "--policy-fixture", "--require-session-display",
                            "--storage-root", self.root / "storage", "--runtime-root", self.root / "control",
                            "--prompt-program", "/bin/false"])
        self.until(lambda: bool(self.owner()))
        unique = self.owner()
        self.assertEqual(str(self.registry.GetNameOwner("org.freedesktop.secrets", timeout=0.25)), unique)
        self.assertEqual(int(self.registry.GetConnectionUnixUser(unique, timeout=0.25)), os.geteuid())
        self.assertEqual(int(self.registry.GetConnectionUnixProcessID(unique, timeout=0.25)), child.pid)
        return child
    def state(self):
        unique = self.owner()
        if not unique: return {}
        native = dbus.Interface(self.bus.get_object(unique, ROOT, introspect=False), NATIVE)
        try: return dict(native.GetPolicyState(timeout=0.5))
        except dbus.DBusException: return {}  # name acquisition can precede object publication
    def start_owner(self, display="qindaqt-7"):
        self.env["WAYLAND_DISPLAY"] = display
        child = self.spawn([OWNER], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self.assertEqual(self.line(child), b"ready\n")
        return child
    def admitted(self):
        state = self.state()
        return state.get("IdleAvailable", False) and state.get("ScreenLockAvailable", False) and not state.get("ScreenLocked", True)
    def command(self, value, expected=b"done\n"):
        self.compositor.stdin.write(value + b"\n")
        self.compositor.stdin.flush()
        self.assertEqual(self.line(self.compositor), expected)
    def test_replacement_recovers_actual_resident_observers(self):
        owner = self.start_owner()
        self.until(self.admitted)
        original = self.owner()
        self.daemon.terminate()
        self.assertEqual(self.daemon.wait(timeout=3), 0)
        self.until(lambda: not self.owner())
        self.daemon = self.start_daemon()
        self.assertNotEqual(self.owner(), original)
        self.until(self.admitted)
        policy = self.state()
        self.assertTrue(policy["LockOnScreenLock"])
        self.assertEqual(policy["LockAfterIdleMinutes"], 1)
        self.command(b"timeout", b"timeout:60000\n")
        self.command(b"lock")
        self.until(lambda: self.state().get("ScreenLocked", False))
        self.command(b"unlock")
        self.until(self.admitted)
        self.command(b"idle")
        self.command(b"resume")
        self.until(self.admitted)
        owner.stdin.write(b"stop\n"); owner.stdin.flush()
        self.assertEqual(owner.wait(timeout=3), 0)
        self.assertEqual(self.daemon.wait(timeout=3), 0)
        self.until(lambda: not self.owner())
    def test_refused_non_native_display_remains_fail_closed_and_is_not_shutdown(self):
        owner = self.start_owner("wayland-0")
        time.sleep(0.7)
        state = self.state()
        self.assertFalse(state["ScreenLockAvailable"])
        self.assertFalse(state["IdleAvailable"])
        self.assertTrue(state["ScreenLocked"])
        unique = self.owner()
        owner.stdin.write(b"stop\n"); owner.stdin.flush()
        self.assertEqual(owner.wait(timeout=3), 0)
        time.sleep(0.1)
        self.assertIsNone(self.daemon.poll())
        self.assertEqual(self.owner(), unique)

sys.argv = [sys.argv[0]]
unittest.main()
