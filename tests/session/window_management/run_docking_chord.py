#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""CTest entry for the windowManagement bridge's nested docking-chord row (ADR-0209).

Prepares hermetic HOME/XDG/D-Bus roots below --output-root, launches qindaqt-wm
on KWin's virtual backend with development input enabled, runs
docking_chord_driver.py as the session program, and judges its evidence. Exit
0 passes, 1 fails, 77 skips (the GTK fixture or dbus-daemon is missing).

AGENT-GUARD: never point this at the live desktop. Every config and HOME/XDG
root is this row's own.

The private bus socket and the runtime tree beneath it are the one exception
to "everything under --output-root": an AF_UNIX path is capped at 108 bytes,
so they live in a pid-named directory under the session's XDG_RUNTIME_DIR
(/tmp where there is none) and are removed when the row ends. Evidence still
lands under --output-root. See private_bus_root.
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
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE.parent / "shade_visibility"))

from nested_session_scenario import (  # noqa: E402
    isolated_environment,
    load_virtual_spec,
    running_private_session_bus,
    write_virtual_output_config,
)
from run_shade_visibility import capability_free_kwin  # noqa: E402
from shade_fixtures import missing_executable  # noqa: E402

SKIP_CODE = 77
MAXIMUM_BUS_SOCKET_PATH = 100
EXPECTED_VERDICTS = (
    "default-chord-docks", "old-chord-inert-after-rebind", "new-chord-docks-after-rebind",
    "disabled-ignores-meta-shift", "disabled-ignores-alt-shift", "click-to-focus-ignores-hover",
    "focus-follows-mouse-after-reconfigure", "focus-follows-mouse-back-again",
    "restored-chord-docks")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--launcher", type=Path, required=True)
    parser.add_argument("--plugin-root", type=Path, required=True)
    parser.add_argument("--kwin", type=Path, required=True)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--name")
    return parser.parse_args()


def evaluate(evidence: dict[str, Any], plugin_root: Path) -> list[str]:
    """Reasons the evidence fails; empty means the row passes."""
    failures: list[str] = []
    if evidence.get("result") != "completed":
        failures.append(f"flow did not complete: {evidence.get('result')}")
    verdicts = evidence.get("verdicts", {})
    failures.extend(f"verdict missing: {name}" for name in EXPECTED_VERDICTS if name not in verdicts)
    failures.extend(f"verdict failed: {name}" for name, value in sorted(verdicts.items())
                    if value is not True)
    compositor = str((plugin_root / "kwin" / "plugins" / "qindaqt_compositor.so").resolve())
    if compositor not in evidence.get("mappedQindaqtLibraries", []):
        failures.append(f"private compositor did not map {compositor}")
    return failures


def private_bus_root(output_root: Path) -> Path:
    """A short-path home for the nested session's private bus socket.

    AGENT-GUARD: an AF_UNIX path is capped at 108 bytes including the
    terminator, and this socket's directory used to be derived from the build
    root. Every row here then refused to start from any build tree with a
    longish path -- twelve `compositor.shade-visibility.*` rows at once -- for
    a reason that has nothing to do with what they test. The session's own
    XDG_RUNTIME_DIR is where a Wayland/D-Bus socket belongs and is short by
    construction; /tmp is the fallback for a session without one.

    Evidence and captures stay under `output_root`: only the socket needs to
    be short, and artifacts belong beside the build they came from.
    """
    session = os.environ.get("XDG_RUNTIME_DIR", "")
    base = Path(session) if session and len(session) <= 64 else Path("/tmp")
    return base / f"qindaqt-{output_root.name}-{os.getpid()}"


def fresh_runtime_root(output_root: Path) -> Path:
    base = private_bus_root(output_root) / "wm"
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
    missing = missing_executable("gtk-csd")
    if missing:
        print(f"docking-chord: fixture executable {missing} is not installed", file=sys.stderr)
        return SKIP_CODE
    dbus_daemon = shutil.which("dbus-daemon")
    if not dbus_daemon:
        print("docking-chord row requires dbus-daemon", file=sys.stderr)
        return SKIP_CODE
    spec = load_virtual_spec(arguments.scenario)
    name = arguments.name or f"docking-chord-{arguments.scenario.stem}"
    output = arguments.output_root / "evidence" / name
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    root = fresh_runtime_root(arguments.output_root)
    environment = isolated_environment(root)
    write_virtual_output_config(Path(environment["XDG_CONFIG_HOME"]), spec)
    environment.update({
        "SHADE_OUT": str(output), "SHADE_KIND": "gtk-csd", "SHADE_FLOW": "docking-chord",
        "SHADE_PIXEL_WIDTH": str(spec.pixel_width), "SHADE_PIXEL_HEIGHT": str(spec.pixel_height),
        "SHADE_SCALE": f"{spec.scale:.12g}", "PYTHONDONTWRITEBYTECODE": "1"})
    driver = HERE / "docking_chord_driver.py"
    if not driver.stat().st_mode & 0o111:
        # KWin execs the session program; a non-executable driver would idle
        # the row to its timeout with an empty stdout. Fail fast instead.
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
    failures = evaluate(evidence, arguments.plugin_root)
    summary = {"row": name, "sessionExit": completed.returncode, "output": str(output),
               "verdictCount": len(evidence.get("verdicts", {})),
               "passedVerdicts": sum(1 for v in evidence.get("verdicts", {}).values() if v is True),
               "failures": failures}
    print("QINDAQT_DOCKING_CHORD=" + json.dumps(summary, sort_keys=True))
    # The runtime tree is outside --output-root now, so this row owns taking
    # it away again rather than leaving one per run behind.
    shutil.rmtree(root.parent, ignore_errors=True)
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
