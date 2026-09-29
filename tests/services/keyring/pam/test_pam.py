#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""PAM confdir, credentials, bus and keyrings are synthetic private fixtures."""
import importlib
import os
import pathlib
import select
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest

DAEMON, MODULE, TOKENS, DRIVER, LAUNCHER, INJECTION, UNAVAILABLE = map(pathlib.Path, sys.argv[1:8])
sys.argv[1:8] = [str(DAEMON)]
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
protocol = importlib.import_module("test_secret_service")

class Fixture(unittest.TestCase):
    setUp = protocol.SecretServiceTest.setUp
    cleanup = protocol.SecretServiceTest.cleanup
    stop = protocol.SecretServiceTest.stop
    collection = protocol.SecretServiceTest.collection
    control = protocol.SecretServiceTest.control
    call = protocol.SecretServiceTest.call
    native = protocol.SecretServiceTest.native

    def start(self):
        env = dict(self.env)
        if getattr(self, "injected", False):
            self.marker = self.root / "injected"
            env["LD_PRELOAD"] = str(INJECTION)
            env["QINDAQT_FIXTURE_INJECTION_MARK"] = str(self.marker)
            args = [str(DAEMON), "--private-bus", env["DBUS_SESSION_BUS_ADDRESS"],
                    "--runtime-root", str(self.runtime/"keyring"), "--storage-root", str(self.storage),
                    "--prompt-program", str(protocol.HELPER)]
        else:
            # The child keeps its PID across a sanitized launcher exec.
            args = [str(LAUNCHER), "--fixture", env["DBUS_SESSION_BUS_ADDRESS"], str(self.runtime),
                    str(self.runtime/"keyring"), str(self.storage), str(protocol.HELPER)]
        self.daemon = subprocess.Popen(args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
        deadline = time.monotonic()+8
        while time.monotonic()<deadline:
            if self.daemon.poll() is not None:
                self.fail("private daemon exited during startup")
            connection = None
            try:
                connection = protocol.secretstorage.dbus_init()
                protocol.DBusAddressWrapper("/org/freedesktop/secrets",
                    "org.freedesktop.Secret.Service", connection).get_property("Collections")
                return
            except Exception:
                time.sleep(0.03)
            finally:
                if connection is not None:
                    connection.close()
        self.fail("private daemon startup timeout")

class PamTest(unittest.TestCase):
    def setUp(self):
        self.fixture = Fixture()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.collection = self.fixture.collection()
        self.item = self.collection.create_item("synthetic", {"fixture":"pam"}, b"synthetic-value", True)
        self.assertEqual(self.fixture.control(3), b"QKR1\0")
        self.assertTrue(self.collection.is_locked())
        # Respect the daemon-wide KDF admission interval after create.
        time.sleep(0.55)
        self.config = self.fixture.root/"pam-conf"
        self.config.mkdir(mode=0o700)

    def run_pam(self, mode="good", action="login", runtime=None, owner=None, no_yama=False):
        runtime = runtime or self.fixture.runtime/"keyring"
        owner = owner if owner is not None else self.fixture.daemon.pid
        service = self.config/"synthetic"
        service.write_text(
            f"auth required {TOKENS} {mode}\n"
            f"auth optional {MODULE}\n"
            f"session optional {MODULE} runtime={runtime} owner={owner}\n"
            f"session required {TOKENS}\n"
            f"password required {TOKENS} {mode}\n"
            f"password required {MODULE} runtime={runtime} owner={owner}\n")
        env = {k:v for k,v in self.fixture.env.items() if not k.startswith(("LD_", "PAM_WRAPPER"))}
        if no_yama:
            env.update(LD_PRELOAD=str(UNAVAILABLE),QINDAQT_FIXTURE_NO_YAMA="1")
        return subprocess.run([str(DRIVER),str(self.config),"synthetic",action],env=env,
                              capture_output=True,timeout=8)

    def test_login_unlock_preserves_secret(self):
        result = self.run_pam()
        self.assertEqual(result.returncode,0,result.stdout)
        self.assertFalse(self.collection.is_locked())
        self.assertEqual(self.item.get_secret(),b"synthetic-value")

    def test_already_unlocked_still_accepts_authenticated_token(self):
        self.assertEqual(self.run_pam().returncode,0)
        time.sleep(0.55)
        self.assertEqual(self.run_pam().returncode,0)
        self.assertFalse(self.collection.is_locked())

    def test_bad_or_missing_login_tokens_are_nonfatal_and_locked(self):
        for mode in ("wrong","missing","empty","oversize"):
            with self.subTest(mode=mode):
                self.assertEqual(self.run_pam(mode).returncode,0)
                self.assertTrue(self.collection.is_locked())
                time.sleep(0.55)

    def test_failed_token_allocation_retires_previous_authentication_snapshot(self):
        self.assertEqual(self.run_pam(action="retry-memory-failure").returncode,0)
        self.assertTrue(self.collection.is_locked())

    def test_changed_identity_cannot_consume_login_token(self):
        self.assertEqual(self.run_pam(action="changed-user").returncode,0)
        self.assertTrue(self.collection.is_locked())

    def test_password_change_is_durable_and_old_token_stops_working(self):
        self.assertEqual(self.run_pam(action="password").returncode,0)
        self.assertFalse(self.collection.is_locked())
        self.assertEqual(self.item.get_secret(),b"synthetic-value")
        self.assertEqual(self.fixture.control(3),b"QKR1\0")
        time.sleep(0.55)
        self.assertEqual(self.fixture.control(1,old=b"synthetic-keyring-password"),b"QKR1\1")
        time.sleep(0.55)
        self.assertEqual(self.fixture.control(1,old=b"synthetic-changed-password"),b"QKR1\0")
        self.fixture.stop();self.fixture.start()
        renewed=protocol.secretstorage.Collection(self.fixture.connection,self.collection.collection_path)
        self.assertTrue(renewed.is_locked())
        self.assertEqual(self.fixture.control(1,old=b"synthetic-changed-password"),b"QKR1\0")
        self.assertEqual(list(renewed.get_all_items())[0].get_secret(),b"synthetic-value")

    def test_already_unlocked_rekey_still_authenticates_old_token(self):
        self.assertEqual(self.run_pam().returncode,0)
        self.assertFalse(self.collection.is_locked())
        time.sleep(0.55)
        self.assertNotEqual(self.run_pam("wrong",action="password").returncode,0)
        self.assertTrue(self.collection.is_locked())

    def test_wrong_old_and_missing_password_tokens_never_claim_success(self):
        for mode in ("wrong","no-old","no-new"):
            with self.subTest(mode=mode):
                self.assertNotEqual(self.run_pam(mode,action="password").returncode,0)
                self.assertTrue(self.collection.is_locked())
                time.sleep(0.55)

    def test_failed_save_keeps_original_and_reports_password_failure(self):
        originals = {p.name:p.read_bytes() for p in self.fixture.storage.glob("*.qkr")}
        self.assertTrue(originals)
        self.fixture.storage.chmod(0o755)
        self.addCleanup(self.fixture.storage.chmod,0o700)
        self.assertNotEqual(self.run_pam(action="password").returncode,0)
        self.assertEqual(originals,{p.name:p.read_bytes() for p in self.fixture.storage.glob("*.qkr")})
        self.assertTrue(self.collection.is_locked())

    def test_absent_daemon_login_nonfatal_password_failed(self):
        self.fixture.stop()
        self.assertEqual(self.run_pam(owner=0).returncode,0)
        self.assertNotEqual(self.run_pam(action="password",owner=0).returncode,0)

    def test_bounded_readiness_wait_and_timeout_fallback(self):
        self.fixture.stop()
        args=[str(LAUNCHER),"--fixture",self.fixture.env["DBUS_SESSION_BUS_ADDRESS"],
              str(self.fixture.runtime),str(self.fixture.runtime/"keyring"),str(self.fixture.storage),
              str(protocol.HELPER)]
        script="import os,sys,time;time.sleep(0.35);os.execve(sys.argv[1],sys.argv[1:],dict(os.environ))"
        self.fixture.daemon=subprocess.Popen(["python3","-c",script,*args],env=self.fixture.env,
            stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        self.assertEqual(self.run_pam().returncode,0)
        self.assertFalse(self.collection.is_locked())
        start=time.monotonic()
        self.assertEqual(self.run_pam(runtime=self.fixture.root/"not-ready").returncode,0)
        self.assertLess(time.monotonic()-start,4.5)

    def test_same_uid_fake_peer_gets_zero_token_bytes(self):
        root = self.fixture.root/"fake"
        root.mkdir(mode=0o700)
        listener = socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
        listener.bind(str(root/"control"));(root/"control").chmod(0o600);listener.listen()
        self.addCleanup(listener.close)
        captured = []
        def accept():
            peer,_ = listener.accept()
            with peer:
                peer.settimeout(4)
                captured.append(peer.recv(1))
        worker = threading.Thread(target=accept)
        worker.start()
        self.assertEqual(self.run_pam(runtime=root).returncode,0)
        worker.join(timeout=5)
        self.assertFalse(worker.is_alive())
        self.assertEqual(captured,[b""])
        self.assertTrue(self.collection.is_locked())

    def direct_helper(self, runtime, owner, env):
        parent,child=socket.socketpair()
        helper=MODULE.parent/"qindaqt_pam_fixture_helper"
        process=subprocess.Popen([str(helper),str(os.geteuid()),str(runtime),str(owner)],
            stdin=child,stdout=child,stderr=subprocess.DEVNULL,env=env)
        child.close();parent.settimeout(4)
        try:
            readiness=parent.recv(1)
            process.wait(timeout=4)
            return readiness,process.returncode
        finally:
            parent.close()
            if process.poll() is None:
                process.kill();process.wait()

    def test_parent_refuses_unsupported_yama_before_helper_launch(self):
        self.assertEqual(self.run_pam(no_yama=True).returncode,0)
        self.assertTrue(self.collection.is_locked())
        self.assertNotEqual(self.run_pam(action="password",no_yama=True).returncode,0)

    def test_unavailable_peer_pidfd_rejects_before_readiness_or_payload(self):
        root=self.fixture.root/"unavailable";root.mkdir(mode=0o700)
        listener=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
        listener.bind(str(root/"control"));(root/"control").chmod(0o600);listener.listen()
        self.addCleanup(listener.close)
        env=dict(self.fixture.env,LD_PRELOAD=str(UNAVAILABLE))
        self.assertEqual(self.direct_helper(root,os.getpid(),env),(bytes([1]),1))
        peer,_=listener.accept()
        with peer:
            peer.settimeout(2);self.assertEqual(peer.recv(1),b"")

    def test_dead_credential_process_with_retained_socket_is_rejected(self):
        root=self.fixture.root/"dead-peer";root.mkdir(mode=0o700)
        listener=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
        listener.bind(str(root/"control"));(root/"control").chmod(0o600)
        self.addCleanup(listener.close)
        # listen records the child's credentials; the parent retains the fd.
        script="import os,socket,sys;s=socket.socket(fileno=int(sys.argv[1]));s.listen();os._exit(0)"
        owner=subprocess.Popen(["python3","-c",script,str(listener.fileno())],
            pass_fds=(listener.fileno(),),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        owner.wait(timeout=3)
        self.assertEqual(self.direct_helper(root,owner.pid,self.fixture.env),(bytes([1]),1))
        peer,_=listener.accept()
        with peer:
            peer.settimeout(2);self.assertEqual(peer.recv(1),b"")

    def test_injected_actual_daemon_is_rejected_before_unlock(self):
        other = Fixture();other.injected=True;other.setUp()
        self.addCleanup(other.doCleanups)
        collection = other.collection()
        self.assertEqual(other.control(3),b"QKR1\0")
        self.assertTrue(other.marker.is_file())
        self.assertEqual(self.run_pam(runtime=other.runtime/"keyring").returncode,0)
        self.assertTrue(collection.is_locked())
        self.assertTrue(self.collection.is_locked())

    def test_sanitized_launcher_discards_injection_environment(self):
        marker = self.fixture.root/"discarded-injection"
        env = dict(self.fixture.env,LD_PRELOAD=str(INJECTION),QINDAQT_FIXTURE_INJECTION_MARK=str(marker))
        child = subprocess.run([str(LAUNCHER),"--fixture",env["DBUS_SESSION_BUS_ADDRESS"],
            str(self.fixture.runtime),str(self.fixture.runtime/"other"),str(self.fixture.root/"other"),
            str(protocol.HELPER)],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=5)
        # Constructor ran in the launcher before its trusted exec; the competing
        # daemon cannot own the existing bus name and receives no PAM token.
        self.assertEqual(marker.read_bytes(),b"1")
        self.assertNotEqual(child.returncode,0)
        self.assertEqual(self.run_pam().returncode,0)
        self.assertFalse(self.collection.is_locked())

if __name__ == "__main__":
    unittest.main(verbosity=2)
