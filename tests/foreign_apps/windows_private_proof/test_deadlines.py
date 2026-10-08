#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual server/final-driver deadline controls; no processes or namespaces."""
import json,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from owned_server import OwnedServer
from test_owned_server import FakeChild,FakeFiles
from test_retirement_admission import positive
from driver import qualify_driver,publish_driver_result

class Deadlines(unittest.TestCase):
    def setUp(self):self.now=99
    def clock(self):return self.now
    def server(self):
        return OwnedServer('/synthetic/prefix','/synthetic/server',{},None,100,
            child_type=FakeChild,files_type=FakeFiles)
    def test_reviewed_past_deadline_retirement_rejected(self):
        with patch('owned_server.time.monotonic',self.clock):
            s=self.server();s.start();self.now=101
            with self.assertRaises(RuntimeError):s.retire()
            self.assertTrue(s.failed);self.assertTrue(s.contain()['contained']);s.close()
    def test_cross_during_retirement_rejected(self):
        with patch('owned_server.time.monotonic',self.clock):
            s=self.server();s.start();original=s.child.retire
            def late(seconds):result=original(seconds);self.now=101;return result
            with patch.object(s.child,'retire',late),patch.object(s.files,'released') as released:
                with self.assertRaises(RuntimeError):s.retire()
                released.assert_not_called()
            self.assertTrue(s.failed);s.close()
    def test_cross_during_release_query_rejected(self):
        with patch('owned_server.time.monotonic',self.clock):
            s=self.server();s.start();original=s.files.released
            def late():result=original();self.now=101;return result
            with patch.object(s.files,'released',late):
                with self.assertRaises(RuntimeError):s.retire()
            self.assertTrue(s.failed);s.close()
    def test_cross_during_readiness_rejected(self):
        with patch('owned_server.time.monotonic',self.clock):
            s=self.server();original=s.files.listening
            def late(pid):result=original(pid);self.now=101;return result
            with patch.object(s.files,'listening',late):
                with self.assertRaises(RuntimeError):s.start()
            self.assertTrue(s.failed);s.close()
    def test_cross_during_current_guard_rejected(self):
        with patch('owned_server.time.monotonic',self.clock):
            s=self.server();s.start();original=s.files.listening
            def late(pid):result=original(pid);self.now=101;return result
            with patch.object(s.files,'listening',late):
                with self.assertRaises(RuntimeError):s.guard()
            self.now=99
            with self.assertRaises(RuntimeError):s.guard()
            s.close()
    def test_final_driver_past_deadline_rejected(self):
        with patch('driver.time.monotonic',self.clock):
            self.now=101;v=positive()
            with self.assertRaises(RuntimeError):qualify_driver(v,100)
            self.assertTrue(v['deadlineExpired']);self.assertFalse(v['deadlineQualified'])
    def test_final_driver_cross_during_admission_rejected(self):
        with patch('driver.time.monotonic',self.clock):
            v=positive()
            def late(value):self.now=101;return True
            with patch('processes.admit_driver',late):
                with self.assertRaises(RuntimeError):qualify_driver(v,100)
            self.assertFalse(v['deadlineQualified'])
    def test_final_write_crossing_deadline_not_published_as_success(self):
        with tempfile.TemporaryDirectory() as root,patch('driver.time.monotonic',self.clock):
            path=Path(root)/'receipt.json';v=positive();original=Path.write_text
            def late(target,*args,**kwargs):
                result=original(target,*args,**kwargs);self.now=101;return result
            with patch.object(Path,'write_text',late):code=publish_driver_result(v,100,path)
            self.assertEqual(code,1)
            self.assertFalse(json.loads(path.read_text())['deadlineQualified'])
    def test_actual_final_driver_success_before_deadline(self):
        with tempfile.TemporaryDirectory() as root,patch('driver.time.monotonic',self.clock):
            path=Path(root)/'receipt.json';v=positive()
            self.assertEqual(publish_driver_result(v,100,path),0)
            self.assertTrue(json.loads(path.read_text())['deadlineQualified'])
if __name__=='__main__':unittest.main(verbosity=2)
