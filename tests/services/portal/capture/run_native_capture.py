#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real frontend/native input/frames in entirely private brokers and EGL display."""
import os
import pathlib
import select
import signal
import subprocess
import sys
import tempfile
import time

if len(sys.argv) not in (10, 12):
    sys.exit("expected nine artifacts and optional --case NAME")
fixture, helper, compositor, consent, relay, exporter, pixels, metadata, selection = map(pathlib.Path, sys.argv[1:10])
if not compositor.is_file():
    print("qualified private compositor unavailable")
    sys.exit(77)
# Native Locked is irreversible without a trusted greeter. Each group gets a
# fresh private compositor; no unlock seam or host service is used.
groups = [
    ["screenshotActualPixelsColorAndCancel", "closeInvalidParentAndRequesterLoss",
     "validForeignParentGrantAndLossRetireCaptureAndStream", "actualPipeWireNodeFramesSessionCloseAndCancel", "frontendAndSupervisorLossWithdrawCaptureAndFiles"],
    ["nativeLockStopsActualStreamPendingCaptureAndRetainedFile"],
    ["compositorLossWithdrawsStreamsFilesAndPendingPublication"],
]
# A bounded diagnostic selects unchanged Qt assertions. Only the default command
# qualifies the whole journey; a selected case cannot be reported as full coverage.
if len(sys.argv) == 12:
    if sys.argv[10] != "--case" or sys.argv[11] not in {case for group in groups for case in group}:
        sys.exit("unknown native capture diagnostic case")
    groups = [[sys.argv[11]]]


def group_alive(process):
    try:
        os.killpg(process.pid, 0)
        return True
    except ProcessLookupError:
        return False


def audit_exit(process):
    deadline = time.monotonic() + 2
    while group_alive(process) and time.monotonic() < deadline:
        time.sleep(.02)
    if group_alive(process):
        raise RuntimeError(f"private process group {process.pid} leaked descendants")


os.umask(0o077)
for group in groups:
    children = []
    logs = []
    with tempfile.TemporaryDirectory(prefix="qindaqt-native-capture-") as tmp:
        root = pathlib.Path(tmp)
        runtime = root / "runtime"
        runtime.mkdir(mode=0o700)
        env = dict(os.environ)
        for key in ("WAYLAND_SOCKET", "WAYLAND_DISPLAY", "DISPLAY", "DBUS_SESSION_BUS_ADDRESS", "DBUS_SYSTEM_BUS_ADDRESS", "QT_WAYLAND_SHELL_INTEGRATION", "QT_QPA_PLATFORM", "QT_QPA_PLATFORMTHEME", "PIPEWIRE_REMOTE", "PIPEWIRE_RUNTIME_DIR", "WIREPLUMBER_CONFIG_DIR", "KWIN_SCREENSHOT_NO_PERMISSION_CHECKS", "KWIN_WAYLAND_NO_PERMISSION_CHECKS"):
            env.pop(key, None)
        env.update(HOME=str(root), XDG_RUNTIME_DIR=str(runtime), XDG_CONFIG_HOME=str(root / "config"),
            XDG_DATA_HOME=str(root / "data"), XDG_CACHE_HOME=str(root / "cache"), XDG_STATE_HOME=str(root / "state"),
            DBUS_SYSTEM_BUS_ADDRESS="unix:path=" + str(root / "no-system-bus"), QT_QPA_PLATFORM="wayland",
            QT_QUICK_BACKEND="software", KWIN_COMPOSE="O2", LIBGL_ALWAYS_SOFTWARE="1", QT_STYLE_OVERRIDE="Fusion", QT_FATAL_WARNINGS="1",
            QINDAQT_CAPTURE_TEST_HELPER=str(helper), QINDAQT_PORTAL_TEST_HELPER=str(consent), QINDAQT_PORTAL_TEST_RELAY=str(relay),
            QINDAQT_CAPTURE_TEST_AUDIT=str(root / "capture.audit"), QINDAQT_CAPTURE_TEST_PIXELS=str(pixels), QINDAQT_PORTAL_FOREIGN_EXPORTER=str(exporter), XDG_DATA_DIRS=str(root / "empty-data"), XDG_CONFIG_DIRS=str(root / "empty-config"),
            PIPEWIRE_REMOTE="pipewire-capture", PIPEWIRE_RUNTIME_DIR=str(runtime))
        portals = root / "portals"
        portals.mkdir()
        lines = metadata.read_text().splitlines()
        for index, line in enumerate(lines):
            if line.startswith("Interfaces="):
                interfaces = line.removeprefix("Interfaces=").split(";")
                interfaces += ["org.freedesktop.impl.portal.Screenshot", "org.freedesktop.impl.portal.ScreenCast"]
                lines[index] = "Interfaces=" + ";".join(dict.fromkeys(interfaces))
        (portals / "qindaqt.portal").write_text("\n".join(lines) + "\n")
        chosen = selection.read_text()
        for family in ("Screenshot", "ScreenCast"):
            chosen = chosen.replace(f"org.freedesktop.impl.portal.{family}=kde;gtk;lxqt", f"org.freedesktop.impl.portal.{family}=qindaqt")
        (portals / "qindaqt-portals.conf").write_text(chosen)
        (portals / "portals.conf").write_text("[preferred]\ndefault=none\n")
        env.update(XDG_CURRENT_DESKTOP="qindaqt", XDG_DESKTOP_PORTAL_DIR=str(portals))
        applications = root / "data/applications"
        applications.mkdir(parents=True)
        # Actual restricted peer belongs to resident fixture, not an app-ID hint.
        for name, executable, permission in (
            ("org.test.CaptureResident", fixture, "X-QindaQt-KWin-Wayland-Interfaces=zkde_screencast_unstable_v1"),
            ("org.test.CaptureHelper", helper, "X-QindaQt-KWin-DBus-Restricted-Interfaces=org.kde.KWin.ScreenShot2"),
        ):
            (applications / (name + ".desktop")).write_text(f"[Desktop Entry]\nType=Application\nName=Private capture fixture\nExec={executable}\nNoDisplay=true\n{permission}\n")
        (applications / "org.test.Capture.desktop").write_text("[Desktop Entry]\nType=Application\nName=Private capture caller\nExec=/usr/bin/true\nNoDisplay=true\n")
        env["PATH"] = str(compositor.parent) + os.pathsep + env.get("PATH", "/usr/bin:/bin")
        env["LD_LIBRARY_PATH"] = str(compositor.parent.parent / "lib64")
        env["QT_PLUGIN_PATH"] = str(compositor.parent.parent / "lib64/qt6/plugins")
        pwconfig = root / "pipewire-private.conf"
        pwconfig.write_text("""context.properties = { core.daemon = true core.name = pipewire-capture support.dbus = false }
context.spa-libs = { support.* = support/libspa-support }
context.modules = [
 { name = libpipewire-module-protocol-native }
 { name = libpipewire-module-metadata }
 { name = libpipewire-module-spa-node-factory }
 { name = libpipewire-module-client-node }
 { name = libpipewire-module-access }
 { name = libpipewire-module-link-factory }
]
context.objects = [ { factory = spa-node-factory args = { factory.name = support.node.driver node.name = Private-Driver priority.driver = 20000 } } ]
""")
        # Installed policy scripts only, no hardware/audio/camera monitor profile.
        wpdir = root / "wireplumber"
        wpdir.mkdir()
        wp = pathlib.Path("/usr/share/wireplumber/wireplumber.conf").read_text()
        wp = wp.replace("policy = {", "policy = {\n    support.logind = disabled\n    support.portal-permissionstore = disabled\n    script.client.access-portal = disabled", 1)
        (wpdir / "wireplumber.conf").write_text(wp)
        env["WIREPLUMBER_CONFIG_DIR"] = str(wpdir)
        env["PIPEWIRE_CONFIG_DIR"] = str(root)
        try:
            config = root / "bus.conf"
            config.write_text("<busconfig><type>session</type><listen>unix:path=" + str(root / "bus") + "</listen><auth>EXTERNAL</auth><policy context='default'><allow own='*'/><allow send_destination='*'/><allow receive_sender='*'/></policy></busconfig>")
            bus = subprocess.Popen(["dbus-daemon", "--nofork", "--config-file=" + str(config), "--print-address=1"], env=env, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, start_new_session=True)
            children.append(bus)
            if not select.select([bus.stdout], [], [], 5)[0]:
                raise RuntimeError("private broker unavailable")
            env["DBUS_SESSION_BUS_ADDRESS"] = bus.stdout.readline().decode().strip()
            cache = subprocess.Popen(["kbuildsycoca6", "--noincremental"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
            children.append(cache)
            if cache.wait(timeout=30):
                raise RuntimeError("private desktop cache unavailable")
            audit_exit(cache)
            for name, command in [("pipewire", ["pipewire", "-c", str(pwconfig)]), ("wireplumber", ["wireplumber", "-p", "policy"])]:
                handle = (root / (name + ".log")).open("wb")
                logs.append(handle)
                producer = dict(env)
                producer.pop("QT_FATAL_WARNINGS", None)
                children.append(subprocess.Popen(command, env=producer, stdout=handle, stderr=subprocess.STDOUT, start_new_session=True))
                if name == "pipewire":
                    deadline = time.monotonic() + 10
                    while not (runtime / "pipewire-capture").exists() and time.monotonic() < deadline:
                        time.sleep(.05)
                    if not (runtime / "pipewire-capture").exists():
                        raise RuntimeError("private PipeWire unavailable")
            handle = (root / "compositor.log").open("wb")
            logs.append(handle)
            producer = dict(env)
            producer.pop("QT_FATAL_WARNINGS", None)
            comp = subprocess.Popen([str(compositor), "--virtual", "--width", "1100", "--height", "820", "--socket", "qindaqt-8", "--no-global-shortcuts"], env=producer, stdout=handle, stderr=subprocess.STDOUT, start_new_session=True)
            children.append(comp)
            env["QINDAQT_PORTAL_TEST_COMPOSITOR_PID"] = str(comp.pid)
            env["WAYLAND_DISPLAY"] = "qindaqt-8"
            deadline = time.monotonic() + 15
            while not (runtime / "qindaqt-8").exists() and comp.poll() is None and time.monotonic() < deadline:
                time.sleep(.05)
            if not (runtime / "qindaqt-8").exists():
                raise RuntimeError("private EGL compositor unavailable")
            driver = subprocess.Popen([str(fixture), *group], env=env, start_new_session=True)
            children.append(driver)
            driver_code = driver.wait(timeout=180)
            audit_exit(driver)
            if driver_code:
                sys.exit(driver_code)
            if "compositorLossWithdrawsStreamsFilesAndPendingPublication" in group:
                comp.wait(timeout=5)
                assert all(child.poll() is None for child in children if child not in (comp, cache, driver)), "other private dependency exited"
            else:
                assert all(child.poll() is None for child in children if child not in (cache, driver)), "private dependency exited"
        except BaseException:
            for handle in logs:
                handle.flush()
            for log in root.glob("*.log"):
                print(log.name, log.read_text(errors="replace")[-32768:], file=sys.stderr)
            raise
        finally:
            cleanup_errors = []
            for child in reversed(children):
                if group_alive(child):
                    os.killpg(child.pid, signal.SIGTERM)
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    if group_alive(child):
                        os.killpg(child.pid, signal.SIGKILL)
                    child.wait(timeout=5)
                if group_alive(child):
                    os.killpg(child.pid, signal.SIGKILL)
                try:
                    audit_exit(child)
                except RuntimeError as error:
                    cleanup_errors.append(str(error))
            for handle in logs:
                handle.close()
            if cleanup_errors:
                raise RuntimeError("; ".join(cleanup_errors))
