#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run native application placement in an isolated bus and virtual compositor."""
import argparse,json,os,shutil,subprocess,sys,tempfile
from pathlib import Path
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parents[0]/"session"))
from nested_session_scenario import isolated_environment,load_virtual_spec,running_private_session_bus,write_virtual_output_config

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ("launcher","plugin-root","kwin","probe","scenario","output"):
        parser.add_argument("--"+name,type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    spec=load_virtual_spec(args.scenario)
    with tempfile.TemporaryDirectory(prefix="qinda-app-",dir="/tmp") as private:
        root=Path(private);env=isolated_environment(root)
        env.update(APP_PLACEMENT_OUTPUT=str(args.output.resolve()),APP_PLACEMENT_PROBE=str(args.probe.resolve()),PYTHONDONTWRITEBYTECODE="1",APP_PLACEMENT_EXPECTED_SCALE=str(spec.scale))
        # Preserve the fork basename: it participates in Qt plugin discovery.
        runtime=args.output/"kwin-nocap"/args.kwin.name;runtime.parent.mkdir(exist_ok=True)
        shutil.copyfile(args.kwin,runtime);runtime.chmod(0o755)
        write_virtual_output_config(Path(env["XDG_CONFIG_HOME"]),spec)
        command=[str(args.launcher),"--plugin-root",str(args.plugin_root),"--kwin",str(runtime.resolve()),"--virtual","--width",str(spec.logical_width),"--height",str(spec.logical_height),"--scale",str(spec.scale),"--output-count",str(spec.output_count),"--test-scenario",str(args.scenario),"--no-lockscreen","--no-global-shortcuts","--session",str(HERE/"native_driver.py")]
        with running_private_session_bus(root,Path(shutil.which("dbus-daemon")),env):
            result=subprocess.run(command,env=env,capture_output=True,text=True,timeout=45)
        (args.output/"session.log").write_text(result.stdout+result.stderr)
    evidence_file=args.output/"evidence.json"
    if not evidence_file.exists():print("No private scene evidence; inspect session.log",file=sys.stderr);return 1
    evidence=json.loads(evidence_file.read_text());log=(args.output/"probe.log").read_text()
    mapped=str((args.plugin_root/"qindaqt-kwin"/"plugins"/"qindaqt_compositor.so").resolve())
    # Read-only scene evidence verifies membership, active tab and tiled frames;
    # completion events alone would miss a transport that reported false success.
    last=evidence.get("completionSnapshot")
    if last is None:
        print("No live post-completion scene evidence", file=sys.stderr)
        return 1
    windows=last["windows"]
    members=[windows.get("Native placement probe "+str(i),{}) for i in (1,2,3)]
    owners={value.get("containerId") for value in members}
    left,right=members[1].get("geometry",{}),members[2].get("geometry",{})
    same_group=len(owners)==1 and bool(next(iter(owners))) and last["hybrid"].get("containerCount")==1
    active_page=members[0].get("minimized") is True and members[1].get("minimized") is False and members[2].get("minimized") is False
    split=bool(left) and bool(right) and left["x"]+left["width"]<=right["x"] and left["y"]==right["y"] and left["height"]==right["height"]
    foreground=any(value.get("active") is True and not value.get("containerId") for value in windows.values())
    scales=last.get("outputs",{}).get("outputs",[])
    actual_scale=bool(scales) and all(abs(row.get("scale",0)-spec.scale)<.01 for row in scales) and f"ACTUAL_PLACEMENT_DPR={spec.scale:.2f}" in log
    passed=actual_scale and foreground and same_group and active_page and split and evidence["probeExit"]==0 and "NATIVE_PLACEMENT_COMPLETED" in log and mapped in evidence["mappedLibraries"]
    print(json.dumps({"probeExit":evidence["probeExit"],"sessionExit":result.returncode,"snapshotCount":len(evidence["snapshots"]),"sameGroup":same_group,"actualScale":actual_scale,"activePage":active_page,"splitFrames":split,"foregroundPreserved":foreground,"passed":passed,"output":str(args.output)},sort_keys=True))
    return 0 if passed else 1
if __name__=="__main__":sys.exit(main())
