#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""All bus, storage and client operations are confined to temporary fixtures."""
import os
import pathlib
import select
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
import xml.etree.ElementTree as ET
import secretstorage
from jeepney import DBusAddress, new_method_call, DBusErrorResponse, MessageType
from secretstorage.util import DBusAddressWrapper

DAEMON = pathlib.Path(sys.argv.pop(1)).resolve()
HELPER = pathlib.Path(__file__).with_name("scripted_prompt.py")

class SecretServiceTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="qindaqt-keyring-protocol-")
        self.root = pathlib.Path(self.temp.name)
        self.storage = self.root / "keyring"
        self.runtime = self.root / "runtime"
        self.runtime.mkdir(mode=0o700)
        self.env = dict(os.environ)
        self.env.pop("WAYLAND_DISPLAY",None)
        self.env.pop("WAYLAND_SOCKET",None)
        self.env["XDG_RUNTIME_DIR"] = str(self.runtime)
        self.env["XDG_DATA_HOME"] = str(self.root / "data")
        self.env["DBUS_SESSION_BUS_ADDRESS"] = "unix:path=" + str(self.root / "bus")
        config = self.root / "bus.conf"
        config.write_text('<busconfig><type>session</type><listen>' + self.env["DBUS_SESSION_BUS_ADDRESS"]
            + '</listen><auth>EXTERNAL</auth><policy context="default"><allow own="*"/>'
              '<allow send_destination="*"/><allow receive_sender="*"/></policy></busconfig>')
        self.bus = subprocess.Popen(["dbus-daemon","--nofork","--config-file="+str(config),"--print-address=1"],
            stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,env=self.env)
        self.addCleanup(self.cleanup)
        ready,_,_ = select.select([self.bus.stdout],[],[],5)
        self.assertTrue(ready)
        self.env["DBUS_SESSION_BUS_ADDRESS"] = self.bus.stdout.readline().decode().strip()
        self.previous_bus = os.environ.get("DBUS_SESSION_BUS_ADDRESS")
        os.environ["DBUS_SESSION_BUS_ADDRESS"] = self.env["DBUS_SESSION_BUS_ADDRESS"]
        self.start()
        self.connection = secretstorage.dbus_init()

    def cleanup(self):
        if hasattr(self,"connection"):
            self.connection.close()
        if hasattr(self,"daemon"):
            self.stop()
        if hasattr(self,"bus"):
            self.bus.terminate()
            self.bus.wait(timeout=5)
            self.bus.stdout.close()
        if getattr(self,"previous_bus",None) is None:
            os.environ.pop("DBUS_SESSION_BUS_ADDRESS",None)
        else:
            os.environ["DBUS_SESSION_BUS_ADDRESS"] = self.previous_bus
        self.temp.cleanup()

    def start(self):
        self.daemon = subprocess.Popen([str(DAEMON),"--storage-root",str(self.storage),
            "--private-bus",self.env["DBUS_SESSION_BUS_ADDRESS"],"--runtime-root",str(self.runtime/"keyring"),
            "--prompt-program",str(HELPER)],
            stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,env=self.env)
        deadline = time.monotonic()+8
        while time.monotonic()<deadline:
            if self.daemon.poll() is not None:
                self.fail("fixture daemon startup failed: "+str(self.daemon.returncode))
            c = None
            try:
                c = secretstorage.dbus_init()
                iface = DBusAddressWrapper("/org/freedesktop/secrets","org.freedesktop.Secret.Service",c)
                iface.get_property("Collections")
                c.close()
                return
            except Exception:
                time.sleep(0.05)
            finally:
                if c is not None: c.close()
        self.fail("fixture daemon startup timeout")

    def stop(self):
        self.daemon.terminate()
        self.daemon.wait(timeout=5)

    def collection(self):
        return secretstorage.create_collection(self.connection,"Synthetic login","default")

    def tool(self,*args,input=None):
        return subprocess.run(["secret-tool",*args],input=input,capture_output=True,env=self.env,timeout=12)

    def call(self,interface,path,method,signature="",*args,connection=None):
        address = DBusAddress(path,"org.freedesktop.secrets",interface)
        reply = (connection or self.connection).send_and_get_reply(
            new_method_call(address,method,signature,args))
        if reply.header.message_type == MessageType.error:
            raise DBusErrorResponse(reply)
        return reply.body

    def native(self,method,signature="",*args):
        return self.call("org.qindaqt.Keyring1","/org/freedesktop/secrets",method,signature,*args)

    def control(self,operation,collection="login",old=b"",new=b"",raw=None):
        import socket
        name = collection.encode()
        frame = raw if raw is not None else b"QKR1"+bytes([operation,len(name)])+len(old).to_bytes(2,"big")+len(new).to_bytes(2,"big")+name+old+new
        with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as peer:
            peer.settimeout(4)
            peer.connect(str(self.runtime/"keyring"/"control"))
            peer.sendall(frame)
            result = peer.recv(5)
        return result

    def test_standard_introspection_exposes_service_and_collection_contracts(self):
        root = "/org/freedesktop/secrets"
        service_xml = self.call("org.freedesktop.DBus.Introspectable",root,"Introspect")[0]
        service_interfaces = {node.attrib["name"] for node in ET.fromstring(service_xml).findall("interface")}
        self.assertIn("org.freedesktop.Secret.Service",service_interfaces)
        self.assertIn("org.qindaqt.Keyring1",service_interfaces)
        self.assertIn("org.freedesktop.DBus.Properties",service_interfaces)

        collection = self.collection()
        collection_xml = self.call("org.freedesktop.DBus.Introspectable",
                                   collection.collection_path,"Introspect")[0]
        collection_interfaces = {node.attrib["name"] for node in ET.fromstring(collection_xml).findall("interface")}
        self.assertIn("org.freedesktop.Secret.Collection",collection_interfaces)
        self.assertNotIn("org.qindaqt.Keyring1",collection_interfaces)

    def test_plain_session_and_cross_caller_close_denial(self):
        collection = self.collection()
        session = self.call("org.freedesktop.Secret.Service","/org/freedesktop/secrets","OpenSession","sv","plain",("s",""))[1]
        properties = {"org.freedesktop.Secret.Item.Label":("s","plain fixture"),
                      "org.freedesktop.Secret.Item.Attributes":("a{ss}",{"wire":"plain"})}
        path = self.call("org.freedesktop.Secret.Collection",collection.collection_path,"CreateItem",
                         "a{sv}(oayays)b",properties,(session,b"",b"plain-synthetic","text/plain"),False)[0]
        secret = self.call("org.freedesktop.Secret.Item",path,"GetSecret","o",session)[0]
        self.assertEqual(secret[2],b"plain-synthetic")
        other = secretstorage.dbus_init()
        try:
            with self.assertRaises(DBusErrorResponse):
                self.call("org.freedesktop.Secret.Item",path,"GetSecret","o",session,connection=other)
            with self.assertRaises(DBusErrorResponse):
                self.call("org.freedesktop.Secret.Session",session,"Close",connection=other)
        finally:
            other.close()
        self.call("org.freedesktop.Secret.Session",session,"Close")
        with self.assertRaises(DBusErrorResponse):
            self.call("org.freedesktop.Secret.Item",path,"GetSecret","o",session)

    def test_prompt_owner_denial_cancel_and_disconnect(self):
        other = secretstorage.dbus_init()
        try:
            properties = {"org.freedesktop.Secret.Collection.Label":("s","delayed-fixture")}
            prompt = self.call("org.freedesktop.Secret.Service","/org/freedesktop/secrets",
                               "CreateCollection","a{sv}s",properties,"")[1]
            with self.assertRaises(DBusErrorResponse):
                self.call("org.freedesktop.Secret.Prompt",prompt,"Dismiss",connection=other)
            self.call("org.freedesktop.Secret.Prompt",prompt,"Prompt","s","")
            time.sleep(0.7)
            self.call("org.freedesktop.Secret.Prompt",prompt,"Dismiss")
            time.sleep(1.2)
            self.assertEqual(list(self.native("ListCollections")[0]),["session"])
            prompt = self.call("org.freedesktop.Secret.Service","/org/freedesktop/secrets",
                               "CreateCollection","a{sv}s",properties,"",connection=other)[1]
            self.call("org.freedesktop.Secret.Prompt",prompt,"Prompt","s","",connection=other)
            time.sleep(0.7)
            other.close()
            time.sleep(1.2)
            self.assertEqual(list(self.native("ListCollections")[0]),["session"])
            self.assertEqual(list(self.storage.glob("*.qkr")),[])
        finally:
            other.close()

    def test_control_unlock_rekey_bounds_and_dump_suppression(self):
        collection = self.collection()
        item = collection.create_item("fixture",{"service":"control"},b"saved")
        time.sleep(0.6)
        self.assertEqual(self.control(3),b"QKR1\x00")
        self.assertTrue(collection.is_locked())
        self.assertEqual(self.control(1,old=b"incorrect"),b"QKR1\x01")
        time.sleep(0.6)
        self.assertEqual(self.control(1,old=b"synthetic-keyring-password"),b"QKR1\x00")
        self.assertEqual(item.get_secret(),b"saved")
        time.sleep(0.6)
        self.assertEqual(self.control(2,old=b"synthetic-keyring-password",new=b"changed-synthetic"),b"QKR1\x00")
        self.assertEqual(self.control(3),b"QKR1\x00")
        time.sleep(0.6)
        self.assertEqual(self.control(1,old=b"synthetic-keyring-password"),b"QKR1\x01")
        time.sleep(0.6)
        self.assertEqual(self.control(1,old=b"changed-synthetic"),b"QKR1\x00")
        self.assertEqual(item.get_secret(),b"saved")
        self.assertEqual(self.control(1,old=b""),b"QKR1\x01")
        self.assertEqual(self.control(1,raw=b"QKR1"+bytes([1,5])+b"\xff\xff\0\0"),b"QKR1\x01")
        core_limits = pathlib.Path("/proc",str(self.daemon.pid),"limits").read_text()
        self.assertRegex(core_limits,r"Max core file size\s+0\s+0")
        with self.assertRaises(PermissionError):
            pathlib.Path("/proc",str(self.daemon.pid),"mem").open("rb")
        self.assertEqual((self.runtime/"keyring"/"control").stat().st_mode & 0o777,0o600)

    def test_one_authenticated_password_unlocks_matching_collections(self):
        login = self.collection()
        time.sleep(0.6)
        imported = secretstorage.create_collection(self.connection,"Imported wallet","imported")
        imported.create_item("saved",{"source":"imported"},b"preserved")
        login.lock()
        imported.lock()
        self.assertTrue(login.is_locked())
        self.assertTrue(imported.is_locked())
        time.sleep(0.6)
        self.assertEqual(self.control(1,old=b"synthetic-keyring-password"),b"QKR1\x00")
        deadline = time.monotonic()+4
        while imported.is_locked() and time.monotonic()<deadline:
            time.sleep(0.05)
        self.assertFalse(imported.is_locked())
        self.assertEqual(list(imported.get_all_items())[0].get_secret(),b"preserved")

    def test_consolidation_copies_before_retiring_and_survives_restart(self):
        login = self.collection()
        login.create_item("existing", {"source":"login"}, b"original")
        time.sleep(0.6)
        imported = secretstorage.create_collection(self.connection,"Imported wallet","")
        imported.create_item("first", {"source":"wallet-one"}, b"first-secret")
        imported.create_item("second", {"source":"wallet-two"}, b"second-secret")
        time.sleep(0.6)
        empty = secretstorage.create_collection(self.connection,"Empty wallet","")
        self.assertEqual(self.native("ConsolidateCollections","oao",
                         login.collection_path,[imported.collection_path,empty.collection_path])[0],2)
        self.assertEqual(set(self.native("ListCollections")[0]),{"login","session"})
        self.assertEqual({item.get_attributes()["source"]:item.get_secret()
                          for item in login.get_all_items()},
                         {"login":b"original","wallet-one":b"first-secret",
                          "wallet-two":b"second-secret"})
        self.assertEqual({file.name for file in self.storage.glob("*.qkr")},{"login.qkr"})
        self.stop()
        self.start()
        login = secretstorage.get_default_collection(self.connection)
        self.assertTrue(login.is_locked())
        self.assertFalse(login.unlock())
        self.assertEqual({item.get_attributes()["source"]:item.get_secret()
                          for item in login.get_all_items()},
                         {"login":b"original","wallet-one":b"first-secret",
                          "wallet-two":b"second-secret"})

    def test_consolidation_refuses_locked_source_without_moving_items(self):
        login = self.collection()
        time.sleep(0.6)
        imported = secretstorage.create_collection(self.connection,"Imported wallet","")
        imported.create_item("source", {"source":"locked"}, b"preserved")
        imported.lock()
        with self.assertRaises(DBusErrorResponse):
            self.native("ConsolidateCollections","oao",
                        login.collection_path,[imported.collection_path])
        self.assertEqual(len(list(login.get_all_items())),0)
        self.assertEqual(len(self.native("ListCollections")[0]),3)
        self.assertTrue(imported.is_locked())
        self.assertEqual(len(list(self.storage.glob("*.qkr"))),2)

    def test_volatile_collection_has_no_disk_secret_and_lock_retires(self):
        collection = secretstorage.Collection(self.connection,"/org/freedesktop/secrets/collection/session")
        item = collection.create_item("volatile",{"service":"volatile"},b"session-only")
        self.assertEqual(item.get_secret(),b"session-only")
        collection.lock()
        self.assertTrue(collection.is_locked())
        self.assertFalse(collection.unlock())
        self.assertEqual(list(collection.get_all_items()),[])
        self.assertEqual(list(self.storage.glob("*.qkr")),[])

    def test_loaded_search_integrity_flag_and_tampering_fail_closed(self):
        collection = self.collection()
        collection.create_item("saved",{"service":"index"},b"saved")
        self.stop()
        file = self.storage/"login.qkr"
        data = bytearray(file.read_bytes())
        data[48] ^= 1  # Public search key is part of authenticated AAD.
        file.write_bytes(data)
        self.start()
        state = self.native("ListCollections")[0]["login"][1]
        self.assertTrue(state["Locked"][1])
        self.assertFalse(state["IndexAuthenticated"][1])
        collection = secretstorage.get_default_collection(self.connection)
        self.assertTrue(collection.unlock())
        self.assertTrue(collection.is_locked())
        self.assertFalse(self.native("ListCollections")[0]["login"][1]["IndexAuthenticated"][1])

    def test_no_persistence_success_when_directory_is_unsafe(self):
        collection = self.collection()
        item = collection.create_item("saved",{"service":"persistence"},b"original")
        original = (self.storage/"login.qkr").read_bytes()
        self.storage.chmod(0o755)
        try:
            with self.assertRaises(DBusErrorResponse):
                item.set_secret(b"must-not-acknowledge")
        finally:
            self.storage.chmod(0o700)
        self.assertEqual((self.storage/"login.qkr").read_bytes(),original)
        self.assertTrue(collection.is_locked())

    def test_native_shutdown_releases_control_and_bus_owner(self):
        self.collection()
        self.native("Shutdown")
        self.daemon.wait(timeout=5)
        self.assertEqual(self.daemon.returncode,0)
        self.assertFalse((self.runtime/"keyring"/"control").exists())


    def test_attached_session_disconnect_ends_activated_owner(self):
        owner = secretstorage.dbus_init()
        other = secretstorage.dbus_init()
        try:
            self.assertEqual(self.call("org.qindaqt.Keyring1","/org/freedesktop/secrets","AttachSession",connection=owner),(True,))
            self.assertEqual(self.call("org.qindaqt.Keyring1","/org/freedesktop/secrets","AttachSession",connection=other),(False,))
            owner.close()
            self.daemon.wait(timeout=5)
            self.assertEqual(self.daemon.returncode,0)
            self.assertFalse((self.runtime/"keyring"/"control").exists())
        finally:
            owner.close(); other.close()

    def test_bus_loss_ends_secret_state_and_control(self):
        self.collection()
        self.bus.terminate()
        self.bus.wait(timeout=5)
        self.daemon.wait(timeout=5)
        self.assertEqual(self.daemon.returncode,1)
        self.assertFalse((self.runtime/"keyring"/"control").exists())

    def test_sole_writer_on_independent_private_bus(self):
        self.collection()
        config = self.root/"second.conf"
        config.write_text('<busconfig><type>session</type><listen>unix:path='+str(self.root/"second-bus")
            +'</listen><auth>EXTERNAL</auth><policy context="default"><allow own="*"/>'
             '<allow send_destination="*"/><allow receive_sender="*"/></policy></busconfig>')
        bus = subprocess.Popen(["dbus-daemon","--nofork","--config-file="+str(config),"--print-address=1"],
                               stdout=subprocess.PIPE,stderr=subprocess.DEVNULL)
        try:
            ready,_,_ = select.select([bus.stdout],[],[],5)
            self.assertTrue(ready)
            address = bus.stdout.readline().decode().strip()
            second = subprocess.run([str(DAEMON),"--private-bus",address,"--storage-root",str(self.storage),
                "--runtime-root",str(self.runtime/"other")],env=self.env,capture_output=True,timeout=5)
            self.assertEqual(second.returncode,2)
            self.assertEqual(second.stdout+second.stderr,b"")
            self.assertIn("login",self.native("ListCollections")[0])
        finally:
            bus.terminate();bus.wait(timeout=5);bus.stdout.close()

    def test_catalog_visibility_and_orphan_recovery(self):
        collection = self.collection()
        collection.create_item("private item label",{"private":"attribute"},b"private-secret")
        catalog = (self.storage/"catalog.json").read_bytes()
        self.assertNotIn(b"private item label",catalog)
        self.assertNotIn(b"attribute",catalog)
        self.assertNotIn(b"private-secret",catalog)
        self.stop()
        shutil.copyfile(self.storage/"login.qkr",self.storage/"orphan.qkr")
        (self.storage/"orphan.qkr").chmod(0o600)
        self.start()
        self.assertFalse((self.storage/"orphan.qkr").exists())
        self.assertEqual(set(self.native("ListCollections")[0]),{"login","session"})
        collection = secretstorage.get_default_collection(self.connection)
        self.assertFalse(collection.unlock())
        collection.delete()
        self.assertFalse((self.storage/"login.qkr").exists())
        self.stop(); self.start()
        self.assertEqual(set(self.native("ListCollections")[0]),{"session"})
        self.assertEqual(self.call("org.freedesktop.Secret.Service","/org/freedesktop/secrets","ReadAlias","s","default"),("/",))

    def test_native_rekey_and_object_validation(self):
        collection = self.collection()
        collection.create_item("fixture",{"native":"rekey"},b"kept")
        session = self.call("org.freedesktop.Secret.Service","/org/freedesktop/secrets","OpenSession","sv","plain",("s",""))[1]
        self.assertEqual(self.native("ChangePassword","o(oayays)(oayays)",collection.collection_path,
            (session,b"",b"synthetic-keyring-password","text/plain"),(session,b"",b"native-changed","text/plain")),(True,))
        self.assertEqual(self.control(3),b"QKR1\x00")
        time.sleep(0.6)
        self.assertEqual(self.control(1,old=b"native-changed"),b"QKR1\x00")
        with self.assertRaises(DBusErrorResponse):
            self.call("org.freedesktop.DBus.Properties",collection.collection_path+"/missing","GetAll","s","org.freedesktop.Secret.Collection")
        with self.assertRaises(DBusErrorResponse):
            self.call("org.freedesktop.Secret.Service","/org/freedesktop/secrets","OpenSession","sv",
                "dh-ietf1024-sha256-aes128-cbc-pkcs7",("ay",b"\0"))

    def test_supervisor_collaborator_adopts_and_retires_owner(self):
        if os.environ.get("QINDAQT_KEYRING_OWNER_HELPER") == "disabled":
            self.skipTest("Supervisor target excluded from this build")
        helper = DAEMON.parents[3]/"tests"/"services"/"keyring"/"qindaqt_keyring_session_owner_helper"
        result = subprocess.run([str(helper)],env=self.env,capture_output=True,timeout=5)
        self.assertEqual(result.returncode,0)
        self.daemon.wait(timeout=5)
        self.assertEqual(self.daemon.returncode,0)

    def test_activated_control_descriptor_uses_real_peer_protocol(self):
        import socket
        self.collection();self.stop()
        path = self.runtime/"keyring"/"control"
        listener = socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
        listener.bind(str(path));path.chmod(0o600);listener.listen(8)
        command = [str(DAEMON),"--storage-root",str(self.storage),"--private-bus",
            self.env["DBUS_SESSION_BUS_ADDRESS"],"--runtime-root",str(self.runtime/"keyring")]
        wrapper = ("import os,sys; fd=int(sys.argv[1]); os.dup2(fd,3); os.set_inheritable(3,True); "
            "os.environ['LISTEN_FDS']='1'; os.environ['LISTEN_PID']=str(os.getpid()); os.execv(sys.argv[2],sys.argv[2:])")
        try:
            self.daemon = subprocess.Popen([sys.executable,"-c",wrapper,str(listener.fileno()),*command],
                pass_fds=(listener.fileno(),),env=self.env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            time.sleep(0.4)
            self.assertIsNone(self.daemon.poll())
            self.assertEqual(self.control(1,old=b"synthetic-keyring-password"),b"QKR1\x00")
            self.stop()
            self.assertTrue(path.exists()) # Activated listener remains owned by the socket manager fixture.
        finally:
            listener.close()

    def test_catalog_failure_never_acknowledges_alias(self):
        self.collection()
        original = (self.storage/"catalog.json").read_bytes()
        self.storage.chmod(0o755)
        try:
            with self.assertRaises(DBusErrorResponse):
                self.call("org.freedesktop.Secret.Service","/org/freedesktop/secrets","SetAlias","so",
                          "uncommitted","/org/freedesktop/secrets/collection/login")
            self.daemon.wait(timeout=5)
            self.assertEqual(self.daemon.returncode,1)
        finally:
            self.storage.chmod(0o700)
        self.assertEqual((self.storage/"catalog.json").read_bytes(),original)
        self.start()
        self.assertEqual(self.call("org.freedesktop.Secret.Service","/org/freedesktop/secrets","ReadAlias","s","uncommitted"),("/",))

    def test_catalog_symlink_and_missing_declared_file_refuse(self):
        self.collection();self.stop()
        catalog = self.storage/"catalog.json"
        other = self.root/"catalog-copy"
        shutil.copyfile(catalog,other);other.chmod(0o600)
        original = other.read_bytes()
        catalog.unlink();catalog.symlink_to(other)
        command = [str(DAEMON),"--private-bus",self.env["DBUS_SESSION_BUS_ADDRESS"],"--storage-root",
                   str(self.storage),"--runtime-root",str(self.runtime/"keyring")]
        result = subprocess.run(command,env=self.env,capture_output=True,timeout=5)
        self.assertEqual(result.returncode,2)
        self.assertEqual(other.read_bytes(),original)
        catalog.unlink();shutil.copyfile(other,catalog);catalog.chmod(0o600)
        (self.storage/"login.qkr").unlink()
        result = subprocess.run(command,env=self.env,capture_output=True,timeout=5)
        self.assertEqual(result.returncode,2)
        self.assertEqual(result.stdout+result.stderr,b"")

    def test_secret_tool_actual_libsecret_dh(self):
        result = self.tool("store","--label=fixture","service","synthetic",input=b"synthetic-secret")
        self.assertEqual(result.returncode,0,result.stderr.decode())
        result = self.tool("lookup","service","synthetic")
        self.assertEqual(result.returncode,0)
        self.assertEqual(result.stdout,b"synthetic-secret")
        result = self.tool("clear","service","synthetic")
        self.assertEqual(result.returncode,0)
        self.assertEqual(self.tool("lookup","service","synthetic").stdout,b"")

    def test_secretstorage_dh_roundtrip_replace_properties_and_delete(self):
        collection = self.collection()
        item = collection.create_item("first",{"service":"synthetic","account":"test"},b"synthetic-secret")
        self.assertEqual(item.get_secret(),b"synthetic-secret")
        self.assertEqual(item.get_label(),"first")
        self.assertEqual(item.get_attributes(),{"service":"synthetic","account":"test"})
        replaced = collection.create_item("updated",item.get_attributes(),b"replacement",replace=True)
        self.assertEqual(replaced.item_path,item.item_path)
        self.assertEqual(item.get_secret(),b"replacement")
        item.set_label("renamed")
        item.set_attributes({"service":"other"})
        self.assertEqual(item.get_label(),"renamed")
        self.assertEqual(len(list(collection.search_items({"service":"other"}))),1)
        item.set_secret(b"after-update")
        self.assertEqual(item.get_secret(),b"after-update")
        item.delete()
        self.assertEqual(len(list(collection.get_all_items())),0)

    def test_restart_locked_search_prompt_unlock_alias_and_volatile(self):
        collection = self.collection()
        item = collection.create_item("persistent",{"service":"persist"},b"saved")
        original_path = item.item_path
        self.stop()
        self.start()
        collection = secretstorage.get_default_collection(self.connection)
        self.assertTrue(collection.is_locked())
        found = list(secretstorage.search_items(self.connection,{"service":"persist"}))
        self.assertEqual(len(found),1)
        self.assertEqual(found[0].item_path,original_path)
        self.assertTrue(found[0].is_locked())
        self.assertFalse(collection.unlock())
        self.assertEqual(found[0].get_secret(),b"saved")
        collection.lock()
        self.assertTrue(collection.is_locked())
        self.assertFalse(found[0].unlock())
        self.assertEqual(found[0].get_secret(),b"saved")

    def test_competing_owner_and_single_writer_refuse(self):
        candidate = subprocess.run([str(DAEMON),"--storage-root",str(self.root/"other"),
            "--private-bus",self.env["DBUS_SESSION_BUS_ADDRESS"],"--prompt-program",str(HELPER)],
            capture_output=True,env=self.env,timeout=5)
        self.assertEqual(candidate.returncode,3)
        self.assertEqual(candidate.stdout,b"")
        collection = self.collection()
        self.assertEqual(collection.get_label(),"Synthetic login")

if __name__ == "__main__":
    unittest.main(verbosity=2)
