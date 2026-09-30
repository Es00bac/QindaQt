#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Native metadata/reveal/rekey private real-client contracts."""
import pathlib
import sys
import time
import unittest
import test_secret_service as protocol
import secretstorage
from secretstorage.util import exec_prompt
from jeepney import DBusErrorResponse

class NativeTest(unittest.TestCase):
    setUp=protocol.SecretServiceTest.setUp
    cleanup=protocol.SecretServiceTest.cleanup
    stop=protocol.SecretServiceTest.stop
    call=protocol.SecretServiceTest.call
    native=protocol.SecretServiceTest.native
    control=protocol.SecretServiceTest.control
    def start(self):
        previous=protocol.HELPER
        protocol.HELPER=pathlib.Path(__file__).with_name("native_scripted_prompt.py")
        try: protocol.SecretServiceTest.start(self)
        finally: protocol.HELPER=previous
    def collection(self,label="Synthetic login"):
        return secretstorage.create_collection(self.connection,label,"default")
    def session(self):
        return self.call("org.freedesktop.Secret.Service","/org/freedesktop/secrets",
                         "OpenSession","sv","plain",("s",""))[1]
    def reveal(self,item,session=None):
        path=self.native("ReadSecretWithPrompt","oo",item.item_path,session or self.session())[0]
        return exec_prompt(self.connection,path,timeout=5)
    def rekey(self,collection):
        path=self.native("ChangePasswordWithPrompt","o",collection.collection_path)[0]
        return exec_prompt(self.connection,path,timeout=5)
    def test_metadata_is_authenticated_only_after_unlock_and_creator_is_captured(self):
        collection=self.collection()
        item=collection.create_item("Synthetic item",{"app":"caller-spoof"},b"synthetic-value")
        values=self.native("ListItems","o",collection.collection_path)[0]
        row={key:value[1] for key,value in values[0].items()}
        self.assertEqual(row["Label"],"Synthetic item")
        self.assertNotEqual(row["CreatedBy"],"caller-spoof")
        self.assertTrue(row["IndexAuthenticated"]);self.assertGreater(row["Created"],0)
        self.assertEqual(self.control(3),b"QKR1\0")
        values=self.native("ListItems","o",collection.collection_path)[0]
        row={key:value[1] for key,value in values[0].items()}
        self.assertTrue(row["Locked"])
        self.assertNotIn("Label",row);self.assertNotIn("CreatedBy",row);self.assertNotIn("Created",row)
        self.stop();self.start()
        values=self.native("ListItems","o",collection.collection_path)[0]
        row={key:value[1] for key,value in values[0].items()}
        self.assertFalse(row["IndexAuthenticated"])
    def test_reveal_returns_owner_session_secret_after_actual_authentication(self):
        collection=self.collection();item=collection.create_item("fixture",{},b"synthetic-value")
        session=self.session();dismissed,(signature,wire)=self.reveal(item,session)
        self.assertFalse(dismissed);self.assertEqual(signature,"(oayays)")
        self.assertEqual(wire[0],session);self.assertEqual(wire[2],b"synthetic-value")
    def test_already_unlocked_does_not_bypass_reveal_authentication(self):
        collection=self.collection("wrong-auth-fixture")
        item=collection.create_item("fixture",{},b"synthetic-value")
        self.assertFalse(collection.is_locked())
        dismissed,(_,wire)=self.reveal(item)
        self.assertTrue(dismissed);self.assertEqual(wire[2],b"");self.assertTrue(collection.is_locked())
    def test_cross_caller_session_and_prompt_denied(self):
        collection=self.collection();item=collection.create_item("fixture",{},b"synthetic-value")
        session=self.session();other=secretstorage.dbus_init()
        try:
            with self.assertRaises(DBusErrorResponse):
                self.call("org.qindaqt.Keyring1","/org/freedesktop/secrets","ReadSecretWithPrompt",
                          "oo",item.item_path,session,connection=other)
            prompt=self.native("ReadSecretWithPrompt","oo",item.item_path,session)[0]
            with self.assertRaises(DBusErrorResponse):
                self.call("org.freedesktop.Secret.Prompt",prompt,"Prompt","s","",connection=other)
            self.call("org.freedesktop.Secret.Prompt",prompt,"Dismiss")
        finally: other.close()
    def test_native_rekey_is_durable_and_requires_old_password(self):
        collection=self.collection();item=collection.create_item("fixture",{},b"synthetic-value")
        dismissed,(signature,result)=self.rekey(collection)
        self.assertFalse(dismissed);self.assertEqual(signature,"ao");self.assertEqual(len(result),1)
        self.stop();self.start()
        self.assertEqual(self.control(1,old=b"synthetic-keyring-password"),b"QKR1\1")
        time.sleep(.6)
        self.assertEqual(self.control(1,old=b"synthetic-new-password"),b"QKR1\0")
        self.assertEqual(secretstorage.Item(self.connection,item.item_path).get_secret(),b"synthetic-value")
    def test_wrong_old_password_leaves_locked_original(self):
        collection=self.collection("wrong-auth-fixture")
        item=collection.create_item("fixture",{},b"synthetic-value")
        before=next(self.storage.glob("*.qkr")).read_bytes()
        self.assertTrue(self.rekey(collection)[0]);self.assertTrue(collection.is_locked())
        self.assertEqual(next(self.storage.glob("*.qkr")).read_bytes(),before)
    def test_hostile_pair_frame_is_rejected_without_persistence(self):
        collection=self.collection("malformed-frame-fixture")
        before=next(self.storage.glob("*.qkr")).read_bytes()
        self.assertTrue(self.rekey(collection)[0])
        self.assertEqual(next(self.storage.glob("*.qkr")).read_bytes(),before)
    def test_failed_durable_save_cannot_complete_as_success(self):
        collection=self.collection();before=next(self.storage.glob("*.qkr")).read_bytes()
        self.storage.chmod(0o500)
        try:
            self.assertTrue(self.rekey(collection)[0])
            self.assertTrue(collection.is_locked())
            self.assertEqual(next(self.storage.glob("*.qkr")).read_bytes(),before)
        finally: self.storage.chmod(0o700)
    def test_native_delete_confirms_then_acknowledges_actual_save(self):
        collection=self.collection();item=collection.create_item("fixture",{},b"synthetic-value")
        prompt=self.native("DeleteItemWithPrompt","o",item.item_path)[0]
        self.assertEqual(len(list(collection.get_all_items())),1)
        self.assertFalse(exec_prompt(self.connection,prompt,timeout=5)[0])
        self.assertEqual(list(collection.get_all_items()),[])
        self.stop();self.start()
        self.assertEqual(list(secretstorage.Collection(self.connection,collection.collection_path).get_all_items()),[])
    def test_native_delete_save_failure_is_dismissed_and_original_preserved(self):
        collection=self.collection();item=collection.create_item("fixture",{},b"synthetic-value")
        before=next(self.storage.glob("*.qkr")).read_bytes()
        prompt=self.native("DeleteItemWithPrompt","o",item.item_path)[0]
        self.storage.chmod(0o500)
        try:
            self.assertTrue(exec_prompt(self.connection,prompt,timeout=5)[0])
            self.assertEqual(next(self.storage.glob("*.qkr")).read_bytes(),before)
            self.assertTrue(collection.is_locked())
        finally: self.storage.chmod(0o700)
    def test_session_closed_before_prompt_cannot_disclose_secret(self):
        collection=self.collection();item=collection.create_item("fixture",{},b"synthetic-value")
        session=self.session();prompt=self.native("ReadSecretWithPrompt","oo",item.item_path,session)[0]
        self.call("org.freedesktop.Secret.Session",session,"Close")
        dismissed,(_,wire)=exec_prompt(self.connection,prompt,timeout=5)
        self.assertTrue(dismissed);self.assertEqual(wire[2],b"")

if __name__=="__main__": unittest.main()
