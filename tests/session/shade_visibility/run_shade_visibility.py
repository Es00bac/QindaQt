#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""CTest entry for one shade-visibility row in a private nested QindaQt session.

Prepares hermetic HOME/XDG/D-Bus roots below --output-root, launches
qindaqt-wm on KWin's virtual backend with development input enabled through
the test scenario, runs shade_session_driver.py as the session program, and
judges the evidence it writes. Exit 0 passes, 1 fails, 77 skips (a required
real-client fixture is not installed).

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
from pathlib import Path
from typing import Any

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))

from nested_session_scenario import (  # noqa: E402
    isolated_environment,
    load_virtual_spec,
    running_private_session_bus,
    write_virtual_output_config,
)
from shade_fixtures import KINDS, missing_executable  # noqa: E402

SKIP_CODE = 77
# Unix socket paths are limited to 108 bytes including the terminator.
MAXIMUM_BUS_SOCKET_PATH = 100
MINIMUM_VERDICTS = {"cycles": 15, "lifecycle": 13}


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--launcher", type=Path, required=True)
    parser.add_argument("--plugin-root", type=Path, required=True)
    parser.add_argument("--kwin", type=Path, required=True)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--kind", choices=sorted(KINDS), required=True)
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


def capability_free_kwin(source: Path, output_root: Path) -> Path:
    """A byte copy of kwin_wayland without file capabilities.

    The host binary carries cap_sys_nice, which makes KWin non-dumpable and
    its framebuffer memfds unreadable to the session driver. Copying drops the
    security.capability xattr; the program bytes are unchanged.
    """
    target = output_root / "kwin-nocap" / "kwin_wayland"
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists() or target.read_bytes() != source.read_bytes():
        shutil.copyfile(source, target)
        target.chmod(0o755)
    return target


def fresh_runtime_root(output_root: Path) -> Path:
    base = output_root / "sv"
    base.mkdir(parents=True, exist_ok=True)
    index = 0
    while (base / str(index)).exists():
        index += 1
    root = base / str(index)
    root.mkdir()
    if len(str(root / "runtime" / "bus")) > MAXIMUM_BUS_SOCKET_PATH:
        raise RuntimeError(f"private bus socket path under {root} exceeds the Unix limit")
    return root


def main() -> int:
    arguments = parse_arguments()
    missing = missing_executable(arguments.kind)
    if missing:
        print(f"shade-visibility {arguments.kind}: fixture executable {missing} is not installed "
              "(coverage missing, not claimed)", file=sys.stderr)
        return SKIP_CODE
    dbus_daemon = shutil.which("dbus-daemon")
    if not dbus_daemon:
        print("shade-visibility rows require dbus-daemon", file=sys.stderr)
        return SKIP_CODE
    spec = load_virtual_spec(arguments.scenario)
    name = arguments.name or f"{arguments.flow}-{arguments.kind}-{arguments.scenario.stem}"
    output = arguments.output_root / "evidence" / name
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    root = fresh_runtime_root(arguments.output_root)
    environment = isolated_environment(root)
    write_virtual_output_config(Path(environment["XDG_CONFIG_HOME"]), spec)
    environment.update({
        "SHADE_OUT": str(output), "SHADE_KIND": arguments.kind, "SHADE_FLOW": arguments.flow,
        "SHADE_PIXEL_WIDTH": str(spec.pixel_width), "SHADE_PIXEL_HEIGHT": str(spec.pixel_height),
        "SHADE_SCALE": f"{spec.scale:.12g}", "PYTHONDONTWRITEBYTECODE": "1"})
    command = [
        str(arguments.launcher), "--plugin-root", str(arguments.plugin_root),
        "--kwin", str(capability_free_kwin(arguments.kwin, arguments.output_root)),
        "--virtual", "--width", str(spec.logical_width), "--height", str(spec.logical_height),
        "--scale", f"{spec.scale:.12g}", "--output-count", str(spec.output_count),
        "--test-scenario", str(arguments.scenario), "--no-lockscreen", "--no-global-shortcuts",
        "--session", str(HERE / "shade_session_driver.py")]
    with running_private_session_bus(root, Path(dbus_daemon), environment):
        completed = subprocess.run(command, env=environment, text=True, capture_output=True,
                                   timeout=420, check=False)
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
    print("QINDAQT_SHADE_VISIBILITY=" + json.dumps(summary, sort_keys=True))
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
