#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Runs only as a private compositor's exit-with-session child."""
import json,os,subprocess,sys,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"session"/"shade_visibility"))
from shade_control import connect

def main():
    output=Path(os.environ["APP_PLACEMENT_OUTPUT"])
    control=connect()
    with (output/"probe.log").open("w") as log:
        # Only the SDK client uses fatal Qt warnings. The non-capability
        # compositor fixture emits upstream's expected real-time-priority warning.
        probe_env = os.environ.copy()
        probe_env["QT_FATAL_WARNINGS"] = "1"
        probe_env.pop("QT_DEBUG_PLUGINS", None)
        child=subprocess.Popen([os.environ["APP_PLACEMENT_PROBE"]],stdout=log,stderr=subprocess.STDOUT,env=probe_env)
        deadline=time.monotonic()+25
        # The production compositor intentionally disables dumpability.
        # The outer runner verifies its successful Qt library-load trace;
        # this ordinary client must not require access to protected process maps.
        evidence={"snapshots":[],"compositorPid":os.getppid()}
        try:
            while child.poll() is None and time.monotonic()<deadline:
                snapshot = {"hybrid": control.hybrid(), "windows": control.windows(), "outputs": control.call("Outputs")}
                evidence["snapshots"].append(snapshot)
                # AGENT-GUARD: Qt destroys surfaces before process exit. A
                # trailing shutdown snapshot is not the committed result of
                # the requests. Retain post-completion live scene evidence.
                expected = {"Native placement probe " + str(i) for i in (1, 2, 3, 6)}
                if ("NATIVE_PLACEMENT_COMPLETED" in (output / "probe.log").read_text()
                        and expected.issubset(snapshot["windows"])):
                    evidence["completionSnapshot"] = snapshot
                time.sleep(.15)
            if child.poll() is None: child.kill()
            evidence["probeExit"]=child.wait(timeout=3)
        finally:
            if child.poll() is None: child.kill();child.wait(timeout=3)
        (output/"evidence.json").write_text(json.dumps(evidence,indent=2))
    return evidence["probeExit"]
if __name__=="__main__":sys.exit(main())
