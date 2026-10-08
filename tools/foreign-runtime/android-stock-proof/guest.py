#!/usr/bin/python3
"""Guest PID1 supervisor. Only the reviewed initramfs /init may enter this."""
import configparser
import json
import os
from pathlib import Path
import signal
import subprocess
import time

ROOT = Path("/run/android-proof")
children = []
cancelled = False

def cancel(*_):
    global cancelled
    cancelled = True

def check():
    if cancelled:
        raise RuntimeError("guest-cancelled")

def call(args, **kwargs):
    check()
    result = subprocess.run(args, stdin=subprocess.DEVNULL, timeout=20,
                            check=True, **kwargs)
    check()
    return result

def spawn(args, env=None, user=None):
    check()
    identity = {} if user is None else {"user": user, "group": user, "extra_groups": ()}
    child = subprocess.Popen(args, stdin=subprocess.DEVNULL, env=env, **identity)
    children.append(child)
    check()
    return child

def wait_path(path, timeout=10):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        check()
        if path.exists():
            return
        time.sleep(.05)
    raise RuntimeError("guest-readiness-timeout")

def bus_config(path):
    return ('<busconfig><type>session</type><listen>unix:path=' + path
            + '</listen><auth>EXTERNAL</auth><policy context="default">'
              '<allow user="*"/><allow own="*"/><allow send_destination="*"/>'
              '<allow receive_sender="*"/></policy></busconfig>')

def main():
    if os.getpid() != 1 or Path("/proc/1/comm").read_text().strip() != "python3":
        raise RuntimeError("not-fixture-pid1")
    signal.signal(signal.SIGTERM, cancel)
    signal.signal(signal.SIGINT, cancel)
    os.umask(0o077)
    result = {"schema": 1, "kind": "stock-android-feasibility",
              "success": False, "appIdentityQualified": False,
              "platformOriginQualified": False, "cleanupQualified": False}
    env = {"PATH": "/usr/bin:/usr/sbin:/bin:/sbin", "LANG": "C.UTF-8",
           "HOME": "/root", "DBUS_SYSTEM_BUS_ADDRESS":
           "unix:path=/run/android-proof/system-bus"}
    os.environ.clear(); os.environ.update(env)
    Path("/etc/machine-id").write_text(Path("/proc/sys/kernel/random/uuid").read_text().replace("-", ""))
    try:
        if Path("/etc/waydroid-extra/images").exists():
            raise RuntimeError("unexpected-image-override")
        if Path("/var/lib/waydroid").exists():
            raise RuntimeError("unexpected-existing-runtime")
        # Stock Waydroid creates /var/lib/waydroid with mkdir, not parents.
        # /var is a fresh guest-only tmpfs; no host directory is adopted.
        Path("/var/lib").mkdir(mode=0o755, exist_ok=True)
        for name in ("system", "session"):
            conf = ROOT / (name + ".conf")
            conf.write_text(bus_config(str(ROOT / (name + "-bus"))))
            conf.chmod(0o644)
            spawn(["/usr/bin/dbus-daemon", "--nofork", "--config-file=" + str(conf)])
            wait_path(ROOT / (name + "-bus"))
        call(["/usr/bin/waydroid", "--details-to-stdout", "init"])
        config = configparser.ConfigParser()
        config.read("/var/lib/waydroid/waydroid.cfg")
        expected = {"images_path": "/usr/share/waydroid-extra/images",
                    "system_ota": "None", "vendor_ota": "None", "arch": "x86_64"}
        if any(config.get("waydroid", k, fallback=None) != v for k,v in expected.items()):
            raise RuntimeError("offline-image-admission")
        properties = Path("/var/lib/waydroid/waydroid_base.prop").read_text()
        if "waydroid.updater.disabled=true" not in properties:
            raise RuntimeError("updater-not-disabled")
        if "ro.hardware.egl=swiftshader" not in properties:
            raise RuntimeError("software-renderer-not-selected")
        container = spawn(["/usr/bin/waydroid", "container", "start"])
        import dbus
        bus = dbus.SystemBus()
        ready_end = time.monotonic() + 10
        while not bus.name_has_owner("id.waydro.Container"):
            check()
            if container.poll() is not None or time.monotonic() >= ready_end:
                raise RuntimeError("container-bus-readiness")
            time.sleep(.1)
        owner = str(bus.get_name_owner("id.waydro.Container"))
        bus_daemon = dbus.Interface(bus.get_object("org.freedesktop.DBus",
                                      "/org/freedesktop/DBus"), "org.freedesktop.DBus")
        if int(bus_daemon.GetConnectionUnixProcessID(owner)) != container.pid:
            raise RuntimeError("container-owner-not-held-process")
        home = Path("/home/proof")
        home.mkdir(parents=True); home.parent.chmod(0o755); os.chown(home, 1000, 1000)
        runtime = Path("/run/user/1000")
        runtime.mkdir(parents=True, mode=0o700); runtime.parent.chmod(0o755); os.chown(runtime, 1000, 1000)
        userenv = dict(env, HOME=str(home), USER="proof", LOGNAME="proof",
                       XDG_RUNTIME_DIR=str(runtime), QT_QPA_PLATFORM="offscreen",
                       QT_QUICK_BACKEND="software", LIBGL_ALWAYS_SOFTWARE="1",
                       KWIN_COMPOSE="Q", QT_NO_XDG_DESKTOP_PORTAL="1",
                       DBUS_SESSION_BUS_ADDRESS="unix:path=/run/android-proof/session-bus")
        command = ["/usr/bin/qindaqt-wm", "--kwin", "/usr/bin/qindaqt-kwin",
                   "--plugin-root", "/usr/lib64/qt6/plugins", "--virtual",
                   "--socket", "android-proof", "--width", "1280", "--height", "800",
                   "--scale", "1", "--output-count", "1", "--no-lockscreen",
                   "--no-global-shortcuts", "--test-scenario", "/proof/scenario.json",
                   "--session", "/proof/windows.py"]
        compositor = spawn(command, userenv, user=1000)
        compositor.wait(timeout=220)
        if compositor.returncode != 0:
            raise RuntimeError("nested-compositor-or-window-proof-failed")
        windows = json.loads((home / "windows.json").read_text())
        if windows.get("twoWindowsObserved") is not True:
            raise RuntimeError("two-window-proof-missing")
        result["windows"] = windows
        result["success"] = True
    except BaseException as error:
        result["errorType"] = type(error).__name__
        result["success"] = False
    finally:
        # Entire namespace/kernel is fixture-owned, yet stop observation remains
        # distinct from QEMU containment. No cleanup is inferred from exit 0.
        try:
            call(["/usr/bin/waydroid", "--details-to-stdout", "container", "stop"])
            stopped = call(["/usr/bin/lxc-info", "-P", "/var/lib/waydroid/lxc",
                            "-n", "waydroid", "-sH"], capture_output=True, text=True)
            result["cleanupQualified"] = stopped.stdout.strip() == "STOPPED"
        except BaseException:
            result["cleanupQualified"] = False
        for child in reversed(children):
            try:
                if child.poll() is None:
                    child.terminate()
                child.wait(timeout=3)
            except BaseException:
                result["cleanupQualified"] = False
                # Direct unreaped Popen lifetime, never a discovered PID.
                try:
                    child.kill(); child.wait(timeout=2)
                except BaseException:
                    pass
        result["success"] = result["success"] and result["cleanupQualified"]
        print("QINDA_ANDROID_RESULT=" + json.dumps(result, sort_keys=True), flush=True)
        os.sync()
    return 0 if result["success"] else 1

if __name__ == "__main__":
    raise SystemExit(main())
