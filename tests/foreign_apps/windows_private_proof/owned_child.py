#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Only direct unreaped Popen children grant signal authority."""
import os,select,signal,subprocess
from pathlib import Path
from processes import identity

class OwnedChild:
    def __init__(self,argv,env,log,pass_fds=()):
        self.process=subprocess.Popen(argv,env=env,stdin=subprocess.DEVNULL,
            stdout=log,stderr=subprocess.STDOUT,close_fds=True,pass_fds=pass_fds)
        self.fd=None;self.failed=False;self.retiring=False;self.reaped=False
        try:
            self.fd=os.pidfd_open(self.process.pid)
            self.initial=identity(self.process.pid)
            if not self.initial or self.initial["parent"]!=os.getpid():raise RuntimeError("direct child lifetime acquisition failed")
            self.executable=str(Path(argv[0]).resolve(strict=True))
            self.check_identity()
        except Exception:
            self.failed=True
            # No bare PID signaling if atomic acquisition failed. Outer containment
            # remains mandatory and the partial acquisition is never success.
            if self.fd is not None:
                self.contain();self.close()
            raise
    def dead(self):
        return bool(select.select([self.fd],[],[],0)[0])
    def check_identity(self):
        now=identity(self.process.pid)
        if self.failed or self.dead() or not now or now["starttime"]!=self.initial["starttime"]:
            self.failed=True;raise RuntimeError("initial child lifetime lost")
        try:
            actual=str(Path("/proc/"+str(self.process.pid)+"/exe").resolve(strict=True))
            uid=Path("/proc/"+str(self.process.pid)).stat().st_uid
            if actual!=self.executable or uid!=os.getuid():raise RuntimeError("child identity mismatch")
        except Exception:
            self.failed=True;raise
        return now
    def retire(self,seconds):
        self.check_identity();self.retiring=True
        try:signal.pidfd_send_signal(self.fd,signal.SIGINT)
        except Exception:self.failed=True;raise
        try:
            code=self.process.wait(timeout=seconds);self.reaped=True
        except subprocess.TimeoutExpired:
            self.failed=True;raise RuntimeError("owned retirement timeout")
        if code!=0 or not self.dead():
            self.failed=True;raise RuntimeError("non-normal owned retirement")
        return {"pid":self.process.pid,"starttime":self.initial["starttime"],
                "signal":"SIGINT","exit":code,"pidfdDead":True,"reaped":True}
    def contain(self):
        # Escalation is containment, always failure, never normal retirement.
        self.failed=True
        if self.fd is None:return {"uncertain":True}
        try:
            if not self.dead():signal.pidfd_send_signal(self.fd,signal.SIGKILL)
            code=self.process.wait(timeout=2);self.reaped=True
            return {"contained":True,"exit":code,"pidfdDead":self.dead(),"reaped":True}
        except Exception:return {"uncertain":True}
    def close(self):
        if self.fd is not None:os.close(self.fd);self.fd=None
