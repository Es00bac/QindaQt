#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fixed interpreter regression; two finite tiny Python children, no Wine."""
import os,stat,subprocess,sys,time,unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
from prefix_domain import supervisor_interpreter
from owned_child import OwnedChild
import inside

class InterpreterControls(unittest.TestCase):
    def test_outer_command_pins_exact_interpreter(self):
        import outer
        args=outer.command(Path("/fixture"),"preflight")
        index=args.index("FIXTURE_PYTHON")
        self.assertEqual(args[index-1:index+2],["--setenv","FIXTURE_PYTHON",supervisor_interpreter()])
        self.assertIn(supervisor_interpreter(),outer.REQUIRED)
    def test_actual_packaged_interpreter(self):
        self.assertEqual(supervisor_interpreter(),str(Path("/proc/self/exe").resolve(strict=True)))
    def test_dispatcher_path_refused(self):
        with patch("prefix_domain.Path.resolve",return_value=Path("/usr/bin/python-exec2c")):
            with self.assertRaisesRegex(RuntimeError,"provenance mismatch"):supervisor_interpreter()
    def test_foreign_version_refused(self):
        with patch("prefix_domain.Path.resolve",return_value=Path("/usr/bin/python9.99")),patch("prefix_domain.Path.stat",return_value=SimpleNamespace(st_mode=stat.S_IFREG|0o755,st_uid=0)):
            with self.assertRaisesRegex(RuntimeError,"provenance mismatch"):supervisor_interpreter()
    def test_nonroot_or_mutable_interpreter_refused(self):
        actual=Path(supervisor_interpreter())
        for mode,uid in [(stat.S_IFREG|0o755,1000),(stat.S_IFREG|0o775,0),(stat.S_IFREG|0o757,0),(stat.S_IFDIR|0o755,0)]:
            with self.subTest(mode=mode,uid=uid),patch("prefix_domain.Path.resolve",return_value=actual),patch("prefix_domain.Path.stat",return_value=SimpleNamespace(st_mode=mode,st_uid=uid)):
                with self.assertRaisesRegex(RuntimeError,"provenance mismatch"):supervisor_interpreter()
    def test_preflight_rejects_interpreter_inventory_change_first(self):
        with patch.dict(os.environ,{"FIXTURE_PYTHON":"/usr/bin/python9.99"}),patch("inside.supervisor_interpreter",return_value="/usr/bin/python3.14"),patch("inside.os.stat") as probe:
            with self.assertRaisesRegex(RuntimeError,"differs from inventoried"):inside.preflight()
            # Path resolution/stat uses the same os module; no namespace is read.
            self.assertFalse(any(str(call.args[0]).startswith("/proc/self/ns/") for call in probe.call_args_list))
    def test_dispatcher_really_changes_executable(self):
        expected=str(Path("/usr/bin/python3").resolve(strict=True))
        if expected==supervisor_interpreter():self.skipTest("host has no interpreter dispatcher")
        # communicate retains the initiating Popen and naturally reaps this
        # bounded read-only child. Timeout cannot produce a passing test.
        child=subprocess.Popen(["/usr/bin/python3","-c","import os; print(os.readlink('/proc/self/exe'))"],stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        fd=os.pidfd_open(child.pid)
        try:
            out,err=child.communicate(timeout=2)
            self.assertEqual(child.returncode,0,err)
            self.assertEqual(out.decode().strip(),supervisor_interpreter())
            self.assertNotEqual(out.decode().strip(),expected)
        finally:
            if child.poll() is None:
                import signal
                signal.pidfd_send_signal(fd,signal.SIGKILL);child.wait(timeout=2)
            os.close(fd)
    def test_direct_interpreter_keeps_owned_identity(self):
        # Finite child self-expires without parent input. No patched identity
        # function can manufacture the actual OwnedChild success here.
        with open(os.devnull,"wb") as log:
            child=OwnedChild([supervisor_interpreter(),"-c","import time; time.sleep(.2)"],{},log)
            try:
                time.sleep(.03)
                self.assertEqual(child.check_identity()["starttime"],child.initial["starttime"])
                self.assertEqual(child.process.wait(timeout=2),0);child.reaped=True
                self.assertTrue(child.dead())
            finally:
                if child.process.poll() is None:child.contain()
                child.close()
    def test_executable_mismatch_remains_rejected(self):
        with open(os.devnull,"wb") as log:
            child=OwnedChild([supervisor_interpreter(),"-c","import time; time.sleep(.2)"],{},log)
            try:
                child.executable="/different/executable"
                with self.assertRaisesRegex(RuntimeError,"child identity mismatch"):child.check_identity()
                self.assertTrue(child.failed)
                self.assertEqual(child.process.wait(timeout=2),0);child.reaped=True
            finally:
                if child.process.poll() is None:child.contain()
                child.close()
if __name__=="__main__":unittest.main()
