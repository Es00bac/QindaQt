#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual resident policy, private Settings1 and ordinary native display fixtures."""
import json
import os
import pathlib
import select
import subprocess
import sys
import time
import unittest
import test_secret_service as protocol
SETTINGS=pathlib.Path(sys.argv.pop(1)).resolve()
COMPOSITOR=pathlib.Path(sys.argv.pop(1)).resolve()
SCHEMAS=pathlib.Path(sys.argv.pop(1)).resolve()
import secretstorage
from secretstorage.util import exec_prompt
from secretstorage.exceptions import LockedException
class ResidentTest(unittest.TestCase):
    setUp=protocol.SecretServiceTest.setUp
    call=protocol.SecretServiceTest.call
    native=protocol.SecretServiceTest.native
    control=protocol.SecretServiceTest.control
    stop=protocol.SecretServiceTest.stop
    def cleanup(self):
        if hasattr(self,"settings") and self.settings.poll() is None:
            self.settings.terminate();self.settings.wait(timeout=5)
        protocol.SecretServiceTest.cleanup(self)
    def start(self):
        self.env["XDG_CONFIG_HOME"]=str(self.root/"config")
        self.env["QINDAQT_SETTINGS_SCHEMA_DIR"]=str(SCHEMAS)
        if not hasattr(self,"settings"):
            preferences={"keyring.lockOnScreenLock":False,"keyring.lockAfterIdleMinutes":0}
            if "screen" in self._testMethodName: preferences["keyring.lockOnScreenLock"]=True
            if "idle" in self._testMethodName: preferences["keyring.lockAfterIdleMinutes"]=1
            target=self.root/"config/qindaqt/settings-v2.json";target.parent.mkdir(parents=True)
            target.write_text(json.dumps({"schemaVersion":2,"layer":"user-overrides","values":preferences}))
            self.settings=subprocess.Popen([str(SETTINGS)],env=self.env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        previous=protocol.HELPER;previous_popen=protocol.subprocess.Popen
        protocol.HELPER=pathlib.Path(__file__).with_name("native_scripted_prompt.py")
        def policy(args,*a,**kw):
            if args[0]==str(protocol.DAEMON): args=args+["--policy-fixture"]
            return previous_popen(args,*a,**kw)
        protocol.subprocess.Popen=policy
        try: protocol.SecretServiceTest.start(self)
        finally: protocol.HELPER=previous;protocol.subprocess.Popen=previous_popen
    def state(self):
        return {key:value[1] for key,value in self.native("GetPolicyState")[0].items()}
    def until(self,predicate):
        deadline=time.monotonic()+5
        while time.monotonic()<deadline:
            if predicate(): return
            time.sleep(.05)
        self.fail("private resident policy did not reach expected state")
    def compositor(self):
        child=subprocess.Popen([str(COMPOSITOR),"qindaqt-7"],env=self.env,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL)
        def stop():
            if child.poll() is None: child.terminate()
            child.wait(timeout=5);child.stdin.close();child.stdout.close()
        self.addCleanup(stop)
        self.assertTrue(select.select([child.stdout],[],[],5)[0]);self.assertEqual(child.stdout.readline(),b"ready\n")
        self.assertTrue(self.native("AttachSessionWithDisplay","s","qindaqt-7")[0])
        return child
    def command(self,child,value,expected=b"done\n"):
        child.stdin.write(value.encode()+b"\n");child.stdin.flush()
        self.assertTrue(select.select([child.stdout],[],[],5)[0]);self.assertEqual(child.stdout.readline(),expected)
    def collection(self):
        return secretstorage.create_collection(self.connection,"Synthetic login","default")
    def unlock(self):
        deadline=time.monotonic()+5
        while time.monotonic()<deadline:
            if self.control(1,old=b"synthetic-keyring-password")==b"QKR1\0": return
            time.sleep(.6)
        self.fail("private unlock did not succeed after admitted resume")
    def test_defaults_do_not_lock_on_native_screen_lock(self):
        self.until(lambda:self.state()["SettingsAvailable"]);peer=self.compositor()
        self.until(lambda:self.state()["ScreenLockAvailable"])
        collection=self.collection();item=collection.create_item("fixture",{},b"synthetic-value")
        self.command(peer,"lock");time.sleep(.2)
        self.assertFalse(collection.is_locked());self.assertEqual(item.get_secret(),b"synthetic-value")
        self.assertFalse(self.state()["LockOnScreenLock"]);self.assertEqual(self.state()["LockAfterIdleMinutes"],0)
    def test_screen_lock_retains_policy_after_settings_owner_loss(self):
        self.until(lambda:self.state()["SettingsAvailable"]);peer=self.compositor()
        self.until(lambda:self.state()["ScreenLockAvailable"])
        collection=self.collection();item=collection.create_item("fixture",{},b"synthetic-value")
        self.command(peer,"locking");self.until(collection.is_locked)
        with self.assertRaises(LockedException): item.get_secret()
        self.assertEqual(self.control(1,old=b"synthetic-keyring-password"),b"QKR1\1")
        session=self.call("org.freedesktop.Secret.Service","/org/freedesktop/secrets","OpenSession","sv","plain",("s",""))[1]
        prompt=self.native("ReadSecretWithPrompt","oo",item.item_path,session)[0]
        dismissed,(_,wire)=exec_prompt(self.connection,prompt,timeout=5)
        self.assertTrue(dismissed);self.assertEqual(wire[2],b"")
        self.command(peer,"unlock");time.sleep(.15);self.assertTrue(collection.is_locked());self.unlock()
        self.assertEqual(item.get_secret(),b"synthetic-value")
        self.settings.terminate();self.settings.wait(timeout=5);self.until(lambda:not self.state()["SettingsAvailable"])
        self.command(peer,"lock");self.until(collection.is_locked)
        self.assertTrue(self.state()["LockOnScreenLock"])
        self.command(peer,"release");self.until(lambda:not self.state()["ScreenLockAvailable"])
    def test_true_idle_events_and_peer_loss_lock_without_auto_unlock(self):
        self.until(lambda:self.state()["SettingsAvailable"]);peer=self.compositor()
        self.until(lambda:self.state()["IdleAvailable"]);time.sleep(.1)
        self.command(peer,"timeout",b"timeout:60000\n")
        collection=self.collection();item=collection.create_item("fixture",{},b"synthetic-value")
        self.command(peer,"idle");self.until(collection.is_locked)
        self.command(peer,"resume");time.sleep(.1);self.assertTrue(collection.is_locked());self.unlock()
        self.assertEqual(item.get_secret(),b"synthetic-value")
        self.command(peer,"drop");self.until(lambda:not self.state()["IdleAvailable"]);self.assertTrue(collection.is_locked())
    def test_fixture_policy_requires_explicit_private_roots(self):
        self.stop()
        result=subprocess.run([str(protocol.DAEMON),"--policy-fixture","--storage-root",str(self.root/"other")],env=self.env,capture_output=True,timeout=5)
        # Current owner collision may reject earlier; either path cannot start
        # another policy process or reach a live Settings bus/user directory.
        self.assertEqual(result.returncode,2)
if __name__=="__main__": unittest.main()
