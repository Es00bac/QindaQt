#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Runs under the private compositor's exit-with-session contract only."""
import json,os,subprocess,sys,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"session"/"shade_visibility"))
from shade_control import connect

def main():
    output=Path(os.environ["COMMAND_NATIVE_OUTPUT"]);control=connect()
    with (output/"probe.log").open("w") as log:
        child_env=dict(os.environ);child_env["QT_FATAL_WARNINGS"]="1"
        child=subprocess.Popen([os.environ["COMMAND_NATIVE_PROBE"]],env=child_env,stdout=log,stderr=subprocess.STDOUT)
        evidence={"snapshots":[],"mappedLibraries":sorted({line.split()[-1] for line in Path(f"/proc/{os.getppid()}/maps").read_text().splitlines() if "qindaqt_compositor.so" in line})}
        deadline=time.monotonic()+65
        try:
            while child.poll() is None and time.monotonic()<deadline:
                evidence["snapshots"].append({"hybrid":control.hybrid(),"windows":control.windows()})
                time.sleep(.2)
            if child.poll() is None:child.kill()
            evidence["probeExit"]=child.wait(timeout=3)
        finally:
            if child.poll() is None:child.kill();child.wait(timeout=3)
        (output/"evidence.json").write_text(json.dumps(evidence,indent=2))
    return evidence["probeExit"]
if __name__=="__main__":sys.exit(main())
