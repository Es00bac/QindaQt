#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checked driver subreaper; orphan observations never grant signal authority."""
import ctypes,os,select
from pathlib import Path
from processes import identity

def subreaper():
    libc=ctypes.CDLL(None,use_errno=True)
    libc.prctl.argtypes=[ctypes.c_int,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_ulong]
    libc.prctl.restype=ctypes.c_int
    if libc.prctl(36,1,0,0,0)!=0:raise RuntimeError("subreaper admission failed")
    value=ctypes.c_int()
    if libc.prctl(37,ctypes.cast(ctypes.byref(value),ctypes.c_void_p).value,0,0,0)!=0 or value.value!=1:
        raise RuntimeError("subreaper readback failed")
    return True

def children(pid):
    raw=Path("/proc/"+str(pid)+"/task/"+str(pid)+"/children").read_text()
    if len(raw)>16384:raise RuntimeError("child census output bound")
    return [int(n) for n in raw.split()]

class ChildLedger:
    def __init__(self):
        subreaper();self.direct={};self.observed={};self.failed=False;self.reaped=[]
        if children(os.getpid()):raise RuntimeError("unexpected initial direct children")
    def register(self,child,label):
        pid=child.process.pid
        if pid in self.direct or len(self.direct)>=4:raise RuntimeError("direct child identity/count bound")
        child.check_identity();self.direct[pid]=(child,label)
    def checkpoint(self):
        # Kernel direct children includes subreaper-adopted double-fork survivors.
        # Only Popen owners can signal; observed descendants retain pidfds solely
        # for retirement evidence, even after their numeric ancestry changes.
        todo=children(os.getpid());visited=set();unknown=[]
        while todo:
            pid=todo.pop()
            if pid in visited:continue
            visited.add(pid)
            if len(visited)>512:self.failed=True;raise RuntimeError("child census bound")
            if pid in self.direct:
                child,label=self.direct[pid];now=identity(pid)
                if now and now["starttime"]!=child.initial["starttime"]:
                    self.failed=True;raise RuntimeError("direct child numeric PID reused")
                todo+=self._children(pid);continue
            now=identity(pid)
            if not now:continue
            if pid not in self.observed:
                if now["parent"] not in self.direct and now["parent"] not in self.observed:
                    unknown.append(pid);continue
                if len(self.observed)>=512:self.failed=True;raise RuntimeError("held descendant bound")
                fd=os.pidfd_open(pid)
                again=identity(pid)
                if not again or again["starttime"]!=now["starttime"]:
                    os.close(fd);self.failed=True;raise RuntimeError("descendant lifetime changed")
                self.observed[pid]={"fd":fd,"starttime":now["starttime"],"reaped":False}
            else:
                item=self.observed[pid]
                if select.select([item["fd"]],[],[],0)[0] or now["starttime"]!=item["starttime"]:
                    self.failed=True;raise RuntimeError("descendant incarnation replaced")
            todo+=self._children(pid)
        if unknown:
            self.failed=True;raise RuntimeError("unknown adopted child; no signal authority")
    def _children(self,pid):
        try:return children(pid)
        except FileNotFoundError:return []
    def final(self):
        self.checkpoint()
        for pid,item in self.observed.items():
            if not select.select([item["fd"]],[],[],0)[0]:
                self.failed=True;raise RuntimeError("observed descendant still live")
        # Popen direct children must have been reaped by their own owners first.
        if any(not c.reaped for c,label in self.direct.values()):
            self.failed=True;raise RuntimeError("direct child not reaped")
        for pid in children(os.getpid()):
            if pid not in self.observed:
                self.failed=True;raise RuntimeError("unknown remaining adopted child")
            got,status=os.waitpid(pid,os.WNOHANG)
            if got!=pid:self.failed=True;raise RuntimeError("adopted child still running")
            self.observed[pid]["reaped"]=True;self.reaped.append({"pid":pid,"status":status})
        if children(os.getpid()):self.failed=True;raise RuntimeError("remaining driver children")
        if self.failed:raise RuntimeError("orphan failure remains latched")
        return {"subreaperChecked":True,"directReaped":len(self.direct),
                "observedRetired":len(self.observed),"adoptedReaped":self.reaped,
                "unknownSurvivors":False,"qualified":True}
    def close(self):
        for item in self.observed.values():os.close(item["fd"])
