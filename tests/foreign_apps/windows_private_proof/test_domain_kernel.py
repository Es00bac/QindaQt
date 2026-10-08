# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded real-child controls, authored only until independent safety review."""
import json,os,signal,subprocess,sys,tempfile,time,unittest
from pathlib import Path

# Every generation self-expires independently, including injected failure.
# The intermediate exits immediately: no checkpoint/release handshake.
CHILD=r"""
import os,time,sys,threading
def spawn():
 p=os.fork()
 if p==0:
  os.setsid();q=os.fork()
  if q==0:time.sleep(.6);os._exit(0)
  os._exit(0)
 os.waitpid(p,0)
if sys.argv[1]=='thread':
 t=threading.Thread(target=spawn);t.start();t.join(timeout=1)
else:spawn()
time.sleep(.2)
"""
SUPERVISOR=r"""
import os,json,time,sys,subprocess
from types import SimpleNamespace
from domain_ledger import DomainLedger
from processes import identity
root=sys.argv[1];mode=sys.argv[2];program=__PROGRAM__
ledger=None;owners=[];fds=[];failure=None;result={}
try:
 ledger=DomainLedger()
 for label in ['server','app']:
  child=subprocess.Popen([sys.executable,'-c',program,mode],stdin=subprocess.DEVNULL,
      stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,close_fds=True)
  owners.append(child);fd=os.pidfd_open(child.pid);fds.append(fd)
  first=identity(child.pid)
  if first is None:raise RuntimeError('synthetic owner acquisition')
  wrapper=SimpleNamespace(process=child,initial=first,reaped=False,
                          check_identity=lambda:True)
  ledger.register(wrapper,label)
 if mode=='failure':raise RuntimeError('injected checkpoint failure')
 # Both direct owners exit before the first census; their double-forked
 # descendants have already become children of this causal subreaper.
 for label,child in zip(['server','app'],owners):
  if child.wait(timeout=2)!=0:raise RuntimeError('synthetic owner exit')
  ledger.direct[label].reaped=True
 result['members']=list(ledger.checkpoint().values())
 result['retirement']=ledger.final(time.monotonic()+2)
 result['success']=True
except Exception as error:
 failure=type(error).__name__;result['failure']=failure
finally:
 # No adopted signals. On any failure all independently bounded generations
 # expire naturally; only exact direct unreaped Popen/pidfds can be contained.
 for child,fd in zip(owners,fds):
  if child.poll() is None:
   signal.pidfd_send_signal(fd,signal.SIGKILL)
  child.wait(timeout=2)
 end=time.monotonic()+2;terminal=False;statuses=[]
 while time.monotonic()<end:
  try:pid,status=os.waitpid(-1,os.WNOHANG)
  except ChildProcessError:terminal=True;break
  if pid:statuses.append([pid,status])
  else:time.sleep(.01)
 result['terminalECHILD']=terminal;result['failureWaitStatuses']=statuses
 result['adoptedSignals']=0
 if ledger:ledger.close()
 for fd in fds:os.close(fd)
 print(json.dumps(result),flush=True)
sys.exit(7 if failure else (0 if terminal else 8))
"""
SCRIPT=SUPERVISOR.replace('__PROGRAM__',repr(CHILD))
class KernelDomains(unittest.TestCase):
    def launch(self,mode):
        root=Path(tempfile.mkdtemp(prefix="windows-domain-control-"))
        log=(root/"stdout").open("wb");err=(root/"stderr").open("wb")
        env={"PATH":"/usr/bin","PYTHONPATH":str(Path(__file__).resolve().parent)}
        p=subprocess.Popen([sys.executable,"-c",SCRIPT,str(root),mode],env=env,
            stdin=subprocess.DEVNULL,stdout=log,stderr=err,close_fds=True)
        return root,p,log,err
    def settle(self,row,expected):
        root,p,log,err=row;fd=None
        try:
            fd=os.pidfd_open(p.pid)
            code=p.wait(timeout=6)
            log.flush();err.flush()
            self.assertEqual(code,expected,(root/"stderr").read_text())
            value=json.loads((root/"stdout").read_text())
            self.assertTrue(value["terminalECHILD"])
            self.assertEqual(value["adoptedSignals"],0)
            return value
        finally:
            # Only held direct supervisor, no unknown PID/PGID authority.
            if p.poll() is None and fd is not None:
                signal.pidfd_send_signal(fd,signal.SIGKILL);p.wait(timeout=2)
            elif p.poll() is None:
                p.wait(timeout=6)
            if fd is not None:os.close(fd)
            log.close();err.close()
            # Keep evidence/scratch; do not delete with uncertain descendants.
    def test_immediate_doublefork_and_thread(self):
        for mode in ["ordinary","thread"]:
            v=self.settle(self.launch(mode),0)
            self.assertTrue(v["success"])
            self.assertTrue(v["retirement"]["kernelECHILD"])
            self.assertEqual(len(v["retirement"]["adoptedReaped"]),2)
    def test_two_domains_disjoint(self):
        left=self.launch("ordinary");right=None
        try:
            right=self.launch("ordinary")
            a=self.settle(left,0);b=self.settle(right,0)
            self.assertEqual(len(a["members"]),2);self.assertEqual(len(b["members"]),2)
            self.assertTrue({v["pid"] for v in a["members"]}.isdisjoint(
                            {v["pid"] for v in b["members"]}))
        finally:
            # settle() owns each initiating handle even on assertion errors.
            if right and right[1].poll() is None:self.settle(right,0)
            if left[1].poll() is None:self.settle(left,0)
    def test_injected_failure_self_expiring_children(self):
        v=self.settle(self.launch("failure"),7)
        self.assertEqual(v["failure"],"RuntimeError")
if __name__=="__main__":unittest.main()
