# SPDX-License-Identifier: GPL-3.0-or-later
"""Causal descendants of one childless fixed-launch subreaper; never signal them."""
import os,select,time
from pathlib import Path
from child_ledger import subreaper
from processes import identity

def kernel_empty(waiter=os.waitpid):
    """Only kernel ECHILD proves no waitable/live children; zero never does."""
    try:
        pid,status=waiter(-1,os.WNOHANG)
    except ChildProcessError:
        return True
    if pid:
        raise RuntimeError("unexpected initial waitable child")
    return False

def census_identity(pid):
    try:
        data=Path("/proc/"+str(pid)+"/stat").read_text()
        if len(data)>16384:raise RuntimeError("domain identity bound")
        fields=data[data.rfind(")")+2:].split()
        return {"pid":pid,"parent":int(fields[1]),"starttime":int(fields[19]),
                "state":fields[0]}
    except FileNotFoundError:return None

def task_children(pid):
    # Diagnostic/current membership only, never terminal emptiness authority.
    result=set()
    for task in Path("/proc/"+str(pid)+"/task").iterdir():
        data=(task/"children").read_text()
        if len(data)>16384:raise RuntimeError("domain child census bound")
        result.update(int(v) for v in data.split())
        if len(result)>512:raise RuntimeError("domain child census bound")
    return result

class DomainLedger:
    def __init__(self,clock=time.monotonic,waiter=os.waitpid):
        subreaper();self.clock=clock;self.waiter=waiter
        self.direct={};self.observed={};self.failed=False;self.reaped=[]
        self.ordinal=0;self.lastCensus=[]
        if not kernel_empty(waiter):raise RuntimeError("domain not initially childless")
    def register(self,child,label):
        if label not in {"server","app"} or label in self.direct:
            raise RuntimeError("closed domain launch list")
        child.check_identity()
        if child.initial["parent"]!=os.getpid():raise RuntimeError("not direct domain owner")
        self.direct[label]=child
    def checkpoint(self):
        self.ordinal+=1;todo=list(task_children(os.getpid()));seen=set();census=[]
        try:
            while todo:
                pid=todo.pop()
                if pid in seen:continue
                seen.add(pid)
                if len(seen)>512:raise RuntimeError("domain census bound")
                now=census_identity(pid)
                if now is None:
                    # A disappearing/zombie process is not evidence of emptiness.
                    continue
                census.append(now)
                direct=next((c for c in self.direct.values() if c.process.pid==pid),None)
                if direct:
                    if now["starttime"]!=direct.initial["starttime"]:
                        raise RuntimeError("direct domain lifetime replaced")
                else:
                    item=self.observed.get(pid)
                    if item is None:
                        if len(self.observed)>=512:raise RuntimeError("domain observation bound")
                        fd=os.pidfd_open(pid)
                        again=census_identity(pid)
                        if not again or again["starttime"]!=now["starttime"]:
                            os.close(fd);raise RuntimeError("domain observation changed")
                        # This domain starts childless and launches only its fixed
                        # server/app. Subreaper adoption supplies causal membership,
                        # including a double-fork before any ancestry checkpoint.
                        self.observed[pid]={"fd":fd,"identity":again}
                    elif now["starttime"]!=item["identity"]["starttime"]:
                        raise RuntimeError("observed domain incarnation replaced")
                try:todo.extend(task_children(pid))
                except FileNotFoundError:pass
            self.lastCensus=census
            return {v["pid"]:{k:v[k] for k in ["pid","parent","starttime"]}
                    for v in census if v["state"]!="Z"}
        except Exception:
            self.lastCensus=census;self.failed=True;raise
    def final(self,deadline):
        if self.failed or set(self.direct)!={"server","app"} or any(
                not c.reaped for c in self.direct.values()):
            raise RuntimeError("domain direct settlement absent")
        # No competing waiter: Popen direct owners have already been reaped.
        # Adopted PID observations do not grant signal authority; only wait.
        while True:
            if self.clock()>=deadline:
                self.failed=True;raise RuntimeError("domain retirement deadline")
            try:pid,status=self.waiter(-1,os.WNOHANG)
            except ChildProcessError:
                if self.clock()>=deadline:
                    self.failed=True;raise RuntimeError("domain retirement deadline")
                return {"qualified":True,"subreaperChecked":True,
                        "directReaped":2,"kernelECHILD":True,
                        "adoptedReaped":self.reaped,"adoptedSignals":0}
            if self.clock()>=deadline:
                self.failed=True;raise RuntimeError("domain retirement deadline")
            if pid:
                self.reaped.append({"pid":pid,"status":status})
                if not os.WIFEXITED(status) or os.WEXITSTATUS(status)!=0:
                    self.failed=True;raise RuntimeError("non-normal adopted child exit")
                if len(self.reaped)>512:
                    self.failed=True;raise RuntimeError("domain reap bound")
            else:
                # waitpid==0 means live children remain even if /proc omitted them.
                time.sleep(.01)
    def diagnostic(self):
        return {"checkpoint":self.ordinal,"direct":[{"label":k,**c.initial,
                "reaped":c.reaped} for k,c in self.direct.items()],
                "census":self.lastCensus,"latchedFailure":self.failed,
                "reaped":self.reaped}
    def close(self):
        for v in self.observed.values():os.close(v["fd"])
        self.observed.clear()
