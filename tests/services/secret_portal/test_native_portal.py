#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real keyring, native ordinary compositor, Settings1 and backend on a private bus.
Only generated temporary collections/synthetic credentials are used. Secret
assertions compare booleans/lengths, never interpolate secret bytes into logs.
"""
import pathlib
import os
import select
import socket
import subprocess
import sys
import time
import unittest
DAEMON,SETTINGS,COMPOSITOR,BACKEND,ROOT=map(lambda value:pathlib.Path(value).resolve(),sys.argv[1:6])
sys.path.insert(0,str(ROOT/"tests/services/keyring"))
sys.argv=[sys.argv[0],str(DAEMON),str(SETTINGS),str(COMPOSITOR),str(ROOT/"data/settings")]+sys.argv[6:]
import test_resident_policy as fixture
from jeepney import DBusAddress,new_method_call,DBusErrorResponse,MatchRule,MessageType
from jeepney.fds import FileDescriptor
from jeepney.io.blocking import open_dbus_connection
from secretstorage.util import exec_prompt
BACKEND_NAME="org.freedesktop.impl.portal.desktop.qindaqt"
FRONTEND_NAME="org.freedesktop.portal.Desktop"
INTERFACE="org.freedesktop.impl.portal.Secret"
class PortalTest(unittest.TestCase):
    def backend(self):
        self.env["QINDAQT_PORTAL_THEME_DIRS"]=str(ROOT/"data/themes")
        self.env.pop("QT_LOGGING_RULES",None)
        child=subprocess.Popen([str(BACKEND)],env=self.env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        def stop():
            if child.poll() is None: child.terminate()
            child.wait(timeout=5)
        self.addCleanup(stop)
        self.until(lambda:self.connection.send_and_get_reply(new_method_call(DBusAddress('/org/freedesktop/DBus','org.freedesktop.DBus','org.freedesktop.DBus'),'NameHasOwner','s',(BACKEND_NAME,))).body[0])
        return child
    def frontend(self):
        connection=open_dbus_connection(self.env["DBUS_SESSION_BUS_ADDRESS"],enable_fds=True)
        self.addCleanup(connection.close)
        response=connection.send_and_get_reply(new_method_call(DBusAddress('/org/freedesktop/DBus','org.freedesktop.DBus','org.freedesktop.DBus'),'RequestName','su',(FRONTEND_NAME,0)))
        self.assertEqual(response.body[0],1)
        return connection
    def retrieve(self,connection,app,number=1):
        read,write=socket.socketpair();read.settimeout(5)
        try:
            with FileDescriptor(os.dup(write.fileno())) as fd:
                method=new_method_call(DBusAddress('/org/freedesktop/portal/desktop',BACKEND_NAME,INTERFACE),'RetrieveSecret','osha{sv}',
                    ('/org/freedesktop/portal/desktop/request/1_2/p'+str(number),app,fd,{}))
                response=connection.send_and_get_reply(method,timeout=10)
            write.close()
            value=b''
            while True:
                block=read.recv(128)
                if not block: break
                value+=block
            return response.body[0],value
        finally:
            read.close();write.close()
    def prepare(self):
        self.until(lambda:self.state()["SettingsAvailable"]);peer=self.compositor()
        self.until(lambda:self.state()["ScreenLockAvailable"] and not self.state()["ScreenLocked"])
        self.backend();return peer,self.frontend()
    def test_distinct_and_same_apps_survive_restart_and_alias_change(self):
        peer,frontend=self.prepare();collection=self.collection()
        first,value=self.retrieve(frontend,"org.example.App");self.assertEqual(first,0);self.assertEqual(len(value),32)
        result,same=self.retrieve(frontend,"org.example.App",2);self.assertEqual(result,0);self.assertTrue(value==same)
        result,other=self.retrieve(frontend,"org.example.Other",3);self.assertEqual(result,0);self.assertEqual(len(other),32);self.assertFalse(value==other)
        self.call("org.freedesktop.Secret.Service","/org/freedesktop/secrets","SetAlias","so","default",'/')
        result,same=self.retrieve(frontend,"org.example.App",4);self.assertEqual(result,0);self.assertTrue(value==same)
        self.stop();self.start();self.assertTrue(self.native("AttachSessionWithDisplay","s","qindaqt-7")[0]);self.until(lambda:self.state()["ScreenLockAvailable"])
        result,restarted=self.retrieve(frontend,"org.example.App",5);self.assertEqual(result,0);self.assertTrue(value==restarted)
    def test_first_call_creates_login_through_owned_prompt(self):
        peer,frontend=self.prepare();result,value=self.retrieve(frontend,"org.example.App")
        self.assertEqual(result,0);self.assertEqual(len(value),32)
        self.assertIn("login",self.native("ListCollections")[0])
    def test_locked_collection_unlocks_without_item_selector(self):
        peer,frontend=self.prepare();collection=self.collection();result,value=self.retrieve(frontend,"org.example.App");self.assertEqual(result,0)
        collection.lock();self.assertTrue(collection.is_locked())
        result,after=self.retrieve(frontend,"org.example.App",2);self.assertEqual(result,0);self.assertTrue(value==after)
        self.assertFalse(collection.is_locked())
    def test_native_lock_is_failure_even_with_collection_preference_false(self):
        peer,frontend=self.prepare();collection=self.collection();self.command(peer,"lock");self.until(lambda:self.state()["ScreenLocked"])
        result,value=self.retrieve(frontend,"org.example.App");self.assertEqual(result,2);self.assertFalse(value);self.assertFalse(collection.is_locked())
    def test_unrelated_default_and_empty_host_id_fail_explicitly(self):
        peer,frontend=self.prepare()
        unrelated=fixture.secretstorage.create_collection(self.connection,"Other","")
        self.call("org.freedesktop.Secret.Service","/org/freedesktop/secrets","SetAlias","so","default",unrelated.collection_path)
        result,value=self.retrieve(frontend,"org.example.App");self.assertEqual(result,2);self.assertFalse(value)
        result,value=self.retrieve(frontend,"",2);self.assertEqual(result,2);self.assertFalse(value)
    def test_installed_frontend_registered_host_and_empty_id(self):
        self.until(lambda:self.state()["SettingsAvailable"]);peer=self.compositor()
        self.until(lambda:self.state()["ScreenLockAvailable"] and not self.state()["ScreenLocked"])
        self.backend();self.collection()
        portals=self.root/"portals";portals.mkdir()
        (portals/"qindaqt.portal").write_text((ROOT/"src/services/portal/data/qindaqt.portal").read_text())
        (portals/"qindaqt-portals.conf").write_text("[preferred]\ndefault=none\norg.freedesktop.impl.portal.Settings=qindaqt\norg.freedesktop.impl.portal.Secret=qindaqt\n")
        applications=self.root/"data/applications";applications.mkdir(parents=True)
        for app in ["org.example.Registered","org.example.Other"]:
            (applications/(app+".desktop")).write_text("[Desktop Entry]\nType=Application\nName=Synthetic registered host\nExec=/bin/true\n")
        self.env.update(XDG_CURRENT_DESKTOP="qindaqt",XDG_DESKTOP_PORTAL_DIR=str(portals),XDG_DATA_DIRS=str(self.root/"empty-data"),XDG_CONFIG_DIRS=str(self.root/"empty-config"))
        self.env.pop("DISPLAY",None);self.env.pop("WAYLAND_DISPLAY",None)
        # No installed host helpers may activate: this private daemon has no
        # activation directories and only the two staged provider declarations.
        frontend=subprocess.Popen(["/usr/libexec/xdg-desktop-portal"],env=self.env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        def stop():
            if frontend.poll() is None: frontend.terminate()
            frontend.wait(timeout=5)
        self.addCleanup(stop)
        self.until(lambda:self.connection.send_and_get_reply(new_method_call(DBusAddress('/org/freedesktop/DBus','org.freedesktop.DBus','org.freedesktop.DBus'),'NameHasOwner','s',(FRONTEND_NAME,))).body[0])
        values=[]
        for index,app in enumerate(["org.example.Registered","org.example.Registered","org.example.Other",None]):
            actor=open_dbus_connection(self.env["DBUS_SESSION_BUS_ADDRESS"],enable_fds=True);self.addCleanup(actor.close)
            if app:
                registered=actor.send_and_get_reply(new_method_call(DBusAddress('/org/freedesktop/portal/desktop',FRONTEND_NAME,'org.freedesktop.host.portal.Registry'),'Register','sa{sv}',(app,{})))
                self.assertEqual(registered.header.message_type,MessageType.method_return)
            read,write=socket.socketpair();read.settimeout(5)
            try:
                rule=MatchRule(interface="org.freedesktop.portal.Request",member="Response",type=MessageType.signal)
                with actor.filter(rule) as signals:
                    with FileDescriptor(os.dup(write.fileno())) as fd:
                        reply=actor.send_and_get_reply(new_method_call(DBusAddress('/org/freedesktop/portal/desktop',FRONTEND_NAME,'org.freedesktop.portal.Secret'),'RetrieveSecret','ha{sv}',(fd,{"handle_token":('s','pk5_'+str(index))})))
                    self.assertTrue(reply.body[0].startswith('/org/freedesktop/portal/desktop/request/'))
                    response=actor.recv_until_filtered(signals,timeout=10)
                self.assertEqual(response.body[0],0 if app else 2)
                write.close();value=b''
                while True:
                    block=read.recv(128)
                    if not block: break
                    value+=block

                self.assertEqual(len(value),32 if app else 0);values.append(value)
            finally: read.close();write.close()
        self.assertTrue(values[0]==values[1]);self.assertFalse(values[0]==values[2])
    def test_direct_native_broker_from_other_actor_is_denied(self):
        peer,frontend=self.prepare();self.collection()
        with self.assertRaises(DBusErrorResponse): self.native("RequestPortalSecret","ss","org.example.App","12345678-1234-4234-8234-123456789abc")
# AGENT-CONTRACT: these are private harness utilities, not production private
# headers/storage access. They preserve all injected roots and synthetic helpers.
for name in ["setUp","cleanup","start","stop","call","native","control","state","until","compositor","command","collection","unlock"]:
    setattr(PortalTest,name,getattr(fixture.ResidentTest,name))
if __name__=="__main__": unittest.main()
