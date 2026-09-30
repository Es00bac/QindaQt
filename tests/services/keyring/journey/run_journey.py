#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real private ordinary Wayland journey. Never uses an ambient bus/display."""
import collections, json, os, pathlib, select, subprocess, sys, tempfile, time
ui, daemon, settings, prompt, clipboard, compositor, schemas=map(pathlib.Path,sys.argv[1:])
if not compositor.is_file():
    print("private production compositor unavailable");sys.exit(77)
os.umask(0o077)
children=[]
with tempfile.TemporaryDirectory(prefix="qindaqt-native-ui-") as tmp:
    root=pathlib.Path(tmp);runtime=root/"runtime";runtime.mkdir(mode=0o700)
    env=dict(os.environ)
    for key in ("WAYLAND_SOCKET","WAYLAND_DISPLAY","DISPLAY","DBUS_SESSION_BUS_ADDRESS","DBUS_SYSTEM_BUS_ADDRESS","QT_WAYLAND_SHELL_INTEGRATION","QT_QPA_PLATFORM"):
        env.pop(key,None)
    env.update(HOME=str(root),XDG_RUNTIME_DIR=str(runtime),XDG_CONFIG_HOME=str(root/"config"),XDG_DATA_HOME=str(root/"data"),XDG_CACHE_HOME=str(root/"cache"),XDG_STATE_HOME=str(root/"state"),
        DBUS_SYSTEM_BUS_ADDRESS="unix:path="+str(root/"no-system-bus"),QT_QPA_PLATFORM="wayland",QT_QUICK_BACKEND="software",KWIN_COMPOSE="Q",
        QINDAQT_SETTINGS_SCHEMA_DIR=str(schemas),QINDAQT_JOURNEY_STORAGE=str(root/"store"),QINDAQT_JOURNEY_CLIPBOARD=str(clipboard),
        QINDAQT_JOURNEY_DAEMON=str(daemon),QINDAQT_JOURNEY_PROMPT=str(prompt),QINDAQT_JOURNEY_PHASE=str(root/"phase"),QINDAQT_JOURNEY_PROMPT_AUDIT=str(root/"prompt.audit"))
    env["LD_LIBRARY_PATH"]=str(compositor.parent.parent/"lib64")
    env["QT_PLUGIN_PATH"]=str(compositor.parent.parent/"lib64/qt6/plugins")
    (root/"phase").write_text("0")
    conf=root/"config/qindaqt";conf.mkdir(parents=True)
    (conf/"settings-v2.json").write_text(json.dumps({"schemaVersion":2,"layer":"user-overrides","values":{"keyring.lockOnScreenLock":False,"keyring.lockAfterIdleMinutes":0}}))
    def spawn(args,**kw):
        p=subprocess.Popen(list(map(str,args)),env=env,**kw);children.append(p);return p
    try:
        bus_config=root/"bus.conf"
        # No activation service directories: fixture clients cannot start host
        # session services while the isolated compositor probes optional names.
        bus_config.write_text("<busconfig><type>session</type><listen>unix:path="+str(root/"bus")+"</listen><auth>EXTERNAL</auth><policy context='default'><allow own='*'/><allow send_destination='*'/><allow receive_sender='*'/></policy></busconfig>")
        bus=spawn(["dbus-daemon","--nofork","--config-file="+str(bus_config),"--print-address=1"],stdout=subprocess.PIPE,stderr=subprocess.DEVNULL)
        if not select.select([bus.stdout],[],[],5)[0]:raise RuntimeError("private bus unavailable")
        env["DBUS_SESSION_BUS_ADDRESS"]=bus.stdout.readline().decode().strip()
        with (root/"compositor.log").open("wb") as log:
            comp=spawn([compositor,"--virtual","--width","1100","--height","760","--socket","qindaqt-7","--no-global-shortcuts"],stdout=log,stderr=subprocess.STDOUT)
            env["QINDAQT_JOURNEY_COMPOSITOR_PID"]=str(comp.pid);env["WAYLAND_DISPLAY"]="qindaqt-7"
            deadline=time.monotonic()+10
            while not (runtime/"qindaqt-7").exists() and comp.poll() is None and time.monotonic()<deadline:time.sleep(.05)
            if not (runtime/"qindaqt-7").exists():raise RuntimeError("ordinary compositor unavailable")
            if subprocess.run([str(ui),"--seed"],env=env).returncode:raise RuntimeError("sealed fixture seed failed")
            spawn([settings],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            keyring=spawn([daemon,"--private-bus",env["DBUS_SESSION_BUS_ADDRESS"],"--storage-root",root/"store","--runtime-root",runtime,"--require-session-display","--policy-fixture","--prompt-program",prompt],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            env["QINDAQT_JOURNEY_DAEMON_PID"]=str(keyring.pid)
            env["QT_FATAL_WARNINGS"]="1"
            result=subprocess.run([str(ui)],env=env,timeout=135)
            if result.returncode==0:assert comp.poll() is None, "private native producer exited"
            lines=(root/"prompt.audit").read_text().splitlines() if (root/"prompt.audit").exists() else []
            print("production prompt visible approvals:",len(lines))
            if result.returncode==0:
                assert collections.Counter(line.rsplit(" ",1)[-1] for line in lines)=={"create":1,"unlock":3,"reveal":8,"change-password":1,"confirm-delete":1}
            sys.exit(result.returncode)
    finally:
        for child in reversed(children):
            if child.poll() is None:child.terminate()
            try:child.wait(timeout=5)
            except subprocess.TimeoutExpired:child.kill();child.wait(timeout=5)
