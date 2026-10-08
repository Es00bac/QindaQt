#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Subreaper tests run only synthetic Python children with bounded natural exit."""
import json,os,subprocess,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
import child_ledger

KNOWN=r"""
import time,json,os,sys
from pathlib import Path
from child_ledger import ChildLedger
from owned_child import OwnedChild
root=Path(sys.argv[1]);ledger=ChildLedger()
program='import os,time,sys;from pathlib import Path;p=os.fork();\nif p==0:\n Path(sys.argv[1]+"/grand").write_text(str(os.getpid()));\n while not Path(sys.argv[1]+"/release").exists():time.sleep(.01)\n time.sleep(.15);os._exit(0)\nelse:\n while not Path(sys.argv[1]+"/release").exists():time.sleep(.01)\n os._exit(0)'
with (root/'child.log').open('wb') as log:
 child=OwnedChild([sys.executable,'-c',program,str(root)],{},log);ledger.register(child,'synthetic owner')
 end=time.monotonic()+2
 while not (root/'grand').exists() and time.monotonic()<end:time.sleep(.01)
 ledger.checkpoint()
 assert len(ledger.observed)==1
 (root/'release').touch()
 assert child.process.wait(timeout=2)==0;child.reaped=True
 time.sleep(.25)
 result=ledger.final();assert result['qualified'] and len(result['adoptedReaped'])==1
 child.close();ledger.close();print(json.dumps(result))
"""
UNKNOWN=r"""
import os,time,json,sys
from pathlib import Path
from child_ledger import ChildLedger,children
from owned_child import OwnedChild
root=Path(sys.argv[1]);ledger=ChildLedger()
program='import os,time,sys;from pathlib import Path;root=Path(sys.argv[1])\nwhile not (root/"release").exists():time.sleep(.01)\np=os.fork()\nif p==0:\n os.setsid();q=os.fork()\n if q==0:time.sleep(.3)\n os._exit(0)\nelse:os._exit(0)'
with (root/'child.log').open('wb') as log:
 parent=OwnedChild([sys.executable,'-c',program,str(root)],{},log);ledger.register(parent,'synthetic owner')
 (root/'release').touch()
 assert parent.process.wait(timeout=2)==0;parent.reaped=True
 refused=False
 try:ledger.checkpoint()
 except RuntimeError as error:refused='unknown adopted child' in str(error)
 assert refused and ledger.failed
 # Unknown child is never signaled. It exits on its own bounded timer and is
 # reaped as our actual child, leaving no process behind on the host test.
 end=time.monotonic()+2
 while children(os.getpid()) and time.monotonic()<end:
  for pid in children(os.getpid()):os.waitpid(pid,os.WNOHANG)
  time.sleep(.01)
 assert not children(os.getpid())
 parent.close();ledger.close();print(json.dumps({'unknownRefused':True,'noSignal':True,'noChildren':True}))
"""
class LedgerControls(unittest.TestCase):
    def run_owned(self,program):
        with tempfile.TemporaryDirectory(prefix='windows-orphan-control-') as root:
            env={'PYTHONPATH':str(Path(__file__).resolve().parent),'PATH':'/usr/bin'}
            p=subprocess.Popen([sys.executable,'-c',program,root],env=env,
                stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE,close_fds=True)
            out,err=p.communicate(timeout=6)
            self.assertEqual(p.returncode,0,(out,err));return json.loads(out)
    def test_known_orphan_actual_reaped(self):
        self.assertTrue(self.run_owned(KNOWN)['qualified'])
    def test_unknown_doublefork_refused_without_signal(self):
        # A peer outside the isolated synthetic supervisor remains alive.
        peer=subprocess.Popen([sys.executable,'-c','import time;time.sleep(2)'],
            stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        try:
            self.assertTrue(self.run_owned(UNKNOWN)['unknownRefused'])
            self.assertIsNone(peer.poll())
        finally:
            # Exact own unreaped Popen/pidfd only, never a discovered peer PID.
            fd=os.pidfd_open(peer.pid)
            import signal
            signal.pidfd_send_signal(fd,signal.SIGKILL);peer.wait(timeout=2);os.close(fd)
    def test_subreaper_failure_refused(self):
        fake=type('L',(),{})()
        fake.prctl=type('F',(),{'__call__':lambda self,*a:-1})()
        with patch('child_ledger.ctypes.CDLL',return_value=fake):
            with self.assertRaises(RuntimeError):child_ledger.subreaper()
if __name__=='__main__':unittest.main(verbosity=2)
