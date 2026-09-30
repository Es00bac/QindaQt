#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic public-store imports before any daemon writer or frontend starts."""
import pathlib
import subprocess
import sys
import unittest
SEED=pathlib.Path(sys.argv.pop(6)).resolve()
import test_native_portal as portal
class ImportWireTest(unittest.TestCase):
    def start(self):
        if not getattr(self,"seeded",False):
            result=subprocess.run([str(SEED),str(self.storage)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=False)
            self.assertEqual(result.returncode,0);self.seeded=True
        portal.PortalTest.start(self)
    def collection(self):
        collection=portal.fixture.secretstorage.get_default_collection(self.connection)
        if collection.is_locked(): self.assertFalse(collection.unlock(timeout=5))
        return collection
    def test_exact_opaque64_restart_and_fresh32_without_truncation(self):
        peer,frontend=self.prepare();result,value=self.retrieve(frontend,"org.example.Legacy");self.assertEqual(result,0);self.assertEqual(len(value),64)
        self.assertTrue(all(value[index]==((index*17)&255) for index in range(64)))
        result,same=self.retrieve(frontend,"org.example.Legacy",2);self.assertEqual(result,0);self.assertTrue(value==same)
        collection=self.collection();collection.lock();self.assertTrue(collection.is_locked())
        result,unlocked=self.retrieve(frontend,"org.example.Legacy",20);self.assertEqual(result,0);self.assertTrue(value==unlocked)
        result,fresh=self.retrieve(frontend,"org.example.New",3);self.assertEqual(result,0);self.assertEqual(len(fresh),32)
        self.stop();self.start();self.assertTrue(self.native("AttachSessionWithDisplay","s","qindaqt-7")[0]);self.until(lambda:self.state()["ScreenLockAvailable"])
        result,restarted=self.retrieve(frontend,"org.example.Legacy",4);self.assertEqual(result,0);self.assertTrue(value==restarted)
    def test_forged64_metadata_and_native_lock_are_failures(self):
        peer,frontend=self.prepare();result,value=self.retrieve(frontend,"org.example.Forged");self.assertEqual(result,2);self.assertFalse(value)
        self.command(peer,"lock");self.until(lambda:self.state()["ScreenLocked"]);result,value=self.retrieve(frontend,"org.example.Legacy",2);self.assertEqual(result,2);self.assertFalse(value)
    def test_installed_frontend_delivers_exact_legacy_size(self):
        self.frontendApps=["org.example.Legacy","org.example.Legacy","org.example.New",None]
        self.frontendLengths={"org.example.Legacy":64}
        portal.PortalTest.test_installed_frontend_registered_host_and_empty_id(self)
for name in ["setUp","cleanup","stop","call","native","control","state","until","compositor","command","unlock","backend","frontend","prepare","retrieve"]:
    setattr(ImportWireTest,name,getattr(portal.PortalTest,name))
if __name__=="__main__": unittest.main()
