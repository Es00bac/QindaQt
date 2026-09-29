#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real private bus/process/socket tests for native prompt display authority."""
import json
import os
import pathlib
import select
import socket
import subprocess
import sys
import time
import unittest
# Reuse fixture lifecycle only; do not inherit unrelated protocol tests.
import test_secret_service as protocol
LAUNCHER=pathlib.Path(sys.argv.pop(1)).resolve()
import secretstorage
from secretstorage.exceptions import PromptDismissedException
from jeepney import DBusAddress,new_method_call

class DisplayTest(unittest.TestCase):
    setUp=protocol.SecretServiceTest.setUp
    cleanup=protocol.SecretServiceTest.cleanup
    stop=protocol.SecretServiceTest.stop
    call=protocol.SecretServiceTest.call
    native=protocol.SecretServiceTest.native
    collection=protocol.SecretServiceTest.collection
    def start(self):
        saved=protocol.HELPER
        protocol.HELPER=pathlib.Path(__file__).with_name("display_prompt.py")
        # Keep the standard client/private lifecycle and add the system-owner fence.
        self.env["WAYLAND_DISPLAY"]="wayland-999"
        self.env["WAYLAND_SOCKET"]="999"
        saved_popen=protocol.subprocess.Popen
        def fenced(args,*a,**kw):
            if args[0]==str(protocol.DAEMON):
                args=[str(LAUNCHER),"--fixture-display",self.env["DBUS_SESSION_BUS_ADDRESS"],
                      str(self.runtime),str(self.runtime/"keyring"),str(self.storage),str(protocol.HELPER)]
            return saved_popen(args,*a,**kw)
        protocol.subprocess.Popen=fenced
        try: protocol.SecretServiceTest.start(self)
        finally:
            protocol.subprocess.Popen=saved_popen;protocol.HELPER=saved
    def compositor(self,name="qindaqt-0"):
        child=subprocess.Popen([sys.executable,str(pathlib.Path(__file__).with_name("fixture_compositor.py")),name],
            env=self.env,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL)
        def close():
            if child.poll() is None: child.terminate()
            child.wait(timeout=5);child.stdin.close();child.stdout.close()
        self.addCleanup(close)
        self.assertTrue(select.select([child.stdout],[],[],5)[0])
        self.assertEqual(child.stdout.readline(),b"ready\n")
        return child
    def command(self,child,value):
        child.stdin.write(value.encode()+b"\n");child.stdin.flush()
        self.assertTrue(select.select([child.stdout],[],[],3)[0])
        self.assertEqual(child.stdout.readline(),b"done\n")
    def attach(self,name="qindaqt-0",connection=None):
        return self.call("org.qindaqt.Keyring1","/org/freedesktop/secrets",
                         "AttachSessionWithDisplay","s",name,connection=connection)[0]
    def denied_prompt(self):
        with self.assertRaises(PromptDismissedException): self.collection()
        self.assertFalse((self.runtime/"prompt-metadata").exists())
        self.assertEqual(list(self.native("ListCollections")[0]),["session"])
    def test_required_default_and_legacy_attachment_do_not_enable_prompt(self):
        self.assertEqual(self.native("AttachSession"),(True,))
        self.denied_prompt()
    def test_canonical_names_missing_owner_and_wrong_prefix_fail_closed(self):
        peer=self.compositor()
        for name in ("","wayland-0","qindaqt-01","qindaqt-4096","qindaqt--1","qindaqt-0/../x",
                     "/tmp/qindaqt-0","qindaqt-0 ","qindaqt-"+"0"*500):
            self.assertFalse(self.attach(name),repr(name))
        self.command(peer,"release")
        self.assertFalse(self.attach())
        self.denied_prompt()
    def test_correct_native_slot_and_exact_connected_fd(self):
        peer=self.compositor("qindaqt-7")
        self.assertTrue(self.attach("qindaqt-7"))
        other=secretstorage.dbus_init()
        try: self.assertFalse(self.attach("qindaqt-7",other))
        finally: other.close()
        collection=self.collection()
        self.assertFalse(collection.is_locked())
        metadata=json.loads((self.runtime/"prompt-metadata").read_text())
        self.assertEqual(metadata,{"display":"qindaqt-7","pid":peer.pid,"uid":os.geteuid()})
        self.assertTrue(list(self.storage.glob("*.qkr")))
    def test_same_uid_fake_socket_must_match_active_compositor_pid(self):
        self.compositor()
        self.runtime.joinpath("qindaqt-0").unlink()
        with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as fake:
            fake.bind(str(self.runtime/"qindaqt-0"));os.chmod(self.runtime/"qindaqt-0",0o600);fake.listen()
            self.assertFalse(self.attach())
            self.denied_prompt()
    def test_path_replacement_after_attachment_is_rejected_before_prompt(self):
        peer=self.compositor();self.assertTrue(self.attach())
        self.command(peer,"unlink")
        with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as fake:
            fake.bind(str(self.runtime/"qindaqt-0"));os.chmod(self.runtime/"qindaqt-0",0o600);fake.listen()
            self.denied_prompt()
    def test_compositor_owner_replacement_revokes_inflight_approval(self):
        peer=self.compositor();self.assertTrue(self.attach())
        properties={"org.freedesktop.Secret.Collection.Label":("s","delayed-fixture")}
        prompt=self.call("org.freedesktop.Secret.Service","/org/freedesktop/secrets",
                         "CreateCollection","a{sv}s",properties,"")[1]
        self.call("org.freedesktop.Secret.Prompt",prompt,"Prompt","s","")
        deadline=time.monotonic()+4
        while not (self.runtime/"prompt-metadata").exists() and time.monotonic()<deadline: time.sleep(.05)
        self.assertTrue((self.runtime/"prompt-metadata").exists())
        self.command(peer,"release")
        replacement=secretstorage.dbus_init()
        try:
            address=DBusAddress("/org/freedesktop/DBus","org.freedesktop.DBus","org.freedesktop.DBus")
            replacement.send_and_get_reply(new_method_call(address,"RequestName","su",("org.qindaqt.KWin",4)))
            time.sleep(2.3)
            self.assertEqual(list(self.native("ListCollections")[0]),["session"])
            self.assertFalse(list(self.storage.glob("*.qkr")))
            self.runtime.joinpath("prompt-metadata").unlink()
            self.denied_prompt()
        finally: replacement.close()
    def test_socket_lineage_loss_revokes_binding(self):
        peer=self.compositor();self.assertTrue(self.attach())
        self.command(peer,"drop");time.sleep(.3)
        self.denied_prompt()
    def test_supervisor_uses_current_native_display_metadata(self):
        self.compositor("qindaqt-7")
        helper=protocol.DAEMON.parents[3]/"tests"/"services"/"keyring"/"qindaqt_keyring_session_owner_helper"
        env=dict(self.env,WAYLAND_DISPLAY="qindaqt-7")
        result=subprocess.run([str(helper)],env=env,capture_output=True,timeout=5)
        self.assertEqual(result.returncode,0)
        self.daemon.wait(timeout=5)
        self.assertEqual(self.daemon.returncode,0)
    def test_session_owner_disconnect_ends_system_owner(self):
        self.compositor()
        owner=secretstorage.dbus_init()
        self.assertTrue(self.attach(connection=owner));owner.close()
        self.daemon.wait(timeout=5)
        self.assertEqual(self.daemon.returncode,0)

if __name__=="__main__": unittest.main()
