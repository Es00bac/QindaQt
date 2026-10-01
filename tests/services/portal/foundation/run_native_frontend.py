#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Private actual production compositor; no host activation/clipboard/data."""
import os, pathlib, select, subprocess, sys, tempfile, time
fixture, helper, compositor, relay, mail, metadata, selection = map(pathlib.Path, sys.argv[1:])
if not compositor.is_file():
    print("private production compositor unavailable"); sys.exit(77)
os.umask(0o077)
children = []
with tempfile.TemporaryDirectory(prefix="qindaqt-native-consent-") as tmp:
    root = pathlib.Path(tmp); runtime = root / "runtime"; runtime.mkdir(mode=0o700)
    env = dict(os.environ)
    for key in ("WAYLAND_SOCKET", "WAYLAND_DISPLAY", "DISPLAY", "DBUS_SESSION_BUS_ADDRESS", "DBUS_SYSTEM_BUS_ADDRESS", "QT_WAYLAND_SHELL_INTEGRATION", "QT_QPA_PLATFORM"):
        env.pop(key, None)
    env.update(HOME=str(root), XDG_RUNTIME_DIR=str(runtime), XDG_CONFIG_HOME=str(root/"config"),
        XDG_DATA_HOME=str(root/"data"), XDG_CACHE_HOME=str(root/"cache"), XDG_STATE_HOME=str(root/"state"),
        DBUS_SYSTEM_BUS_ADDRESS="unix:path="+str(root/"no-system-bus"), QT_QPA_PLATFORM="wayland",
        QT_QUICK_BACKEND="software", KWIN_COMPOSE="Q", QINDAQT_PORTAL_TEST_HELPER=str(helper),
        QINDAQT_PORTAL_TEST_AUDIT=str(root/"consent.audit"), QINDAQT_PORTAL_TEST_RELAY=str(relay),
        QINDAQT_PORTAL_TEST_MAIL=str(mail), QINDAQT_PORTAL_TEST_MAIL_AUDIT=str(root/"mail.audit"), QT_FATAL_WARNINGS="1")
    portals = root/"portals"; portals.mkdir()
    (portals/"qindaqt.portal").write_text(metadata.read_text())
    (portals/"qindaqt-portals.conf").write_text(selection.read_text())
    (portals/"portals.conf").write_text("[preferred]\ndefault=none\n")
    env.update(XDG_CURRENT_DESKTOP="qindaqt", XDG_DESKTOP_PORTAL_DIR=str(portals),
        XDG_DATA_DIRS=str(root/"empty-data"), XDG_CONFIG_DIRS=str(root/"empty-config"))
    env["LD_LIBRARY_PATH"] = str(compositor.parent.parent/"lib64")
    env["QT_PLUGIN_PATH"] = str(compositor.parent.parent/"lib64/qt6/plugins")
    def spawn(args, **kw):
        p = subprocess.Popen(list(map(str, args)), env=env, **kw); children.append(p); return p
    try:
        config = root/"bus.conf"
        config.write_text("<busconfig><type>session</type><listen>unix:path="+str(root/"bus")+"</listen><auth>EXTERNAL</auth><policy context='default'><allow own='*'/><allow send_destination='*'/><allow receive_sender='*'/></policy></busconfig>")
        bus = spawn(["dbus-daemon", "--nofork", "--config-file="+str(config), "--print-address=1"], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        if not select.select([bus.stdout], [], [], 5)[0]: raise RuntimeError("private bus unavailable")
        env["DBUS_SESSION_BUS_ADDRESS"] = bus.stdout.readline().decode().strip()
        with (root/"compositor.log").open("wb") as log:
            producer_env = dict(env); producer_env.pop("QT_FATAL_WARNINGS", None)
            comp = subprocess.Popen([str(compositor), "--virtual", "--width", "1000", "--height", "760", "--socket", "qindaqt-7", "--no-global-shortcuts"], env=producer_env, stdout=log, stderr=subprocess.STDOUT); children.append(comp)
            env["QINDAQT_PORTAL_TEST_COMPOSITOR_PID"] = str(comp.pid)
            env["WAYLAND_DISPLAY"] = "qindaqt-7"
            deadline = time.monotonic()+15
            while not (runtime/"qindaqt-7").exists() and comp.poll() is None and time.monotonic()<deadline: time.sleep(.05)
            if not (runtime/"qindaqt-7").exists(): raise RuntimeError("ordinary compositor unavailable")
            result = subprocess.run([str(fixture)], env=env, timeout=100)
            if result.returncode == 0: assert comp.poll() is None, "production compositor exited"
            sys.exit(result.returncode)
    finally:
        for child in reversed(children):
            if child.poll() is None: child.terminate()
            try: child.wait(timeout=5)
            except subprocess.TimeoutExpired: child.kill(); child.wait(timeout=5)
