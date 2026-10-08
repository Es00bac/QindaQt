#!/usr/bin/python3
"""Guest-only sequential launch observation, never per-app authority."""
import json
import os
from pathlib import Path
import subprocess
import time

APPS = ("com.android.calculator2", "com.android.deskclock")

def run_command(args, deadline, *, boot_readiness=False):
    remaining = deadline - time.monotonic()
    if remaining <= 0:
        raise RuntimeError("window-proof-deadline")
    # AGENT-GUARD: Stock IPlatform discovery can wait for asynchronous boot.
    # Only boot readiness and the two fixed launch RPCs may consume that budget;
    # timeout remains terminal, never an individual-command retry.
    fixed_app_launch = tuple(args) in tuple(("app", "launch", app) for app in APPS)
    timeout = remaining if boot_readiness or fixed_app_launch else min(10, remaining)
    return subprocess.run(["/usr/bin/waydroid", *args], stdin=subprocess.DEVNULL,
                          capture_output=True, text=True, check=True,
                          timeout=timeout).stdout.strip()


def bounded_output(value):
    if isinstance(value, bytes):
        value = value.decode("utf-8", "replace")
    return value[:2048] if isinstance(value, str) else ""


def main():
    import dbus
    if os.getuid() != 1000 or os.environ.get("HOME") != "/home/proof":
        raise RuntimeError("not-fixture-user")
    if os.environ.get("DBUS_SESSION_BUS_ADDRESS") != "unix:path=/run/android-proof/session-bus":
        raise RuntimeError("not-fixture-bus")
    deadline = time.monotonic() + 190
    def remaining():
        value = deadline - time.monotonic()
        if value <= 0:
            raise RuntimeError("window-proof-deadline")
        return value
    def command(*args, boot_readiness=False):
        return run_command(args, deadline, boot_readiness=boot_readiness)
    session = subprocess.Popen(["/usr/bin/waydroid", "session", "start"],
                               stdin=subprocess.DEVNULL)
    evidence = {"schema": 1, "twoWindowsObserved": False, "steps": [],
                "identityAuthority": False, "renderedInputQualified": False,
                "resizeCloseQualified": False}
    phase = "compositor-baseline"
    try:
        bus = dbus.SessionBus()
        owner = str(bus.get_name_owner("org.qindaqt.Compositor"))
        interface = dbus.Interface(bus.get_object(owner, "/org/qindaqt/Compositor",
                                                  introspect=False),
                                   "org.qindaqt.Compositor1")
        def snapshot():
            remaining()
            if str(bus.get_name_owner("org.qindaqt.Compositor")) != owner:
                raise RuntimeError("compositor-owner-changed")
            raw = bytes(interface.Windows(byte_arrays=True, timeout=min(2, remaining())))
            if len(raw) > 262144:
                raise RuntimeError("window-snapshot-bound")
            value = json.loads(raw)
            if value.get("status") != "ok" or not isinstance(value.get("windows"), list):
                raise RuntimeError("window-snapshot-refused")
            windows = value["windows"]
            if len(windows) > 16:
                raise RuntimeError("unexpected-window-count")
            return {row["id"]: row for row in windows}
        before = snapshot()
        evidence["compositorBaselineObserved"] = True
        phase = "boot-readiness"
        while command("prop", "get", "sys.boot_completed", boot_readiness=True) != "1":
            if session.poll() is not None:
                raise RuntimeError("session-ended-before-readiness")
            time.sleep(.5)
        evidence["bootCompletedObserved"] = True
        phase = "multiwindow-readback"
        command("prop", "set", "persist.waydroid.multi_windows", "true")
        if command("prop", "get", "persist.waydroid.multi_windows") != "true":
            raise RuntimeError("multiwindow-readback")
        observed = []
        previous = before
        for app in APPS:
            phase = "app-launch-" + app
            command("app", "launch", app)
            while True:
                current = snapshot()
                added = set(current) - set(previous)
                if len(added) == 1 and set(previous).issubset(current):
                    window = current[next(iter(added))]
                    geometry = window.get("geometry", {})
                    if geometry.get("width", 0) > 0 and geometry.get("height", 0) > 0:
                        break
                if session.poll() is not None:
                    raise RuntimeError("session-lost")
                time.sleep(.2)
            observed.append(window["id"])
            evidence["steps"].append({"requestedPackage": app,
                                      "observedWindow": window,
                                      "association": "sequential-fixture-observation-only"})
            previous = current
        evidence["twoWindowsObserved"] = len(set(observed)) == 2
        evidence["final"] = list(snapshot().values())
    except BaseException as error:
        evidence["error"] = {"stage": phase, "type": type(error).__name__,
                             "stdout": bounded_output(getattr(error, "stdout", None)),
                             "stderr": bounded_output(getattr(error, "stderr", None))}
        raise
    finally:
        Path("/home/proof/windows.json").write_text(json.dumps(evidence, sort_keys=True))
        print("QINDA_ANDROID_WINDOWS=" + json.dumps(evidence, sort_keys=True), flush=True)
        try: command("session", "stop")
        finally:
            if session.poll() is None:
                session.terminate()
            try: session.wait(timeout=3)
            except subprocess.TimeoutExpired:
                session.kill(); session.wait(timeout=2)
    return 0 if evidence["twoWindowsObserved"] else 1

if __name__ == "__main__": raise SystemExit(main())
