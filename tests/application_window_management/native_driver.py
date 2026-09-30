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
        child=subprocess.Popen([os.environ["APP_PLACEMENT_PROBE"]],stdout=log,stderr=subprocess.STDOUT)
        deadline=time.monotonic()+25
        evidence={"snapshots":[],"mappedLibraries":sorted({line.split()[-1] for line in Path(f"/proc/{os.getppid()}/maps").read_text().splitlines() if "qindaqt_compositor.so" in line})}
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
