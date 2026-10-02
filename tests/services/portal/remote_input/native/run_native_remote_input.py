#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Private remote-input journey: private bus, staged production compositor
with the candidate eis plugin shadowing the stock one, real frontend. No host
bus, input, display, clipboard or activation directory is used.
Args: fixture consent_helper staged_compositor candidate_eis_plugin_dir"""
import os, pathlib, select, subprocess, sys, tempfile, time
fixture, helper, compositor, candidate = map(pathlib.Path, sys.argv[1:5])
if not compositor.is_file() or not (candidate / "qindaqt-kwin/plugins/eis.so").is_file():
    print("private production compositor or candidate eis plugin unavailable"); sys.exit(77)
os.umask(0o077)
children = []
with tempfile.TemporaryDirectory(prefix="qindaqt-native-remote-input-") as tmp:
    root = pathlib.Path(tmp); runtime = root / "runtime"; runtime.mkdir(mode=0o700)
    env = dict(os.environ)
    for key in ("WAYLAND_SOCKET", "WAYLAND_DISPLAY", "DISPLAY", "DBUS_SESSION_BUS_ADDRESS", "DBUS_SYSTEM_BUS_ADDRESS",
                "QT_WAYLAND_SHELL_INTEGRATION", "QT_QPA_PLATFORM", "QT_PLUGIN_PATH", "LD_PRELOAD", "LIBEI_SOCKET"):
        env.pop(key, None)
    env.update(HOME=str(root), XDG_RUNTIME_DIR=str(runtime), XDG_CONFIG_HOME=str(root/"config"),
        XDG_DATA_HOME=str(root/"data"), XDG_CACHE_HOME=str(root/"cache"), XDG_STATE_HOME=str(root/"state"),
        DBUS_SYSTEM_BUS_ADDRESS="unix:path="+str(root/"no-system-bus"), QT_QPA_PLATFORM="wayland",
        QT_QUICK_BACKEND="software", KWIN_COMPOSE="Q", QINDAQT_PORTAL_TEST_HELPER=str(helper),
        QINDAQT_PORTAL_TEST_AUDIT=str(root/"consent.audit"), QT_FATAL_WARNINGS="1")
    portals = root/"portals"; portals.mkdir()
    # Task-owned overlay only: production metadata/selector stay on KDE.
    (portals/"qindaqt.portal").write_text("[portal]\nDBusName=org.freedesktop.impl.portal.desktop.qindaqt\n"
        "Interfaces=org.freedesktop.impl.portal.Access;org.freedesktop.impl.portal.RemoteDesktop;"
        "org.freedesktop.impl.portal.InputCapture;org.freedesktop.impl.portal.Clipboard;\nUseIn=qindaqt;\n")
    (portals/"qindaqt-portals.conf").write_text("[preferred]\ndefault=none\norg.freedesktop.impl.portal.Access=qindaqt\n"
        "org.freedesktop.impl.portal.RemoteDesktop=qindaqt\norg.freedesktop.impl.portal.InputCapture=qindaqt\n"
        "org.freedesktop.impl.portal.Clipboard=qindaqt\n")
    (portals/"portals.conf").write_text("[preferred]\ndefault=none\n")
    env.update(XDG_CURRENT_DESKTOP="qindaqt", XDG_DESKTOP_PORTAL_DIR=str(portals),
        XDG_DATA_DIRS=str(root/"empty-data"), XDG_CONFIG_DIRS=str(root/"empty-config"))
    stage = compositor.parent.parent
    plugins = root/"plugins"
    # Private plugin tree: every staged plugin by symlink, candidate eis.so instead of the stock one.
    for source in (stage/"lib64/qt6/plugins").rglob("*"):
        target = plugins/source.relative_to(stage/"lib64/qt6/plugins")
        if source.is_dir(): target.mkdir(parents=True, exist_ok=True)
        elif source.name != "eis.so" or "qindaqt-kwin" not in source.parts:
            target.parent.mkdir(parents=True, exist_ok=True); target.symlink_to(source)
    (plugins/"qindaqt-kwin/plugins/eis.so").symlink_to(candidate/"qindaqt-kwin/plugins/eis.so")
    env["LD_LIBRARY_PATH"] = str(stage/"lib64")
    def spawn(args, **kw):
        p = subprocess.Popen(list(map(str, args)), env=env, **kw); children.append(p); return p
    try:
        config = root/"bus.conf"
        config.write_text("<busconfig><type>session</type><listen>unix:path="+str(root/"bus")+"</listen><auth>EXTERNAL</auth><policy context='default'><allow own='*'/><allow send_destination='*'/><allow receive_sender='*'/></policy></busconfig>")
        bus = spawn(["dbus-daemon", "--nofork", "--config-file="+str(config), "--print-address=1"], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        if not select.select([bus.stdout], [], [], 5)[0]: raise RuntimeError("private bus unavailable")
        env["DBUS_SESSION_BUS_ADDRESS"] = bus.stdout.readline().decode().strip()
        log = (root/"compositor.log").open("wb")
        producer = dict(env); producer.pop("QT_FATAL_WARNINGS", None); producer["QT_PLUGIN_PATH"] = str(plugins)
        comp = subprocess.Popen([str(compositor), "--virtual", "--width", "1000", "--height", "760", "--socket", "qindaqt-7",
                                 "--no-global-shortcuts"], env=producer, stdout=log, stderr=subprocess.STDOUT); children.append(comp)
        env["QINDAQT_PORTAL_TEST_COMPOSITOR_PID"] = str(comp.pid)
        env["WAYLAND_DISPLAY"] = "qindaqt-7"
        deadline = time.monotonic()+15
        while not (runtime/"qindaqt-7").exists() and comp.poll() is None and time.monotonic() < deadline: time.sleep(.05)
        if not (runtime/"qindaqt-7").exists(): raise RuntimeError("private compositor unavailable")
        time.sleep(1)
        result = subprocess.run([str(fixture), *sys.argv[5:]], env=env, timeout=150)
        if result.returncode == 0: assert comp.poll() is None, "production compositor exited"
        code = result.returncode
    finally:
        for child in reversed(children):
            if child.poll() is None: child.terminate()
            try: child.wait(timeout=5)
            except subprocess.TimeoutExpired: child.kill(); child.wait(timeout=5)
        evidence = pathlib.Path(os.environ.get("QINDAQT_REMOTE_INPUT_EVIDENCE", ""))
        if evidence.name:
            evidence.mkdir(parents=True, exist_ok=True)
            for name in ("compositor.log", "consent.audit"):
                if (root/name).exists(): (evidence/name).write_bytes((root/name).read_bytes())
    sys.exit(code)
