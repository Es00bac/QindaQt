#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""CTest entry for one live-customization row in a private nested QindaQt session.

Prepares hermetic HOME/XDG/D-Bus roots below --output-root, launches
qindaqt-wm on KWin's virtual backend with development input enabled through
the test scenario, runs live_customization_driver.py as the session program
(which starts the production shell on the proof profile and drives the
Meta+right-click chord through the private seat), and judges the evidence.
Exit 0 passes, 1 fails, 77 skips.

AGENT-GUARD: never point this at the live desktop. Every socket, bus, and
runtime directory lives under --output-root; input comes only from the private
compositor's development device, never from host uinput.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE.parent / "shade_visibility"))
sys.path.insert(0, str(HERE))

from nested_session_scenario import (  # noqa: E402
    isolated_environment,
    load_virtual_spec,
    running_private_session_bus,
    write_virtual_output_config,
)
from run_shade_visibility import capability_free_kwin  # noqa: E402

SKIP_CODE = 77
MAXIMUM_BUS_SOCKET_PATH = 100
MINIMUM_VERDICTS = {"menu": 10, "editmode": 6}


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--launcher", type=Path, required=True)
    parser.add_argument("--plugin-root", type=Path, required=True)
    parser.add_argument("--kwin", type=Path, required=True)
    parser.add_argument("--shell", type=Path, required=True)
    parser.add_argument("--parity-tool", type=Path, required=True)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--flow", choices=sorted(MINIMUM_VERDICTS), required=True)
    parser.add_argument("--profile-dir", type=Path, required=True)
    parser.add_argument("--profile-id", required=True)
    parser.add_argument("--theme-dir", type=Path, required=True)
    parser.add_argument("--applet-dir", type=Path, required=True)
    parser.add_argument("--applet-policy", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--name")
    return parser.parse_args()


def evaluate(evidence: dict[str, Any], flow: str, plugin_root: Path) -> list[str]:
    """Reasons the evidence fails; empty means the row passes."""
    failures: list[str] = []
    if evidence.get("result") != "completed":
        failures.append(f"flow did not complete: {evidence.get('result')}")
    verdicts = evidence.get("verdicts", {})
    if len(verdicts) < MINIMUM_VERDICTS[flow]:
        failures.append(f"only {len(verdicts)} verdicts recorded, expected at least "
                        f"{MINIMUM_VERDICTS[flow]}")
    failures.extend(f"verdict failed: {name}" for name, value in sorted(verdicts.items())
                    if value is not True)
    compositor = str((plugin_root / "kwin" / "plugins" / "qindaqt_compositor.so").resolve())
    if compositor not in evidence.get("mappedQindaqtLibraries", []):
        failures.append(f"private compositor did not map {compositor}")
    if evidence.get("shellExit") not in (None, 0, -15):
        failures.append(f"shell exited with {evidence.get('shellExit')}")
    return failures


def fresh_runtime_root() -> Path:
    # Lane build roots are deep; the private bus socket must fit the Unix
    # limit, so the disposable session root lives in the temporary directory
    # (the shell-surface nested row's precedent). Evidence stays under
    # --output-root.
    root = Path(tempfile.mkdtemp(prefix="qindaqt-lc-"))
    if len(str(root / "runtime" / "bus")) > MAXIMUM_BUS_SOCKET_PATH:
        raise RuntimeError(f"private bus socket path under {root} exceeds the Unix limit")
    return root


def main() -> int:
    arguments = parse_arguments()
    dbus_daemon = shutil.which("dbus-daemon")
    if not dbus_daemon:
        print("live-customization rows require dbus-daemon", file=sys.stderr)
        return SKIP_CODE
    for path, description in ((arguments.shell, "qindaqt-shell"),
                              (arguments.parity_tool, "parity tool"),
                              (arguments.launcher, "qindaqt-wm")):
        if not path.is_file():
            print(f"{description} is unavailable: {path}", file=sys.stderr)
            return SKIP_CODE
    spec = load_virtual_spec(arguments.scenario)
    name = arguments.name or f"{arguments.flow}-{arguments.scenario.stem}"
    output = arguments.output_root / "evidence" / name
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    root = fresh_runtime_root()
    environment = isolated_environment(root)
    write_virtual_output_config(Path(environment["XDG_CONFIG_HOME"]), spec)
    environment.update({
        "LC_OUT": str(output), "LC_FLOW": arguments.flow,
        "LC_SHELL": str(arguments.shell), "LC_PARITY_TOOL": str(arguments.parity_tool),
        "LC_PROFILE_DIR": str(arguments.profile_dir), "LC_PROFILE_ID": arguments.profile_id,
        "LC_THEME_DIR": str(arguments.theme_dir), "LC_APPLET_DIR": str(arguments.applet_dir),
        "LC_APPLET_POLICY": str(arguments.applet_policy),
        "LC_PIXEL_WIDTH": str(spec.pixel_width), "LC_PIXEL_HEIGHT": str(spec.pixel_height),
        "LC_SCALE": f"{spec.scale:.12g}", "PYTHONDONTWRITEBYTECODE": "1"})
    command = [
        str(arguments.launcher), "--plugin-root", str(arguments.plugin_root),
        "--kwin", str(capability_free_kwin(arguments.kwin, arguments.output_root)),
        "--virtual", "--width", str(spec.logical_width), "--height", str(spec.logical_height),
        "--scale", f"{spec.scale:.12g}", "--output-count", str(spec.output_count),
        "--test-scenario", str(arguments.scenario), "--no-lockscreen", "--no-global-shortcuts",
        "--session", str(HERE / "live_customization_driver.py")]
    try:
        with running_private_session_bus(root, Path(dbus_daemon), environment):
            try:
                completed = subprocess.run(command, env=environment, text=True,
                                           capture_output=True, timeout=280, check=False)
            except subprocess.TimeoutExpired as error:
                (output / "session-stdout.log").write_text(
                    (error.stdout or b"").decode("utf-8", "replace") if isinstance(error.stdout, bytes)
                    else str(error.stdout or ""), encoding="utf-8")
                (output / "session-stderr.log").write_text(
                    (error.stderr or b"").decode("utf-8", "replace") if isinstance(error.stderr, bytes)
                    else str(error.stderr or ""), encoding="utf-8")
                print(f"nested live-customization session timed out; see {output}", file=sys.stderr)
                return 1
    finally:
        shutil.rmtree(root, ignore_errors=True)
    (output / "session-stdout.log").write_text(completed.stdout, encoding="utf-8")
    (output / "session-stderr.log").write_text(completed.stderr, encoding="utf-8")
    evidence_path = output / "evidence.json"
    if not evidence_path.exists():
        print(f"no evidence written (session exit {completed.returncode}); see {output}",
              file=sys.stderr)
        return 1
    evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
    failures = evaluate(evidence, arguments.flow, arguments.plugin_root)
    summary = {"row": name, "sessionExit": completed.returncode, "output": str(output),
               "verdictCount": len(evidence.get("verdicts", {})),
               "passedVerdicts": sum(1 for v in evidence.get("verdicts", {}).values() if v is True),
               "captures": len(evidence.get("captures", [])),
               "failures": failures}
    print("QINDAQT_LIVE_CUSTOMIZATION=" + json.dumps(summary, sort_keys=True))
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
