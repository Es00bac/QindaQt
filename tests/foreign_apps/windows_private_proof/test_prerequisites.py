#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Pure tests of actual fixture functions; never invokes namespace/app entry."""
import json,os,tempfile,unittest
from pathlib import Path
import inside,outer

class PublicRuntimeTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix="windows-public-data-")
        self.root=Path(self.tmp.name)
        (self.root/"usr/bin").mkdir(parents=True)
        (self.root/"usr/bin/bash").write_text("fixture only")
        (self.root/"usr/bin/sh").symlink_to("bash")
        (self.root/"bin").symlink_to("usr/bin")
        self.required=["usr/bin/xkbcomp","usr/share/xkeyboard-config-2/rules/evdev",
            "usr/share/xkeyboard-config-2/symbols/us","usr/share/xkeyboard-config-2/keycodes/evdev",
            "usr/share/xkeyboard-config-2/types/complete","etc/fonts/fonts.conf"]
        for n in self.required:
            f=self.root/n;f.parent.mkdir(parents=True,exist_ok=True);f.write_text("public fixture")
    def tearDown(self):self.tmp.cleanup()
    def test_complete(self):
        self.assertEqual(inside.validate_public_runtime(self.root),
            {"mergedUsrShell":True,"xkbCompilerAndData":True,"fontConfig":True})
    def test_missing_alias(self):
        (self.root/"bin").unlink()
        with self.assertRaises(RuntimeError):inside.validate_public_runtime(self.root)
    def test_wrong_alias(self):
        (self.root/"bin").unlink();(self.root/"bin").symlink_to("etc")
        with self.assertRaises(RuntimeError):inside.validate_public_runtime(self.root)
    def test_missing_shell(self):
        (self.root/"usr/bin/bash").unlink()
        with self.assertRaises(FileNotFoundError):inside.validate_public_runtime(self.root)
    def test_foreign_shell_target(self):
        (self.root/"usr/bin/other").write_text("other")
        (self.root/"usr/bin/sh").unlink();(self.root/"usr/bin/sh").symlink_to("other")
        with self.assertRaises(RuntimeError):inside.validate_public_runtime(self.root)
    def test_each_missing_public_input(self):
        for n in self.required:
            with self.subTest(name=n):
                f=self.root/n;f.unlink()
                with self.assertRaises(RuntimeError):inside.validate_public_runtime(self.root)
                f.write_text("public fixture")
    def test_empty_public_input(self):
        (self.root/self.required[0]).write_text("")
        with self.assertRaises(RuntimeError):inside.validate_public_runtime(self.root)
    def test_command_synthetic_alias(self):
        args=outer.command(self.root,"preflight")
        i=args.index("--symlink");self.assertEqual(args[i:i+3],["--symlink","usr/bin","/bin"])
    def test_command_no_forbidden_host_bind(self):
        args=outer.command(self.root,"execute")
        mounts=[args[i+1] for i,v in enumerate(args) if v in {"--bind","--ro-bind"}]
        self.assertNotIn("/etc",mounts);self.assertNotIn("/tmp",mounts)
        self.assertNotIn("/run/user",mounts);self.assertNotIn("/home",mounts)
        self.assertIn("--unshare-all",args);self.assertIn("--clearenv",args)
    def test_manifest_actual_members(self):
        hashes=outer.source_identity()
        self.assertEqual(len(hashes),18)
    def test_nonexecutable_staged_driver_refuses_before_launch(self):
        import shutil
        from unittest.mock import patch
        staged=self.root/"reviewed-source";staged.mkdir()
        for name in [*outer.source_identity(),"source-sha256.json"]:
            shutil.copy2(outer.HERE/name,staged/name)
        (staged/"driver.py").chmod(0o644)
        with patch.object(outer,"HERE",staged):
            with self.assertRaisesRegex(RuntimeError,"executable mode absent"):outer.source_identity()
    def test_public_payloads_bound(self):
        self.assertEqual(len(outer.REQUIRED),21)
        self.assertIn(outer.supervisor_interpreter(),outer.REQUIRED)
        self.assertIn("/usr/lib64/libXtst.so.6",outer.REQUIRED)
        self.assertIn("/usr/bin/sh",outer.REQUIRED)
        self.assertIn("/etc/fonts/fonts.conf",outer.REQUIRED)
if __name__=="__main__":unittest.main()
