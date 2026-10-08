#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Every synthetic generation self-expires; supervisor always settles/reaps."""
import json,os,signal,shutil,subprocess,sys,tempfile,time,unittest
from pathlib import Path
from unittest.mock import patch
import child_ledger

KNOWN_CHILD=r"""
import os,time,sys
from pathlib import Path
root=Path(sys.argv[1]);stop=time.monotonic()+1.5
p=os.fork()
if p==0:
 (root/"grand").write_text(str(os.getpid()))
 while not (root/"release").exists() and time.monotonic()<stop:time.sleep(.01)
 time.sleep(.15);os._exit(0)
else:
 while not (root/"release").exists() and time.monotonic()<stop:time.sleep(.01)
 os._exit(0)
"""
UNKNOWN_CHILD=r"""
import os,time,sys
from pathlib import Path
root=Path(sys.argv[1]);stop=time.monotonic()+1.5
while not (root/"release").exists() and time.monotonic()<stop:time.sleep(.01)
if time.monotonic()>=stop:os._exit(0)
p=os.fork()
if p==0:
 os.setsid();q=os.fork()
 if q==0:time.sleep(.3)
 os._exit(0)
else:os._exit(0)
"""
SUPERVISOR=r"""
import time,json,os,sys,select
from pathlib import Path
from child_ledger import ChildLedger,children
from owned_child import OwnedChild
root=Path(sys.argv[1]);mode=sys.argv[2];kind=__KIND__
ledger=None;child=None;result={};failure=None
program=__PROGRAM__
try:
 ledger=ChildLedger()
 with (root/'child.log').open('wb') as log:
  child=OwnedChild([sys.executable,'-c',program,str(root)],{},log)
  ledger.register(child,'synthetic owner')
  if kind=='known':
   end=time.monotonic()+2
   while not (root/'grand').exists() and time.monotonic()<end:time.sleep(.01)
   if mode=='fail-before-release':
    def rejected():raise RuntimeError('injected pre-release checkpoint failure')
    ledger.checkpoint=rejected
   ledger.checkpoint()
   assert len(ledger.observed)==1
   (root/'release').touch()
   assert child.process.wait(timeout=2)==0;child.reaped=True
   end=time.monotonic()+2
   while any(not select.select([v['fd']],[],[],0)[0] for v in ledger.observed.values()) and time.monotonic()<end:time.sleep(.01)
   result=ledger.final();assert result['qualified'] and len(result['adoptedReaped'])==1
  else:
   (root/'release').touch()
   assert child.process.wait(timeout=2)==0;child.reaped=True
   refused=False
   try:ledger.checkpoint()
   except RuntimeError as error:refused='unknown adopted child' in str(error)
   assert refused and ledger.failed
   result={'unknownRefused':True,'noSignal':True}
except Exception as error:failure=type(error).__name__
finally:
 # Direct owner containment only; unknown descendants exit on independent
 # generation deadlines even when the release path is never created.
 if child and not child.reaped:
  try:child.contain()
  except Exception as error:failure=type(error).__name__
 end=time.monotonic()+3
 while children(os.getpid()) and time.monotonic()<end:
  for pid in children(os.getpid()):os.waitpid(pid,os.WNOHANG)
  time.sleep(.01)
 result['noChildren']=not children(os.getpid())
 if ledger:ledger.close()
 if child:child.close()
 result['failure']=failure
 print(json.dumps(result),flush=True)
sys.exit(7 if failure else (0 if result['noChildren'] else 8))
"""
KNOWN=SUPERVISOR.replace('__KIND__',repr('known')).replace('__PROGRAM__',repr(KNOWN_CHILD))
UNKNOWN=SUPERVISOR.replace('__KIND__',repr('unknown')).replace('__PROGRAM__',repr(UNKNOWN_CHILD))

class LedgerControls(unittest.TestCase):
    def run_owned(self,program,mode='normal',expected=0,timeout=6):
        child_ledger.subreaper()
        baseline=set(child_ledger.children(os.getpid()))
        root=tempfile.mkdtemp(prefix='windows-orphan-control-');clean=False
        try:
            env={'PYTHONPATH':str(Path(__file__).resolve().parent),'PATH':'/usr/bin'}
            p=subprocess.Popen([sys.executable,'-c',program,root,mode],env=env,
                stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE,close_fds=True)
            fd=None;out=b'';err=b'';timed=False
            try:
                fd=os.pidfd_open(p.pid)
                try:out,err=p.communicate(timeout=timeout)
                except subprocess.TimeoutExpired:
                    timed=True;signal.pidfd_send_signal(fd,signal.SIGKILL)
                    out,err=p.communicate(timeout=2)
            finally:
                try:
                    if p.poll() is None:
                        if fd is not None:signal.pidfd_send_signal(fd,signal.SIGKILL)
                        p.wait(timeout=8) # independently bounded supervisor/generations
                finally:
                    if fd is not None:os.close(fd)
                    # Settlement failure must not skip naturally bounded
                    # adopted generations; signal only the held supervisor.
                    end=time.monotonic()+4
                    while set(child_ledger.children(os.getpid()))-baseline and time.monotonic()<end:
                        for pid in set(child_ledger.children(os.getpid()))-baseline:
                            os.waitpid(pid,os.WNOHANG)
                        time.sleep(.01)
                    self.assertFalse(set(child_ledger.children(os.getpid()))-baseline)
            self.assertIsNotNone(p.returncode)
            clean=True
            if timed:return {'timed':True,'supervisorReaped':True,'noChildren':True}
            self.assertEqual(p.returncode,expected,(out,err));return json.loads(out)
        finally:
            if clean:shutil.rmtree(root) # unknown/surviving-child uncertainty retains root
    def test_known_orphan_actual_reaped(self):
        self.assertTrue(self.run_owned(KNOWN)['qualified'])
    def test_unknown_doublefork_refused_without_signal(self):
        peer=subprocess.Popen([sys.executable,'-c','import time;time.sleep(6)'],
            stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        fd=os.pidfd_open(peer.pid)
        try:
            self.assertTrue(self.run_owned(UNKNOWN)['unknownRefused'])
            self.assertIsNone(peer.poll())
        finally:
            if peer.poll() is None:signal.pidfd_send_signal(fd,signal.SIGKILL)
            peer.wait(timeout=2);os.close(fd)
    def test_pre_release_checkpoint_failure_leaves_no_generation(self):
        result=self.run_owned(KNOWN,mode='fail-before-release',expected=7)
        self.assertEqual(result['failure'],'RuntimeError');self.assertTrue(result['noChildren'])
    def test_communication_timeout_owned_supervisor_settles(self):
        result=self.run_owned('import time;time.sleep(.2)',timeout=.001)
        self.assertTrue(result['timed']);self.assertTrue(result['supervisorReaped']);self.assertTrue(result['noChildren'])
    def test_subreaper_failure_refused(self):
        fake=type('L',(),{})()
        fake.prctl=type('F',(),{'__call__':lambda self,*a:-1})()
        with patch('child_ledger.ctypes.CDLL',return_value=fake):
            with self.assertRaises(RuntimeError):child_ledger.subreaper()
if __name__=='__main__':unittest.main(verbosity=2)
