#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual owner-bound legacy readers and native importer; synthetic services only."""
import os
import pathlib
import select
import struct
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import dbus
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import GLib
from legacy_fixture_services import Fixture
IMPORTER=pathlib.Path(sys.argv.pop(1)).resolve()
COMPOSITOR=pathlib.Path(sys.argv.pop(1)).resolve()
VERIFY=pathlib.Path(sys.argv.pop(1)).resolve()
DBusGMainLoop(set_as_default=True)
class ImportTest(unittest.TestCase):
    def setUp(self):
        self.children=[];self.addCleanup(self.cleanup)
        self.temp=tempfile.TemporaryDirectory(prefix="qindaqt-legacy-import-");self.root=pathlib.Path(self.temp.name)
        self.runtime=self.root/"runtime";self.runtime.mkdir(mode=0o700);self.storage=self.root/"native"
        self.env=dict(os.environ);self.env.pop("WAYLAND_SOCKET",None);self.env.pop("WAYLAND_DISPLAY",None)
        self.env["XDG_RUNTIME_DIR"]=str(self.runtime)
        config=self.root/"bus.conf";config.write_text("<busconfig><type>session</type><listen>unix:path="+str(self.root/"bus")+"</listen><auth>EXTERNAL</auth><policy context='default'><allow send_destination='*' eavesdrop='true'/><allow eavesdrop='true'/><allow own='*'/></policy></busconfig>")
        self.bus=subprocess.Popen(["dbus-daemon","--nofork","--config-file="+str(config),"--print-address=1"],stdout=subprocess.PIPE,stderr=subprocess.DEVNULL)
        self.assertTrue(select.select([self.bus.stdout],[],[],5)[0]);self.address=self.bus.stdout.readline().decode().strip();self.env["DBUS_SESSION_BUS_ADDRESS"]=self.address
        self.loop=GLib.MainLoop();self.thread=threading.Thread(target=self.loop.run);self.thread.start()
        self.fixture=Fixture(self.address);self.session=dbus.bus.BusConnection(self.address)
        self.compositor=subprocess.Popen([str(COMPOSITOR),"qindaqt-7"],env=self.env,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL)
        self.assertTrue(select.select([self.compositor.stdout],[],[],5)[0]);self.assertEqual(self.compositor.stdout.readline(),b"ready\n")
        self.peer=self.session.get_name_owner(subprocess.check_output([str(VERIFY),"--compositor-service"]).decode().strip())
    def cleanup(self):
        for child in self.children:
            if child.poll() is None: child.terminate()
            child.communicate(timeout=5)
        if hasattr(self,"compositor"):
            if self.compositor.poll() is None: self.compositor.terminate()
            self.compositor.wait(timeout=5);self.compositor.stdin.close();self.compositor.stdout.close()
        if hasattr(self,"fixture"): self.fixture.close()
        if hasattr(self,"session"): self.session.close()
        if hasattr(self,"loop"): self.loop.quit();self.thread.join(timeout=5)
        if hasattr(self,"bus"): self.bus.terminate();self.bus.wait(timeout=5);self.bus.stdout.close()
        if hasattr(self,"temp"): self.temp.cleanup()
    def command(self,value):
        self.compositor.stdin.write(value.encode()+b"\n");self.compositor.stdin.flush()
        self.assertTrue(select.select([self.compositor.stdout],[],[],5)[0]);self.assertEqual(self.compositor.stdout.readline(),b"done\n")
    def args(self,fd,kind="both"):
        args=[str(IMPORTER),"--bus-address",self.address,"--session-owner",self.session.get_unique_name(),"--runtime-root",str(self.runtime),
            "--display","qindaqt-7","--compositor-owner",str(self.peer),"--compositor-pid",str(self.compositor.pid),"--storage-root",str(self.storage),"--password-fd",str(fd),"--alias","alternate"]
        if kind in ("both","secret"): args += ["--secret-owner",self.fixture.ss.get_unique_name(),"--secret-pid",str(os.getpid())]
        if kind in ("both","kwallet"): args += ["--kwallet-owner",self.fixture.kw.get_unique_name(),"--kwallet-pid",str(os.getpid())]
        return args
    def run_import(self,kind="both",password=b"B"*8,extra=None):
        read,write=os.pipe();frames=(struct.pack(">I",len(password))+password)*8;os.write(write,frames);os.close(write)
        child=subprocess.Popen(self.args(read,kind)+(extra or []),env=self.env,pass_fds=(read,),stdout=subprocess.PIPE,stderr=subprocess.PIPE);os.close(read);self.children.append(child)
        output,error=child.communicate(timeout=20)
        self.assertNotIn(password,output+error) if password else None
        return child.returncode,output,error
    def test_complete_sources_exact_bytes_restart_idempotence(self):
        code,output,error=self.run_import();self.assertEqual(code,0,error.decode());self.assertIn(b"4 collections, 8 items",output)
        result=subprocess.run([str(VERIFY),str(self.storage),"both"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);self.assertEqual(result.returncode,0)
        before={p.name:p.read_bytes() for p in self.storage.glob("*.qkr")};catalog=(self.storage/"catalog.json").read_bytes()
        code,output,error=self.run_import();self.assertEqual(code,0,error.decode());self.assertIn(b"4 collections unchanged",output)
        self.assertTrue(before=={p.name:p.read_bytes() for p in self.storage.glob("*.qkr")});self.assertTrue(catalog==(self.storage/"catalog.json").read_bytes())
        self.assertEqual(self.fixture.calls.count("openAsync"),self.fixture.calls.count("close"))
        self.assertTrue(set(self.fixture.calls)<=set("Get OpenSession Close Unlock Prompt Dismiss ReadAlias GetSecrets wallets openAsync folderList entryList entryType readEntry close".split()))
    def test_each_provider_is_independently_complete(self):
        for kind in ("secret","kwallet"):
            with self.subTest(kind=kind):
                self.storage=self.root/("native-"+kind);code,output,error=self.run_import(kind);self.assertEqual(code,0,error.decode())
                self.assertEqual(subprocess.run([str(VERIFY),str(self.storage),kind],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode,0)
    def test_owned_unlock_and_cancel_or_failure(self):
        self.fixture.locked=True;code,output,error=self.run_import("secret");self.assertEqual(code,0,error.decode());self.assertIn("Prompt",self.fixture.calls)
    def test_foreign_replies_and_completed_never_publish(self):
        for mode in ("forged-secret","forged-metadata","forged-completed"):
            with self.subTest(mode=mode):
                self.fixture.mode=mode;self.fixture.locked=mode=="forged-completed";self.storage=self.root/mode
                code,output,error=self.run_import("secret");self.assertNotEqual(code,0);self.assertFalse(output);self.assertFalse(self.storage.exists());self.assertIn(b"code 4" if mode=="forged-completed" else b"code 6",error)
    def test_malformed_missing_mutated_cancel_and_replaced_sources(self):
        for mode in ("malformed","missing-secret","mutation","cancel","unlock-failure","kw-cancel","duplicate","oversize","attributes","rate-limit","replacement"):
            with self.subTest(mode=mode):
                self.fixture.mode=mode;self.fixture.secret_calls=0;self.fixture.locked=mode in ("cancel","unlock-failure");self.storage=self.root/mode
                code,output,error=self.run_import("kwallet" if mode=="kw-cancel" else "secret");self.assertNotEqual(code,0);self.assertFalse(output);self.assertFalse(self.storage.exists())
    def test_native_locked_refuses_before_source_secrets(self):
        self.command("lock");code,output,error=self.run_import();self.assertNotEqual(code,0);self.assertEqual(self.fixture.secret_calls,0);self.assertFalse(self.storage.exists())
    def test_destination_password_failure_and_fd_cancel(self):
        code,_,error=self.run_import("secret");self.assertEqual(code,0,error.decode());before={p.name:p.read_bytes() for p in self.storage.iterdir() if p.is_file()}
        code,output,error=self.run_import("secret",b"C"*8);self.assertNotEqual(code,0);self.assertIn(b"code 5",error);self.assertTrue(before=={p.name:p.read_bytes() for p in self.storage.iterdir() if p.is_file()})
        self.storage=self.root/"cancelled";code,output,error=self.run_import("secret",b"");self.assertNotEqual(code,0);self.assertIn(b"code 4",error);self.assertFalse(list(self.storage.glob("*.qkr")))
    def test_closed_partial_and_oversized_password_frames(self):
        for name,frame in (("closed",b""),("header",b"\x00\x00"),("body",struct.pack(">I",8)+b"B"),("oversized",struct.pack(">I",4097))):
            with self.subTest(frame=name):
                self.storage=self.root/("fd-"+name);read,write=os.pipe();os.write(write,frame);os.close(write)
                child=subprocess.Popen(self.args(read,"secret"),env=self.env,pass_fds=(read,),stdout=subprocess.PIPE,stderr=subprocess.PIPE);os.close(read);self.children.append(child)
                output,error=child.communicate(timeout=10);self.assertNotEqual(child.returncode,0);self.assertFalse(output)
                self.assertIn(b"code 1" if name=="oversized" else b"code 4",error);self.assertFalse(list(self.storage.glob("*.qkr")))
    def test_lock_or_session_loss_retires_waiting_password(self):
        for mode in ("lock","mutation","session"):
            if mode!="lock": self.command("unlock")
            with self.subTest(mode=mode):
                self.storage=self.root/("waiting-"+mode);read,write=os.pipe()
                child=subprocess.Popen(self.args(read,"secret"),env=self.env,pass_fds=(read,),stdout=subprocess.PIPE,stderr=subprocess.PIPE);os.close(read);self.children.append(child)
                deadline=time.monotonic()+8
                while not (self.storage/"catalog.json").exists() and child.poll() is None and time.monotonic()<deadline: time.sleep(.02)
                self.assertTrue((self.storage/"catalog.json").exists())
                if mode=="lock": self.command("lock")
                elif mode=="mutation":
                    signal=dbus.lowlevel.SignalMessage("/org/freedesktop/secrets","org.freedesktop.Secret.Service","CollectionChanged")
                    signal.append(dbus.ObjectPath("/legacy/collection/login"),signature="o");self.fixture.ss.send_message(signal)
                elif mode=="source":
                    self.fixture.replacement.request_name("org.freedesktop.secrets",dbus.bus.NAME_FLAG_REPLACE_EXISTING)
                else: self.session.close()
                output,error=child.communicate(timeout=8);os.close(write)
                self.assertNotEqual(child.returncode,0);self.assertFalse(output);self.assertFalse(list(self.storage.glob("*.qkr")))
if __name__=="__main__": unittest.main()
