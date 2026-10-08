#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Root-reviewed private Windows fixture orchestrator; never installed."""
import argparse,hashlib,json,os,signal,subprocess,tempfile,time
from pathlib import Path
from processes import identity,settle,admit_driver
HERE=Path(__file__).resolve().parent
REQUIRED=["/usr/bin/bwrap","/usr/bin/python3","/usr/bin/qindaqt-wm","/usr/bin/qindaqt-kwin","/usr/bin/Xwayland",
"/usr/lib/wine-proton-11.0.2/bin/wine","/usr/lib/wine-proton-11.0.2/bin/wineserver",
"/usr/lib/wine-proton-11.0.2/wine/x86_64-windows/notepad.exe","/usr/lib/wine-proton-11.0.2/wine/x86_64-windows/wordpad.exe",
"/usr/bin/sh","/usr/bin/xkbcomp","/usr/share/xkeyboard-config-2/rules/evdev",
"/usr/share/xkeyboard-config-2/symbols/us","/usr/share/xkeyboard-config-2/keycodes/evdev","/usr/share/xkeyboard-config-2/types/complete","/etc/fonts/fonts.conf",
"/usr/lib64/libX11.so.6","/usr/lib64/libXRes.so.1","/usr/lib64/libXtst.so.6","/usr/lib64/qt6/plugins/qindaqt-kwin/plugins/qindaqt_compositor.so"]
ENV={"PWD":"/fixture","PATH":"/usr/bin","LANG":"C.UTF-8","LC_ALL":"C.UTF-8","HOME":"/fixture/home","USER":"fixture","LOGNAME":"fixture",
"XDG_CONFIG_HOME":"/fixture/config","XDG_DATA_HOME":"/fixture/data","XDG_CACHE_HOME":"/fixture/cache",
"XDG_STATE_HOME":"/fixture/state","XDG_RUNTIME_DIR":"/fixture/runtime","TMPDIR":"/tmp",
"KWIN_COMPOSE":"Q","QT_QPA_PLATFORM":"wayland","QT_QUICK_BACKEND":"software","LIBGL_ALWAYS_SOFTWARE":"1",
"QT_NO_XDG_DESKTOP_PORTAL":"1","GTK_USE_PORTAL":"0","QT_IM_MODULE":"none","GTK_IM_MODULE":"none","XMODIFIERS":"@im=none"}
def command(root,phase):
    args=["/usr/bin/bwrap","--unshare-all","--new-session","--die-with-parent","--clearenv","--hostname","windows-proof",
          "--ro-bind","/usr","/usr","--symlink","usr/bin","/bin","--ro-bind","/lib","/lib","--ro-bind","/lib64","/lib64",
          "--dir","/etc","--ro-bind","/etc/fonts","/etc/fonts","--ro-bind","/etc/ld.so.cache","/etc/ld.so.cache",
          "--ro-bind",str(root/"passwd"),"/etc/passwd","--ro-bind",str(root/"group"),"/etc/group",
          "--proc","/proc","--dev","/dev","--tmpfs","/tmp","--dir","/run","--dir","/home",
          "--ro-bind",str(HERE),"/source","--bind",str(root),"/fixture","--chdir","/fixture"]
    env=dict(ENV)
    env["HOST_NET_NS"]=str(os.stat("/proc/self/ns/net").st_ino);env["HOST_PID_NS"]=str(os.stat("/proc/self/ns/pid").st_ino)
    for k,v in env.items():args+=["--setenv",k,v]
    return args+["--","/usr/bin/python3","/source/inside.py",phase]

def source_identity():
    hashes=json.loads((HERE/"source-sha256.json").read_text())
    if not isinstance(hashes,dict) or set(hashes)!={"outer.py","inside.py","driver.py","processes.py","x11.py","scenario.json","readonly-plan.json","owned_child.py","server_files.py","owned_server.py","child_ledger.py","domain_ledger.py","domain_protocol.py","prefix_worker.py","prefix_domain.py","wine_inputs.py","typing_probe.py","private_document.py"}:
        raise RuntimeError("exact source manifest members absent")
    for name,digest in hashes.items():
        if hashlib.sha256((HERE/name).read_bytes()).hexdigest()!=digest:
            raise RuntimeError("reviewed source manifest mismatch")
    return hashes

def inventory():
    if not hasattr(os,"pidfd_open") or not hasattr(signal,"pidfd_send_signal"):raise RuntimeError("pidfd runtime prerequisites absent")
    result={}
    for name in REQUIRED:
        path=Path(name)
        if not path.is_file():raise RuntimeError("required Portage payload missing: "+name)
        result[name]={"bytes":path.stat().st_size,"sha256":hashlib.sha256(path.read_bytes()).hexdigest()}
    return result

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--plan",action="store_true")
    parser.add_argument("--preflight-approved",action="store_true")
    parser.add_argument("--execute-approved",action="store_true")
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--accepted-preflight",type=Path)
    parser.add_argument("--reviewed-preflight-sha256")
    args=parser.parse_args()
    if sum([args.plan,args.preflight_approved,args.execute_approved])!=1:parser.error("choose one explicit phase")
    if args.output.resolve()!=HERE.parent/"windows-proof-runs":parser.error("output must be own ignored windows-proof-runs")
    payloads=inventory()
    source_hashes=source_identity()
    if args.execute_approved:
        if not args.accepted_preflight or not args.reviewed_preflight_sha256:parser.error("reviewed exact preflight required")
        raw=args.accepted_preflight.read_bytes()
        if hashlib.sha256(raw).hexdigest()!=args.reviewed_preflight_sha256:parser.error("preflight digest differs")
        prior=json.loads(raw)
        if prior.get("phase")!="preflight" or prior.get("exit")!=0 or not prior.get("kernelPidNamespaceContainment") or prior.get("payloads")!=payloads or prior.get("sourceHashes")!=source_hashes:
            parser.error("admissible preflight/payload proof absent")
    if args.plan:
        print(json.dumps({"sourceBase":"a34f15c10fc23e8d272cc504dae5dd6c16e727b0","payloads":payloads,"environmentAllowlist":ENV,"nativeExecuted":False,"prefixCreated":False},indent=2))
        return 0
    # The entire namespace is fresh; only this disposable root is writable.
    args.output.mkdir(parents=True,exist_ok=True)
    root=Path(tempfile.mkdtemp(prefix="private-",dir=args.output.resolve()))
    for name in ["home","config","data","cache","state"]: (root/name).mkdir()
    (root/"runtime").mkdir(mode=0o700)
    (root/"passwd").write_text("fixture:x:"+str(os.getuid())+":"+str(os.getgid())+":fixture:/fixture/home:/usr/bin/false\n")
    (root/"group").write_text("fixture:x:"+str(os.getgid())+":\n")
    receipt={"root":str(root),"phase":"preflight" if args.preflight_approved else "execute","payloads":payloads,"prefixesRetained":True,"sourceHashes":source_hashes}
    cmd=command(root,receipt["phase"]);receipt["argv"]=cmd
    log=(root/"outer.log").open("wb")
    proc=subprocess.Popen(cmd,env={},stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,close_fds=True)
    fd=os.pidfd_open(proc.pid) # own unreaped child: no PID reuse gap
    receipt["wrapperIdentity"]=identity(proc.pid)
    try:
        deadline=time.monotonic()+150
        while proc.poll() is None:
            if time.monotonic()>deadline:raise RuntimeError("outer deadline")
            if sum(p.stat().st_size for p in root.glob("*.log"))>8*1024*1024:raise RuntimeError("output limit")
            time.sleep(.1)
        receipt["exit"]=proc.returncode
        if receipt["phase"]=="execute":
            admit_driver(json.loads((root/"driver-evidence.json").read_text()))
            receipt["driverResultAdmitted"]=True
    except Exception as error:
        receipt["exit"]=1;receipt["errorType"]=type(error).__name__
    finally:
        try:settle(proc,fd)
        except Exception:receipt["namespaceExitUncertain"]=True
        os.close(fd);log.close()
        # No prefix deletion on any path. Namespace teardown is reported, not
        # inferred from a scope query or detached child exit.
        receipt["wrapperExit"]=proc.returncode
        try:
            checked=json.loads((root/"preflight.json").read_text())
            receipt["kernelPidNamespaceContainment"]=checked["pidNamespace"]!=os.stat("/proc/self/ns/pid").st_ino and checked["netNamespace"]!=os.stat("/proc/self/ns/net").st_ino
        except (OSError,KeyError,ValueError):
            receipt["kernelPidNamespaceContainment"]=False
        if not receipt["kernelPidNamespaceContainment"] or receipt.get("namespaceExitUncertain"):
            receipt["exit"]=1
        receipt["cleanupClaim"]="owned wrapper "+("settled" if proc.returncode is not None else "uncertain")+"; prefixes retained; no peer signals"
        (root/"outer-evidence.json").write_text(json.dumps(receipt,indent=2)+"\n")
    print(json.dumps({"root":str(root),"exit":receipt["exit"],"namespaceExitUncertain":receipt.get("namespaceExitUncertain",False)},indent=2))
    return receipt["exit"]
if __name__=="__main__":raise SystemExit(main())
