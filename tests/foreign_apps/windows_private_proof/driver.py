#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Private compositor exit-with-session child, trusted Wine built-ins only."""
import json,os,subprocess,time
from pathlib import Path
from processes import identity,live_children,settle,wait
from x11 import X11
from owned_child import OwnedChild
from owned_server import OwnedServer
from child_ledger import ChildLedger
ROOT=Path("/fixture")
LOADER="/usr/lib/wine-proton-11.0.2/bin/wine"
SERVER="/usr/lib/wine-proton-11.0.2/bin/wineserver"
PE="/usr/lib/wine-proton-11.0.2/wine/x86_64-windows/"
# Each app receives only these fixed inputs plus nested display/auth created here.
def wine_environment(name):
    env={k:os.environ[k] for k in ["PATH","LANG","LC_ALL","DISPLAY","XDG_RUNTIME_DIR"] if k in os.environ}
    authority=os.environ.get("XAUTHORITY")
    # AGENT-GUARD: KWin's internal-Xwayland path exports exact empty authority.
    # Omit only unset/empty: Wine sees its own private HOME, never host auth.
    if authority is not None and authority!="":
        if authority.isspace():
            raise RuntimeError("Xauthority outside own sandbox")
        auth=Path(authority).resolve()
        if not (str(auth).startswith("/fixture/") or str(auth).startswith("/tmp/")):
            raise RuntimeError("Xauthority outside own sandbox")
        env["XAUTHORITY"]=str(auth)
    home=ROOT/name/"home"
    env.update(HOME=str(home),USER="fixture",LOGNAME="fixture",WINEPREFIX=str(ROOT/name/"prefix"),WINEARCH="win64",
               XDG_CONFIG_HOME=str(home/"config"),XDG_DATA_HOME=str(home/"data"),
               XDG_CACHE_HOME=str(home/"cache"),XDG_STATE_HOME=str(home/"state"),
               TMPDIR="/tmp",WINEDEBUG="-all",
               WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=d;winepulse.drv,winealsa.drv=d;winewayland.drv=d")
    return env

def authority_state():
    # Bounded original state only; never log auth pathname or cookie contents.
    value=os.environ.get("XAUTHORITY")
    return {"present":value is not None,"exactEmpty":value=="",
            "whitespaceOnly":bool(value) and value.isspace(),"cwd":os.getcwd()[:256]}

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
    apps=[];servers=[];serverlogs=[];x=None;ledger=None
    deadline=time.monotonic()+110
    def checkpoint():
        require_driver_time(evidence,deadline)
        if ledger:ledger.checkpoint()
        for server in servers:
            if server.ready:server.guard()
    def bounded_wait(description,predicate,seconds=15):
        def probe():
            checkpoint();value=predicate();checkpoint();return value
        return wait(description,probe,min(seconds,require_driver_time(evidence,deadline)))
    try:
        if not os.environ.get("DISPLAY") or os.environ.get("DBUS_SESSION_BUS_ADDRESS","").find("/fixture/")<0:
            raise RuntimeError("private nested display/bus missing")
        if Path("/dev/dri").exists() or Path("/home/cabewse").exists():raise RuntimeError("host resource visible")
        ledger=ChildLedger()
        evidence["subreaperChecked"]=True
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
            env=wine_environment(name)
            for directory in [Path(env["HOME"]),Path(env["WINEPREFIX"]),*(Path(env[k]) for k in ["XDG_CONFIG_HOME","XDG_DATA_HOME","XDG_CACHE_HOME","XDG_STATE_HOME"])]:
                directory.mkdir(parents=True,exist_ok=True)
            checkpoint()
            serverlog=(ROOT/(name+"-server.log")).open("wb");serverlogs.append(serverlog)
            server=OwnedServer(Path(env["WINEPREFIX"]),SERVER,env,serverlog,deadline)
            servers.append(server);ledger.register(server.child,name+" server")
            readiness=server.start(monitor=checkpoint)
            evidence.setdefault("serverReadiness",[]).append({"app":name,**readiness,
                "pid":server.child.process.pid,"starttime":server.child.initial["starttime"]})
            checkpoint()
            log=(ROOT/(name+".log")).open("wb")
            checkpoint()
            owner=OwnedChild([LOADER,PE+fixture],env,log)
            try:ledger.register(owner,name+" app")
            except Exception:
                owner.contain();owner.close();log.close();raise
            evidence.setdefault("appLaunch",[]).append({"app":name,
                "pid":owner.process.pid,"starttime":owner.initial["starttime"]})
            apps.append({"name":name,"process":owner.process,"owner":owner,
                "environment":env,"known":{},"log":log,"fixture":fixture})
        def pairs():
            results=[]
            for app in apps:
                app["known"].update(live_children([app["process"].pid]))
                alive={pid:value for pid,value in app["known"].items()
                       if identity(pid) and identity(pid)["starttime"]==value["starttime"]}
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
        evidence["serverRetirement"]=[]
        for index,server in enumerate(servers):
            name="app-a" if index==0 else "app-b"
            try:
                # Success requires orderly exact initial lifetime retirement, not
                # a successful namespace kill or an already-dead initial server.
                result=server.retire()
                evidence["serverRetirement"].append({"app":name,**result})
            except Exception as error:
                evidence.setdefault("uncertainCleanup",[]).append(name+" server")
                evidence["serverRetirement"].append({"app":name,"qualified":False,
                    "errorType":type(error).__name__,"containment":server.contain()})
            finally:server.close()
        for app in apps:
            try:
                code=app["process"].wait(timeout=min(3,require_driver_time(evidence,deadline)))
                require_driver_time(evidence,deadline)
                app["owner"].reaped=True
                if code!=0 or not app["owner"].dead():raise RuntimeError("application retirement refused")
                evidence.setdefault("appRetirement",[]).append({"app":app["name"],
                    "pid":app["process"].pid,"starttime":app["owner"].initial["starttime"],
                    "exit":code,"reaped":True,"pidfdDead":True})
            except Exception:
                evidence.setdefault("uncertainCleanup",[]).append(app["name"]+" app")
                app["owner"].contain()
        if ledger:
            try:
                require_driver_time(evidence,deadline)
                evidence["childrenRetirement"]=ledger.final()
                require_driver_time(evidence,deadline)
            except Exception as error:
                evidence.setdefault("uncertainCleanup",[]).append("child-ledger")
                evidence["childrenRetirement"]={"qualified":False,"errorType":type(error).__name__}
            finally:ledger.close()
        for app in apps:app["owner"].close();app["log"].close()
        for log in serverlogs:log.close()
        if x:
            try:x.close()
            except Exception:evidence.setdefault("uncertainCleanup",[]).append("X11")
    return publish_driver_result(evidence,deadline,ROOT/"driver-evidence.json")
if __name__=="__main__":raise SystemExit(main())
