#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""CTest entry for one iconified-window row in a private nested QindaQt session.

Same hermetic shape as the shade-visibility runner: HOME/XDG/D-Bus roots below
--output-root, qindaqt-wm on KWin's virtual backend with development input
enabled through the test scenario, iconify_session_driver.py as the session
program, and a verdict judgement of the evidence it writes. Exit 0 passes,
1 fails, 77 skips (dbus-daemon is unavailable).

AGENT-GUARD: never point this at the live desktop. Every socket, bus, and
runtime directory lives under --output-root; input comes only from the private
compositor's development device, never from host uinput.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any

HERE = Path(__file__).resolve().parent
SHADE = HERE.parent / "shade_visibility"
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(SHADE))
sys.path.insert(0, str(HERE))

from nested_session_scenario import (  # noqa: E402
    isolated_environment,
    load_virtual_spec,
    running_private_session_bus,
    write_virtual_output_config,
)
from run_shade_visibility import capability_free_kwin, fresh_runtime_root  # noqa: E402

SKIP_CODE = 77
MINIMUM_VERDICTS = {"basic": 24, "dock": 10}


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--launcher", type=Path, required=True)
    parser.add_argument("--plugin-root", type=Path, required=True)
    parser.add_argument("--kwin", type=Path, required=True)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--flow", choices=sorted(MINIMUM_VERDICTS), required=True)
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
    return failures


def runtime_root(output_root: Path) -> Path:
    """A private runtime root whose bus socket path fits the Unix limit.

    AGENT-NOTE: a lane build root with a long name (this wave's
    `.cache/small-team/build/f1-iconified-windows`) pushes
    `<output-root>/sv/<n>/runtime/bus` past 100 bytes, so the session's private
    bus could never be created. Evidence stays under --output-root; only the
    sockets move to a short directory under the user's runtime dir.
    """
    try:
        return fresh_runtime_root(output_root)
    except RuntimeError:
        short = Path(os.environ.get("XDG_RUNTIME_DIR") or "/tmp") / "qindaqt-iconify"
        short.mkdir(parents=True, exist_ok=True)
        return fresh_runtime_root(short)


def main() -> int:
    arguments = parse_arguments()
    dbus_daemon = shutil.which("dbus-daemon")
    if not dbus_daemon:
        print("iconify-visibility rows require dbus-daemon", file=sys.stderr)
        return SKIP_CODE
    spec = load_virtual_spec(arguments.scenario)
    name = arguments.name or f"iconify-{arguments.flow}-{arguments.scenario.stem}"
    output = arguments.output_root / "evidence" / name
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    root = runtime_root(arguments.output_root)
    environment = isolated_environment(root)
    write_virtual_output_config(Path(environment["XDG_CONFIG_HOME"]), spec)
    # The shade session helpers read SHADE_* names; the values are ours.
    environment.update({
        "SHADE_OUT": str(output), "SHADE_KIND": "gtk-ssd", "SHADE_FLOW": arguments.flow,
        "SHADE_PIXEL_WIDTH": str(spec.pixel_width), "SHADE_PIXEL_HEIGHT": str(spec.pixel_height),
        "SHADE_SCALE": f"{spec.scale:.12g}", "PYTHONDONTWRITEBYTECODE": "1"})
    driver = HERE / "iconify_session_driver.py"
    # The launcher execs the session driver; a driver that lost its
    # executable bit would leave the compositor idle until the timeout.
    if not os.access(driver, os.X_OK):
        print(f"session driver is not executable: {driver}", file=sys.stderr)
        return 1
    command = [
        str(arguments.launcher), "--plugin-root", str(arguments.plugin_root),
        "--kwin", str(capability_free_kwin(arguments.kwin, arguments.output_root)),
        "--virtual", "--width", str(spec.logical_width), "--height", str(spec.logical_height),
        "--scale", f"{spec.scale:.12g}", "--output-count", str(spec.output_count),
        "--test-scenario", str(arguments.scenario), "--no-lockscreen", "--no-global-shortcuts",
        "--session", str(driver)]
    with running_private_session_bus(root, Path(dbus_daemon), environment):
        try:
            completed = subprocess.run(command, env=environment, text=True, capture_output=True,
                                       timeout=420, check=False)
        except subprocess.TimeoutExpired as expired:
            # Keep whatever the private compositor said; a traceback hides it.
            (output / "session-stdout.log").write_bytes(expired.stdout or b"")
            (output / "session-stderr.log").write_bytes(expired.stderr or b"")
            print(f"private session timed out after 420 s; see {output}", file=sys.stderr)
            return 1
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
               "failures": failures}
    print("QINDAQT_ICONIFY_VISIBILITY=" + json.dumps(summary, sort_keys=True))
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
