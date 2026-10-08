# SPDX-License-Identifier: GPL-3.0-or-later
"""Owned fixture process identity; never signal a peer by a recycled PID."""
import os,signal,time
from pathlib import Path

def identity(pid):
    try:
        data=Path("/proc/"+str(pid)+"/stat").read_text()
        fields=data[data.rfind(")")+2:].split()
        if fields[0]=="Z":return None
        return {"pid":pid,"parent":int(fields[1]),"starttime":int(fields[19])}
    except (OSError,ValueError,IndexError):
        return None

def live_children(roots):
    # This helper runs in the exclusive fixture PID namespace, not host /proc.
    result={}
    for entry in Path("/proc").iterdir():
        if entry.name.isdigit():
            value=identity(int(entry.name))
            if value:
                if len(result)>=512:raise RuntimeError("private process inventory bound")
                result[value["pid"]]=value
    owned={pid for pid in roots if pid in result}
    for unused in range(len(result)+1):
        newer={pid for pid,v in result.items() if v["parent"] in owned}
        if newer.issubset(owned):break
        owned.update(newer)
    return {pid:result[pid] for pid in owned}

def send_owned(fd,number):
    # pidfd signal is atomic with respect to PID reuse; no bare kill/killpg.
    signal.pidfd_send_signal(fd,number)

def settle(process,fd,grace=3):
    if process.poll() is None:
        send_owned(fd,signal.SIGTERM)
        try:process.wait(timeout=grace)
        except __import__("subprocess").TimeoutExpired:
            send_owned(fd,signal.SIGKILL)
            process.wait(timeout=3)
    return process.returncode

def wait(description,predicate,seconds=15):
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline:
        value=predicate()
        if value:return value
        time.sleep(.05)
    raise RuntimeError("bounded timeout: "+description)

def admit_driver(value):
    """Fixed trusted fixture result admission; launcher exit alone is insufficient."""
    if value.get("deadlineQualified") is not True or value.get("deadlineExpired"):
        raise RuntimeError("driver deadline qualification absent")
    clients=value.get("clients",[])
    steps=value.get("steps",[])
    stops=value.get("serverRetirement",[])
    if value.get("passed") is not True or value.get("uncertainCleanup"):
        raise RuntimeError("driver success/cleanup receipt absent")
    if len(clients)!=2 or {v.get("application") for v in clients}!={"app-a","app-b"}:
        raise RuntimeError("two expected fixture clients absent")
    if {v.get("fixedProgram") for v in clients}!={"notepad.exe","wordpad.exe"} or any(v.get("normalWindowType") is not True or v.get("fixedProgramArgumentObserved") is not True for v in clients):
        raise RuntimeError("ordinary fixed-program window evidence absent")
    for field in ["xid","xresLocalPid","prefix"]:
        if len({v.get(field) for v in clients})!=2 or any(not v.get(field) for v in clients):
            raise RuntimeError("independent fixture identity absent")
    if any(not v.get("starttime") for v in clients):
        raise RuntimeError("process starttime absent")
    if [v.get("stage") for v in steps]!=["resize-a","close-a","resize-b","close-b"]:
        raise RuntimeError("complete two-client steps absent")
    ready=value.get("serverReadiness",[])
    if len(ready)!=2 or {v.get("app") for v in ready}!={"app-a","app-b"}:
        raise RuntimeError("two initial server readiness receipts absent")
    for field in ["pid","prefixIdentity","serverDirectory","lock","socket"]:
        if len({str(v.get(field)) for v in ready})!=2 or any(not v.get(field) for v in ready):
            raise RuntimeError("distinct initial server identity absent")
    if any(v.get("peerPid")!=v.get("pid") or v.get("peerUid")!=os.getuid() or not v.get("starttime") for v in ready):
        raise RuntimeError("initial server peer/lifetime absent")
    required={"qualified":True,"exit":0,"pidfdDead":True,"reaped":True,
              "signal":"SIGINT","heldLockReleased":True,"currentLockSame":True,
              "replacementOwnerAbsent":True}
    if len(stops)!=2 or {v.get("app") for v in stops}!={"app-a","app-b"}:
        raise RuntimeError("complete two-server retirement absent")
    for stop in stops:
        first=next(v for v in ready if v["app"]==stop["app"])
        if any(stop.get(k)!=expected for k,expected in required.items()) or any(
            stop.get(k)!=first.get(k) for k in ["pid","starttime","prefix"]):
            raise RuntimeError("initial server orderly retirement absent")
    launches=value.get("appLaunch",[])
    if len(launches)!=2 or {v.get("app") for v in launches}!={"app-a","app-b"} or len({v.get("pid") for v in launches})!=2 or any(
        not v.get("pid") or not v.get("starttime") for v in launches):
        raise RuntimeError("distinct held application launch lifetimes absent")
    apps=value.get("appRetirement",[])
    if len(apps)!=2 or {v.get("app") for v in apps}!={"app-a","app-b"} or any(
        v.get("exit")!=0 or v.get("reaped") is not True or v.get("pidfdDead") is not True for v in apps):
        raise RuntimeError("application retirement absent")
    for app in apps:
        first=next(v for v in launches if v["app"]==app["app"])
        if any(app.get(k)!=first.get(k) for k in ["pid","starttime"]):
            raise RuntimeError("application lifetime retirement mismatch")
    domains=value.get("lifecycleDomains",[])
    if len(domains)!=2 or {d.get("domain") for d in domains}!={"app-a","app-b"}:
        raise RuntimeError("two causal domains absent")
    if len({d.get("nonce") for d in domains})!=2 or any(not isinstance(d.get("nonce"),str) or len(d["nonce"])!=64 for d in domains):
        raise RuntimeError("independent domain generations absent")
    owners=[d.get("supervisor",{}) for d in domains]
    if any(not o.get("pid") or not o.get("starttime") for o in owners) or len({o["pid"] for o in owners})!=2:
        raise RuntimeError("independent held domain lifetimes absent")
    for domain in domains:
        if any(domain.get(k)!=expected for k,expected in {"qualified":True,"supervisorExit":0,
                "supervisorReaped":True,"supervisorPidfdDead":True}.items()):
            raise RuntimeError("domain supervisor retirement absent")
        retired=domain.get("retirement",{})
        matchedServer=next(v for v in stops if v["app"]==domain["domain"])
        matchedApp=next(v for v in apps if v["app"]==domain["domain"])
        if any(retired.get("server",{}).get(k)!=matchedServer.get(k) for k in
                ["pid","starttime","prefix","qualified","exit","pidfdDead","reaped","signal","heldLockReleased","currentLockSame","replacementOwnerAbsent"]):
            raise RuntimeError("domain server receipt join refused")
        if any(retired.get("app",{}).get(k)!=matchedApp.get(k) for k in
                ["pid","starttime","exit","reaped","pidfdDead"]):
            raise RuntimeError("domain application receipt join refused")
        child=retired.get("children",{})
        if retired.get("qualified") is not True or any(child.get(k)!=expected for k,expected in {
                "qualified":True,"subreaperChecked":True,"directReaped":2,
                "kernelECHILD":True,"adoptedSignals":0}.items()):
            raise RuntimeError("kernel terminal child retirement absent")
    children=value.get("childrenRetirement",{})
    if any(children.get(k)!=expected for k,expected in {
            "qualified":True,"kernelECHILDRequired":True,"perPrefixDomains":2,
            "adoptedSignals":0,"unknownSurvivors":False}.items()) or value.get("subreaperChecked") is not True:
        raise RuntimeError("complete causal domain retirement absent")
    return True
