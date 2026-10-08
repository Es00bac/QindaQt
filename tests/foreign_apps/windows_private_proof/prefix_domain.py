# SPDX-License-Identifier: GPL-3.0-or-later
"""Driver-owned private domain worker lifetime; never infer ownership from UID."""
import os,secrets,socket,time,sys,stat
from pathlib import Path
from owned_child import OwnedChild
from domain_protocol import send,receive,receipt,require_time
from processes import identity

def supervisor_interpreter():
    # AGENT-GUARD: Gentoo python3 is a dispatcher that execs another ELF.
    # Pin our actual packaged interpreter before Popen; OwnedChild must still
    # reject every later executable/lifetime replacement (ED-22 private proof).
    path=Path("/proc/self/exe").resolve(strict=True)
    expected=Path(f"/usr/bin/python{sys.version_info.major}.{sys.version_info.minor}")
    info=path.stat()
    if path!=expected or not stat.S_ISREG(info.st_mode) or info.st_uid!=0 or info.st_mode & 0o022:
        raise RuntimeError("private supervisor interpreter provenance mismatch")
    return str(path)

class PrefixDomain:
    def __init__(self,name,deadline):
        if name not in {"app-a","app-b"}:raise RuntimeError("fixed domain name refused")
        self.name=name;self.deadline=deadline;self.nonce=secrets.token_hex(32)
        self.sequence=0;self.failed=False;self.retired=None;self.child=None
        self.sock,remote=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET|socket.SOCK_CLOEXEC)
        self.log=(Path("/fixture")/(name+"-supervisor.log")).open("xb")
        try:
            require_time(deadline)
            self.child=OwnedChild([supervisor_interpreter(),str(Path(__file__).with_name("prefix_worker.py")),str(remote.fileno())],
                dict(os.environ),self.log,pass_fds=(remote.fileno(),))
            remote.close()
            send(self.sock,{"domain":name,"nonce":self.nonce,"deadline":deadline,
                           "parent":identity(os.getpid())},deadline)
            self.startup=self._receive(0)
            expected={"app-a":"notepad.exe","app-b":"wordpad.exe"}[name]
            if self.startup.get("subreaperChecked") is not True or self.startup.get("fixedProgram")!=expected or self.startup.get("prefix")!="/fixture/"+name+"/prefix":
                raise RuntimeError("domain startup declaration refused")
            self.child.check_identity();require_time(deadline)
        except Exception:
            self.failed=True
            if self.child:self.child.contain();self.child.close()
            remote.close();self.sock.close();self.log.close();raise
    def _receive(self,sequence):
        value=receive(self.sock,self.deadline)
        return receipt(value,self.name,self.nonce,sequence,self.child.initial)
    def call(self,operation):
        if self.failed:raise RuntimeError("domain failure remains latched")
        try:
            require_time(self.deadline);self.child.check_identity()
            self.sequence+=1
            send(self.sock,{"domain":self.name,"nonce":self.nonce,
                           "sequence":self.sequence,"operation":operation},self.deadline)
            value=self._receive(self.sequence)
            require_time(self.deadline)
            if operation!="finish":self.child.check_identity()
            return value
        except Exception:self.failed=True;raise
    def checkpoint(self):return self.call("checkpoint")
    def retire(self):
        if self.retired is not None:raise RuntimeError("driver domain retirement replay")
        value=self.call("retire")
        if value.get("qualified") is not True:raise RuntimeError("domain retirement receipt absent")
        self.retired=value
        self.call("finish")
        code=self.child.process.wait(timeout=min(3,require_time(self.deadline)))
        self.child.reaped=True;require_time(self.deadline)
        if code!=0 or not self.child.dead():self.failed=True;raise RuntimeError("domain supervisor settlement refused")
        return {"domain":self.name,"nonce":self.nonce,"supervisor":self.child.initial,
                "qualified":True,"supervisorExit":code,"supervisorReaped":True,
                "supervisorPidfdDead":True,"retirement":value}
    def contain(self):
        self.failed=True
        return self.child.contain() if self.child else {"uncertain":True}
    def close(self):
        if self.child:self.child.close()
        self.sock.close();self.log.close()
