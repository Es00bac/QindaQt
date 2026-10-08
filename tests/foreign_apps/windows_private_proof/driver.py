#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Private compositor exit-with-session child, trusted Wine built-ins only."""
import json,os,subprocess,time
from pathlib import Path
from processes import identity,wait
from x11 import X11
from prefix_domain import PrefixDomain
from wine_inputs import ROOT,LOADER,SERVER,PE,wine_environment,authority_state

def require_driver_time(evidence,deadline):
    remaining=deadline-time.monotonic()
    if evidence.get("deadlineExpired") or remaining<=0:
        evidence["deadlineExpired"]=True
        raise RuntimeError("overall driver deadline expired")
    return remaining

def qualify_driver(evidence,deadline):
    from processes import admit_driver
    evidence["deadlineQualified"]=False
    require_driver_time(evidence,deadline)
    evidence["deadlineQualified"]=True
    try:
        admit_driver(evidence)
        require_driver_time(evidence,deadline)
    except Exception:
        evidence["deadlineQualified"]=False
        raise
    return True

def publish_driver_result(evidence,deadline,path):
    try:qualify_driver(evidence,deadline);result=0
    except Exception:result=1
    path.write_text(json.dumps(evidence,indent=2)+"\n")
    if result==0:
        try:require_driver_time(evidence,deadline)
        except Exception:
            evidence["deadlineQualified"]=False;result=1
            path.write_text(json.dumps(evidence,indent=2)+"\n")
    return result

def main():
    evidence={"trustedFixturesOnly":True,"prefixesAreNotPerAppSandbox":True,"authenticatedBadge":False,"steps":[],"prefixesRetained":True}
    evidence["xauthorityInput"]=authority_state()
    apps=[];domains=[];documents=[];x=None
    deadline=time.monotonic()+110
    def checkpoint():
        require_driver_time(evidence,deadline)
        for domain in domains:
            if domain.retired is None:domain.checkpoint()
    def bounded_wait(description,predicate,seconds=15):
        def probe():
            checkpoint();value=predicate();checkpoint();return value
        return wait(description,probe,min(seconds,require_driver_time(evidence,deadline)))
    try:
        if not os.environ.get("DISPLAY") or os.environ.get("DBUS_SESSION_BUS_ADDRESS","").find("/fixture/")<0:
            raise RuntimeError("private nested display/bus missing")
        if Path("/dev/dri").exists() or Path("/home/cabewse").exists():raise RuntimeError("host resource visible")
        evidence["subreaperChecked"]=False
        def connect_x11():
            try:return X11()
            except RuntimeError as error:
                if str(error)=="private X11 connect failed":return None
                raise
        x=bounded_wait("private X11 readiness",connect_x11,10)
        evidence["xresVersion"]=x.version
        import dbus
        bus=dbus.SessionBus()
        def ready_iface():
            try:
                iface=dbus.Interface(bus.get_object("org.qindaqt.Compositor","/org/qindaqt/Compositor"),"org.qindaqt.Compositor1")
                value=json.loads(bytes(iface.Windows(byte_arrays=True,timeout=1)).decode())
                return iface if value.get("status")=="ok" else None
            except dbus.DBusException as error:
                if error.get_dbus_name() in {"org.freedesktop.DBus.Error.ServiceUnknown","org.freedesktop.DBus.Error.UnknownObject","org.freedesktop.DBus.Error.UnknownInterface","org.freedesktop.DBus.Error.UnknownMethod"}:return None
                raise
        iface=bounded_wait("private compositor interface readiness",ready_iface,10)
        def snapshot():
            result=json.loads(bytes(iface.Windows(byte_arrays=True,timeout=1)).decode())
            if result.get("status")!="ok":raise RuntimeError("private compositor inventory unavailable")
            return result
        evidence["before"]=snapshot()
        for name,fixture in [("app-a","notepad.exe"),("app-b","wordpad.exe")]:
            checkpoint()
            from private_document import PrivateDocument
            document=PrivateDocument(ROOT,name);documents.append(document)
            domain=PrefixDomain(name,deadline);domains.append(domain)
            ready=domain.startup
            evidence.setdefault("serverReadiness",[]).append({"app":name,**ready["serverReadiness"]})
            evidence.setdefault("appLaunch",[]).append({"app":name,**ready["appLaunch"]})
            apps.append({"name":name,"domain":domain,"fixture":fixture,
                "environment":wine_environment(name),"document":document,"known":{}})
        evidence["subreaperChecked"]=len(domains)==2 and all(d.startup["subreaperChecked"] is True for d in domains)
        def pairs():
            results=[]
            for app in apps:
                observed=app["domain"].checkpoint()["members"]
                alive={}
                for value in observed:
                    now=identity(value["pid"])
                    if now and now["starttime"]==value["starttime"]:alive[value["pid"]]=value
                candidates=[]
                for window in x.windows():
                    pid=x.local_pid(window)
                    if pid in alive and x.normal_window(window):
                        raw=Path("/proc/"+str(pid)+"/cmdline").read_bytes()
                        if len(raw)>16384:raise RuntimeError("owned command metadata bound")
                        args=raw.split(b"\0")
                        # Fixed launched PE path, not title/class or Wine error-dialog hints.
                        if (PE+app["fixture"]).encode() in args:
                            candidates.append((window,pid,alive[pid]))
                if len(candidates)!=1:return None
                results.append(candidates[0])
            return results if results[0][0]!=results[1][0] and results[0][1]!=results[1][1] else None
        selected=bounded_wait("two independent owned XRes clients",pairs,35)
        evidence["clients"]=[{"application":app["name"],"fixedProgram":app["fixture"],"normalWindowType":True,"fixedProgramArgumentObserved":True,"xid":str(window),"xresLocalPid":pid,"starttime":value["starttime"],"prefix":app["environment"]["WINEPREFIX"],"geometry":x.geometry(window)} for app,(window,pid,value) in zip(apps,selected)]
        evidence["twoLiveWindows"]=snapshot()
        a,b=[v[0] for v in selected]
        from typing_probe import TypingIO,probe
        typing=TypingIO(x)
        def current_clients():
            checkpoint()
            current=pairs()
            if not current or any((a[0],a[1],a[2]["starttime"])!=(b[0],b[1],b[2]["starttime"])
                    for a,b in zip(current,selected)):
                raise RuntimeError("typing client incarnation changed")
        for app,(window,pid,value) in zip(apps,selected):
            letter=app["name"][-1]
            typed=probe(typing,window,"qinda "+letter+" 3108",current_clients,
                min(deadline,time.monotonic()+8),ROOT/("typing-"+app["name"]))
            typing.chord(window,"s");current_clients()
            saved=bounded_wait("private document CtrlS content",lambda:app["document"].read_saved("qinda "+letter+" 3108"),3)
            evidence["steps"].append({"stage":"type-"+letter,**typed,**saved})
        original_b=x.geometry(b)
        x.resize(a,520,320)
        bounded_wait("first client server geometry updates after resize request",lambda:x.geometry(a) and x.geometry(a)[2:]==[520,320])
        if x.geometry(b)!=original_b:raise RuntimeError("second client changed during first resize")
        evidence["steps"].append({"stage":"resize-a","serverGeometryOnly":True,"resized":str(a),"geometry":x.geometry(a),"otherUnchanged":True})
        x.request_close(a)
        bounded_wait("first close removes only first client",lambda:a not in x.windows())
        if b not in x.windows() or (not identity(selected[1][1]) or identity(selected[1][1])["starttime"]!=selected[1][2]["starttime"]):raise RuntimeError("second app lost on first close")
        evidence["steps"].append({"stage":"close-a","closed":str(a),"secondStillLive":True,"secondGeometry":x.geometry(b)})
        x.resize(b,560,360)
        bounded_wait("survivor server geometry updates after resize request",lambda:x.geometry(b) and x.geometry(b)[2:]==[560,360])
        evidence["steps"].append({"stage":"resize-b","serverGeometryOnly":True,"geometry":x.geometry(b)})
        x.request_close(b)
        bounded_wait("second client closes",lambda:b not in x.windows())
        evidence["steps"].append({"stage":"close-b","closed":str(b)})
        evidence["after"]=snapshot()
        evidence["passed"]=True
    except Exception as error:
        evidence["passed"]=False;evidence["errorType"]=type(error).__name__
        # Fixed fixture exceptions only; no RPC payload or arbitrary argv logging.
        evidence["failure"]=str(error)[:256] if isinstance(error,RuntimeError) else "fixture-operation-failed"
    finally:
        evidence["serverRetirement"]=[];evidence["appRetirement"]=[]
        evidence["lifecycleDomains"]=[]
        for domain in domains:
            try:
                require_driver_time(evidence,deadline)
                result=domain.retire()
                evidence["lifecycleDomains"].append(result)
                evidence["serverRetirement"].append({"app":domain.name,**result["retirement"]["server"]})
                evidence["appRetirement"].append({"app":domain.name,**result["retirement"]["app"]})
                require_driver_time(evidence,deadline)
            except Exception as error:
                evidence.setdefault("uncertainCleanup",[]).append(domain.name+" domain")
                evidence["lifecycleDomains"].append({"domain":domain.name,"qualified":False,
                    "errorType":type(error).__name__,"refusal":str(error)[:128] if isinstance(error,RuntimeError) else "domain-cleanup-failed",
                    "observedRetirement":domain.retired,"containment":domain.contain()})
            finally:domain.close()
        qualified=len(domains)==2 and len(evidence["lifecycleDomains"])==2 and all(
            v.get("qualified") is True for v in evidence["lifecycleDomains"])
        evidence["childrenRetirement"]={"qualified":qualified,
            "kernelECHILDRequired":True,"perPrefixDomains":len(domains),
            "adoptedSignals":0,"unknownSurvivors":False if qualified else None}
        for document in documents:document.close()
        if x:
            try:x.close()
            except Exception:evidence.setdefault("uncertainCleanup",[]).append("X11")
    return publish_driver_result(evidence,deadline,ROOT/"driver-evidence.json")
if __name__=="__main__":raise SystemExit(main())
