#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual old/new driver functions; pure env/path controls, no native entry."""
import argparse,importlib.util,os,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch

parser=argparse.ArgumentParser()
parser.add_argument("--driver-source",type=Path,default=Path(__file__).with_name("driver.py"))
parser.add_argument("--metadata",action="store_true")
args=parser.parse_args()
sys.path.insert(0,str(args.driver_source.resolve().parent))
spec=importlib.util.spec_from_file_location("authority_driver_control",args.driver_source)
api=importlib.util.module_from_spec(spec);spec.loader.exec_module(api)

class EnvironmentControls(unittest.TestCase):
    def call(self,value=None):
        env={"PATH":"/usr/bin","DISPLAY":":fixture","XDG_RUNTIME_DIR":"/fixture/runtime"}
        if value is not None:env["XAUTHORITY"]=value
        # Exact inherited native cwd is /fixture, not a /tmp test working root.
        with patch.dict(os.environ,env,clear=True),patch("os.getcwd",return_value="/fixture"):
            return api.wine_environment("app-a")
    def test_absent_omitted(self):
        self.assertNotIn("XAUTHORITY",self.call())
    def test_exact_empty_omitted(self):
        try:result=self.call("")
        except RuntimeError:self.fail("exact empty XAUTHORITY must be omitted")
        self.assertNotIn("XAUTHORITY",result)
    def test_fixture_auth_preserved(self):
        self.assertEqual(self.call("/fixture/runtime/xauth")["XAUTHORITY"],"/fixture/runtime/xauth")
    def test_private_tmp_auth_preserved(self):
        self.assertEqual(self.call("/tmp/fixture-auth")["XAUTHORITY"],"/tmp/fixture-auth")
    def test_external_refused(self):
        with self.assertRaises(RuntimeError):self.call("/outside/auth")
    def test_whitespace_refused(self):
        with self.assertRaises(RuntimeError):self.call(" 	")
    def test_exact_fixture_root_refused(self):
        with self.assertRaises(RuntimeError):self.call("/fixture")
    def test_traversal_refused(self):
        with self.assertRaises(RuntimeError):self.call("/fixture/../outside/auth")
    def test_symlink_escape_refused(self):
        with tempfile.TemporaryDirectory(prefix="windows-auth-pure-") as tmp:
            f=Path(tmp)/"auth";f.symlink_to("/outside/auth")
            with self.assertRaises(RuntimeError):self.call(str(f))
    def test_literal_spaces_not_trimmed(self):
        self.assertEqual(self.call("/fixture/auth file")["XAUTHORITY"],"/fixture/auth file")

class MetadataControls(unittest.TestCase):
    def state(self,env):
        with patch.dict(os.environ,env,clear=True),patch("os.getcwd",return_value="/fixture"):
            return api.authority_state()
    def test_absent_original_state(self):
        self.assertEqual(self.state({}),{"present":False,"exactEmpty":False,"whitespaceOnly":False,"cwd":"/fixture"})
    def test_empty_original_state(self):
        self.assertEqual(self.state({"XAUTHORITY":""}),{"present":True,"exactEmpty":True,"whitespaceOnly":False,"cwd":"/fixture"})
    def test_nonempty_not_logged(self):
        v=self.state({"XAUTHORITY":"/fixture/runtime/metadata-only"})
        self.assertEqual(set(v),{"present","exactEmpty","whitespaceOnly","cwd"})
        self.assertTrue(v["present"]);self.assertFalse(v["exactEmpty"])
        self.assertNotIn("metadata-only",str(v))

suite=unittest.defaultTestLoader.loadTestsFromTestCase(EnvironmentControls)
if args.metadata:suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(MetadataControls))
result=unittest.TextTestRunner(verbosity=2).run(suite)
raise SystemExit(0 if result.wasSuccessful() else 1)
