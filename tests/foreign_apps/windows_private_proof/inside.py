#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exclusive fixture namespace entry; preflight must pass before any prefix."""
import json,os,stat,subprocess,sys
from pathlib import Path
from processes import settle,wait,admit_driver
from prefix_domain import supervisor_interpreter
ROOT=Path("/fixture")
def validate_public_runtime(root=Path("/")):
    # AGENT-GUARD: synthetic merged-/usr alias only; no host /bin or /etc bind.
    # Xwayland Popen requires /bin/sh even when xkbcomp and XKB data are in /usr.
    if not (root/"bin").is_symlink() or os.readlink(root/"bin")!="usr/bin":
        raise RuntimeError("private merged-usr shell alias absent")
    shell=(root/"bin/sh").resolve(strict=True)
    if shell!=(root/"usr/bin/bash").resolve(strict=True) or not shell.is_file():
        raise RuntimeError("private shell target mismatch")
    required=["usr/bin/xkbcomp","usr/share/xkeyboard-config-2/rules/evdev",
              "usr/share/xkeyboard-config-2/symbols/us",
              "usr/share/xkeyboard-config-2/keycodes/evdev",
              "usr/share/xkeyboard-config-2/types/complete","etc/fonts/fonts.conf"]
    for name in required:
        file=root/name
        if not file.is_file() or file.stat().st_size==0:
            raise RuntimeError("public runtime prerequisite absent")
    return {"mergedUsrShell":True,"xkbCompilerAndData":True,"fontConfig":True}

def preflight():
    if supervisor_interpreter()!=os.environ.get("FIXTURE_PYTHON"):
        raise RuntimeError("private interpreter differs from inventoried payload")
    net=os.stat("/proc/self/ns/net").st_ino
    pid=os.stat("/proc/self/ns/pid").st_ino
    if net==int(os.environ["HOST_NET_NS"]) or pid==int(os.environ["HOST_PID_NS"]):
        raise RuntimeError("required namespace isolation absent")
    for path in ["/home/cabewse","/run/user","/dev/dri","/dev/snd","/etc/resolv.conf"]:
        if Path(path).exists():raise RuntimeError("forbidden host resource mounted")
    routes=Path("/proc/net/route").read_text().splitlines()[1:]
    if any(row.split()[1]=="00000000" for row in routes):raise RuntimeError("network default route present")
    assert os.environ["HOME"]=="/fixture/home"
    assert os.environ.get("PWD")=="/fixture"
    assert Path.cwd()==ROOT
    allowed_keys={"PWD","PATH","LANG","LC_ALL","HOME","USER","LOGNAME","XDG_CONFIG_HOME","XDG_DATA_HOME","XDG_CACHE_HOME","XDG_STATE_HOME","XDG_RUNTIME_DIR","TMPDIR","KWIN_COMPOSE","QT_QPA_PLATFORM","QT_QUICK_BACKEND","LIBGL_ALWAYS_SOFTWARE","QT_NO_XDG_DESKTOP_PORTAL","GTK_USE_PORTAL","HOST_NET_NS","HOST_PID_NS","QT_IM_MODULE","GTK_IM_MODULE","XMODIFIERS","FIXTURE_PYTHON"}
    diagnostic={"unexpectedKeyNames":sorted(set(os.environ)-allowed_keys),"fixedValueMatches":{"PWD_is_fixture":os.environ.get("PWD")=="/fixture","LC_CTYPE_is_C_UTF8":os.environ.get("LC_CTYPE")=="C.UTF-8"}}
    (ROOT/"preflight-diagnostic.json").write_text(json.dumps(diagnostic,indent=2))
    assert set(os.environ).issubset(allowed_keys)
    public_data=validate_public_runtime()
    value={"publicRuntime":public_data,"netNamespace":net,"pidNamespace":pid,"networkRouteAbsent":True,"hostHomeAudioGraphicsAbsent":True,"environmentAllowlist":sorted(os.environ),"prefixInitialized":False,"supervisorInterpreter":supervisor_interpreter()}
    (ROOT/"preflight.json").write_text(json.dumps(value,indent=2))
    return value

def prepare_x11_directory(path=Path("/tmp/.X11-unix")):
    # The installed fork requires a sticky owned socket directory. This is
    # exclusively the bwrap-created tmpfs, never a host socket/tmp bind.
    path.mkdir(mode=0o700) # fail closed if any unexpected entry already exists
    fd=os.open(path,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW|os.O_CLOEXEC)
    try:
        info=os.fstat(fd)
        if not stat.S_ISDIR(info.st_mode) or info.st_uid!=os.getuid():
            raise RuntimeError("private X11 socket directory owner/type mismatch")
        os.fchmod(fd,0o1777)
        if stat.S_IMODE(os.fstat(fd).st_mode)!=0o1777:
            raise RuntimeError("private X11 socket directory mode mismatch")
    finally:os.close(fd)

def main():
    preflight()
    if sys.argv[1]=="preflight":return 0
    prepare_x11_directory()
    buslog=(ROOT/"bus.log").open("wb")
    config=ROOT/"bus.conf"
    config.write_text("""<busconfig><type>session</type><listen>unix:path=/fixture/runtime/bus</listen><auth>EXTERNAL</auth><policy context="default"><allow user="*"/><allow own="*"/><allow send_destination="*"/><allow receive_sender="*"/></policy></busconfig>""")
    bus=subprocess.Popen(["/usr/bin/dbus-daemon","--nofork","--config-file="+str(config)],stdout=buslog,stderr=subprocess.STDOUT)
    busfd=os.pidfd_open(bus.pid);launcher=None;launcherfd=None;log=None
    try:
        wait("private bus socket",lambda:(ROOT/"runtime/bus").exists(),5)
        env=dict(os.environ);env["DBUS_SESSION_BUS_ADDRESS"]="unix:path=/fixture/runtime/bus";env["DBUS_SYSTEM_BUS_ADDRESS"]="unix:path=/nonexistent"
        env["QT_DEBUG_PLUGINS"]="1"
        command=["/usr/bin/qindaqt-wm","--kwin","/usr/bin/qindaqt-kwin","--plugin-root","/usr/lib64/qt6/plugins",
                 "--virtual","--socket","qinda-windows-proof","--width","1280","--height","800","--scale","1","--output-count","1",
                 "--test-scenario","/source/scenario.json","--no-lockscreen","--no-global-shortcuts","--session","/source/driver.py"]
        log=(ROOT/"native.log").open("wb")
        launcher=subprocess.Popen(command,env=env,stdout=log,stderr=subprocess.STDOUT)
        launcherfd=os.pidfd_open(launcher.pid)
        result=launcher.wait(timeout=135)
        if result!=0:return result
        admit_driver(json.loads((ROOT/"driver-evidence.json").read_text()))
        return 0
    finally:
        if launcher is not None:settle(launcher,launcherfd)
        if launcherfd is not None:os.close(launcherfd)
        settle(bus,busfd);os.close(busfd)
        buslog.close()
        if log:log.close()
if __name__=="__main__":raise SystemExit(main())
