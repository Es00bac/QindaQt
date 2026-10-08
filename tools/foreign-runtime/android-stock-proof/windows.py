#!/usr/bin/python3
"""Guest-only sequential launch observation, never per-app authority."""
import json
import os
from pathlib import Path
import subprocess
import time

APPS = ("com.android.calculator2", "com.android.deskclock")

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
    def command(*args):
        return subprocess.run(["/usr/bin/waydroid", *args], stdin=subprocess.DEVNULL,
                              capture_output=True, text=True, check=True,
                              timeout=min(10, remaining())).stdout.strip()
    session = subprocess.Popen(["/usr/bin/waydroid", "session", "start"],
                               stdin=subprocess.DEVNULL)
    evidence = {"schema": 1, "twoWindowsObserved": False, "steps": [],
                "identityAuthority": False, "renderedInputQualified": False,
                "resizeCloseQualified": False}
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
        while command("prop", "get", "sys.boot_completed") != "1":
            if session.poll() is not None:
                raise RuntimeError("session-ended-before-readiness")
            time.sleep(.5)
        command("prop", "set", "persist.waydroid.multi_windows", "true")
        if command("prop", "get", "persist.waydroid.multi_windows") != "true":
            raise RuntimeError("multiwindow-readback")
        observed = []
        previous = before
        for app in APPS:
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
    finally:
        Path("/home/proof/windows.json").write_text(json.dumps(evidence, sort_keys=True))
        try: command("session", "stop")
        finally:
            if session.poll() is None:
                session.terminate()
            try: session.wait(timeout=3)
            except subprocess.TimeoutExpired:
                session.kill(); session.wait(timeout=2)
    return 0 if evidence["twoWindowsObserved"] else 1

if __name__ == "__main__": raise SystemExit(main())
