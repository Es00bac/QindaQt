#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exactly two direct fixed launches in one private childless subreaper domain."""
import json,os,socket,sys,time,math,select
from pathlib import Path
from processes import identity
from owned_child import OwnedChild
from owned_server import OwnedServer
from domain_ledger import DomainLedger
from domain_protocol import receive,send,request,require_time
from wine_inputs import ROOT,LOADER,SERVER,PE,wine_environment

def main():
    control=socket.socket(fileno=int(sys.argv[1]))
    ledger=None;server=None;app=None;logs=[];failed=False;retired=None
    parentfd=None;parent=None;name=None;nonce=None;sequence=0;deadline=time.monotonic()+110
    own=identity(os.getpid())
    evidence={"supervisor":own,"qualified":False,"adoptedSignals":0}
    def parent_current():
        require_time(deadline)
        if parentfd is None or select.select([parentfd],[],[],0)[0] or identity(parent["pid"])!=parent:
            raise RuntimeError("domain initiating owner lost")
    def log_budget():
        for path in [ROOT/(name+"-server.log"),ROOT/(name+".log")]:
            if path.exists() and path.stat().st_size>1048576:
                raise RuntimeError("domain child log bound")
    def checkpoint():
        parent_current();log_budget()
        if server and server.ready:server.guard()
        values=ledger.checkpoint()
        require_time(deadline)
        return {"members":[v for v in values.values()],
                "diagnostic":ledger.diagnostic()}
    def retire():
        nonlocal failed,retired
        if retired is not None:raise RuntimeError("domain retirement replay")
        result={"qualified":False,"server":None,"app":None,"children":None}
        try:
            parent_current();log_budget();result["server"]=server.retire();parent_current();log_budget()
            code=app.process.wait(timeout=min(3,require_time(deadline)))
            app.reaped=True
            result["app"]={**app.initial,"exit":code,"reaped":True,
                           "pidfdDead":app.dead()}
            parent_current()
            if code!=0 or not app.dead():raise RuntimeError("domain application retirement refused")
            result["children"]=ledger.final(deadline)
            parent_current();log_budget();result["qualified"]=True
        except Exception as error:
            failed=True;result["errorType"]=type(error).__name__
            result["refusal"]=str(error)[:128] if isinstance(error,RuntimeError) else "domain-retirement-failed"
        retired=result
        if not result["qualified"]:raise RuntimeError("domain retirement refused")
        return result
    try:
        initial=receive(control,deadline)
        if not isinstance(initial,dict) or set(initial)!={"domain","nonce","deadline","parent"}:
            raise RuntimeError("domain startup schema")
        name=initial["domain"];nonce=initial["nonce"]
        if name not in {"app-a","app-b"} or not isinstance(nonce,str) or len(nonce)!=64 or any(v not in "0123456789abcdef" for v in nonce):
            raise RuntimeError("domain fixed input refusal")
        if initial["parent"]!=identity(os.getppid()) or own["parent"]!=os.getppid():
            raise RuntimeError("domain initiating lifetime mismatch")
        parent=initial["parent"];parentfd=os.pidfd_open(parent["pid"])
        if identity(parent["pid"])!=parent or select.select([parentfd],[],[],0)[0]:
            raise RuntimeError("domain initiating owner changed")
        supplied=initial["deadline"]
        if type(supplied) not in (int,float) or not math.isfinite(supplied) or supplied>deadline:
            raise RuntimeError("domain deadline broadened")
        deadline=supplied;require_time(deadline)
        ledger=DomainLedger()
        fixture={"app-a":"notepad.exe","app-b":"wordpad.exe"}[name]
        env=wine_environment(name)
        for directory in [Path(env["HOME"]),Path(env["WINEPREFIX"]),
                *(Path(env[k]) for k in ["XDG_CONFIG_HOME","XDG_DATA_HOME","XDG_CACHE_HOME","XDG_STATE_HOME"])]:
            directory.mkdir(parents=True,exist_ok=True)
        serverlog=(ROOT/(name+"-server.log")).open("xb");logs.append(serverlog)
        server=OwnedServer(Path(env["WINEPREFIX"]),SERVER,env,serverlog,deadline)
        ledger.register(server.child,"server")
        ready=server.start(monitor=checkpoint)
        parent_current()
        applog=(ROOT/(name+".log")).open("xb");logs.append(applog)
        app=OwnedChild([LOADER,PE+fixture],env,applog)
        ledger.register(app,"app")
        checkpoint()
        payload={"subreaperChecked":True,"serverReadiness":{**ready,
                 "pid":server.child.process.pid,"starttime":server.child.initial["starttime"]},
                 "appLaunch":app.initial,"prefix":env["WINEPREFIX"],"fixedProgram":fixture}
        send(control,{"domain":name,"nonce":nonce,"sequence":0,
             "supervisor":own,"passed":True,"payload":payload},deadline)
        evidence["startup"]=payload
        while True:
            sequence+=1
            operation=request(receive(control,deadline),name,nonce,sequence)
            parent_current()
            if operation=="checkpoint":
                if retired is not None:raise RuntimeError("checkpoint after retirement")
                payload=checkpoint()
            elif operation=="retire":payload=retire()
            else:
                if failed or retired is None or not retired["qualified"]:
                    raise RuntimeError("domain finish before retirement")
                payload={"qualified":True,"retirement":retired}
            parent_current()
            send(control,{"domain":name,"nonce":nonce,"sequence":sequence,
                 "supervisor":own,"passed":True,"payload":payload},deadline)
            if operation=="finish":
                evidence["qualified"]=True;break
    except Exception as error:
        failed=True;evidence.update(errorType=type(error).__name__,
            refusal=str(error)[:128] if isinstance(error,RuntimeError) else "domain-operation-failed")
        if name and nonce:
            try:send(control,{"domain":name,"nonce":nonce,"sequence":sequence,
                    "supervisor":own,"passed":False,"payload":evidence},deadline)
            except Exception:evidence["controlFailureRetained"]=True
    finally:
        evidence["retirement"]=retired
        if ledger:evidence["diagnostic"]=ledger.diagnostic()
        if not evidence["qualified"]:
            # Only held direct Popen owners can receive containment signals.
            # Adopted/live observed peers never get signal authority.
            evidence["containment"]={}
            if server:evidence["containment"]["server"]=server.contain()
            if app:evidence["containment"]["app"]=app.contain()
        if server:server.close()
        if app:app.close()
        if ledger:ledger.close()
        for log in logs:log.close()
        control.close()
        if parentfd is not None:os.close(parentfd)
    if name:
        path=ROOT/(name+"-domain-evidence.json")
        try:require_time(deadline)
        except RuntimeError:evidence["qualified"]=False;failed=True
        path.write_text(json.dumps(evidence,indent=2)+"\n")
        if evidence["qualified"]:
            try:require_time(deadline)
            except RuntimeError:
                evidence["qualified"]=False;failed=True
                path.write_text(json.dumps(evidence,indent=2)+"\n")
    return 0 if evidence["qualified"] and not failed else 1
if __name__=="__main__":raise SystemExit(main())
