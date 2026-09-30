#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise semantic commands in a bounded private virtual compositor."""
import argparse,json,os,shutil,subprocess,sys,tempfile
from pathlib import Path
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parents[0]/"session"))
from nested_session_scenario import isolated_environment,load_virtual_spec,running_private_session_bus,write_virtual_output_config

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ("launcher","plugin-root","kwin","probe","scenario","output"):
        parser.add_argument("--"+name,type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True);spec=load_virtual_spec(args.scenario)
    with tempfile.TemporaryDirectory(prefix="qinda-command-",dir="/tmp") as private:
        root=Path(private);env=isolated_environment(root)
        # Copying drops CAP_SYS_NICE; the fork's help probe emits its expected
        # scheduling warning. Only the actual Qt command client is warning-fatal.
        env["QT_FATAL_WARNINGS"]="0"
        env.update(COMMAND_NATIVE_OUTPUT=str(args.output.resolve()),COMMAND_NATIVE_PROBE=str(args.probe.resolve()),COMMAND_EXPECTED_SCALE=str(spec.scale),PYTHONDONTWRITEBYTECODE="1")
        runtime=args.output/"kwin-nocap"/args.kwin.name;runtime.parent.mkdir(exist_ok=True)
        shutil.copyfile(args.kwin,runtime);runtime.chmod(0o755)
        write_virtual_output_config(Path(env["XDG_CONFIG_HOME"]),spec)
        command=[str(args.launcher),"--plugin-root",str(args.plugin_root),"--kwin",str(runtime.resolve()),"--virtual","--width",str(spec.logical_width),"--height",str(spec.logical_height),"--scale",str(spec.scale),"--output-count",str(spec.output_count),"--test-scenario",str(args.scenario),"--no-lockscreen","--no-global-shortcuts","--session",str(HERE/"native_driver.py")]
        with running_private_session_bus(root,Path(shutil.which("dbus-daemon")),env):
            result=subprocess.run(command,env=env,capture_output=True,text=True,timeout=80)
        (args.output/"session.log").write_text(result.stdout+result.stderr)
    evidence_path=args.output/"evidence.json"
    if not evidence_path.exists():print("No native evidence; inspect session.log",file=sys.stderr);return 1
    evidence=json.loads(evidence_path.read_text());probe=(args.output/"probe.log").read_text()
    mapped=str((args.plugin_root/"qindaqt-kwin"/"plugins"/"qindaqt_compositor.so").resolve())
    passed=evidence["probeExit"]==0 and result.returncode==0 and "NATIVE_COMMANDS_COMPLETED" in probe and f"ACTUAL_COMMAND_DPR={spec.scale:.2f}" in probe and mapped in evidence["mappedLibraries"]
    print(json.dumps({"passed":passed,"probeExit":evidence["probeExit"],"sessionExit":result.returncode,"snapshots":len(evidence["snapshots"]),"output":str(args.output)}))
    return 0 if passed else 1
if __name__=="__main__":sys.exit(main())
