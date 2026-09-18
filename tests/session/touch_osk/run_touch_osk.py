#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Outer runner for the on-screen keyboard rows (ADR-0204).

The shade harness with fingers and the first-party keyboard: the private
virtual session's kwinrc names a desktop entry whose Exec is this build's
qindaqt-osk, so KWin launches it as its input method on its own
input-method connection. touch_osk_driver.py then taps a GTK entry with a
finger and judges the keyboard by KWin's own VirtualKeyboard interface, the
keyboard's evidence file, the entry's text, and framebuffer captures.
"""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "shade_visibility"))

import run_shade_visibility as shade  # noqa: E402

MINIMUM_VERDICTS = {"osk": 8}


def main() -> int:
    arguments = _parse_arguments()
    missing = shade.missing_executable("gtk-csd")
    if missing:
        print(f"touch-osk: fixture executable {missing} is not installed "
              "(coverage missing, not claimed)", file=sys.stderr)
        return shade.SKIP_CODE
    dbus_daemon = shutil.which("dbus-daemon")
    if not dbus_daemon:
        print("touch-osk rows require dbus-daemon", file=sys.stderr)
        return shade.SKIP_CODE
    if not arguments.osk.exists():
        print(f"touch-osk: qindaqt-osk is not built at {arguments.osk}", file=sys.stderr)
        return 1
    spec = shade.load_virtual_spec(arguments.scenario)
    name = arguments.name or f"touch-osk-{arguments.flow}-{arguments.scenario.stem}"
    output = arguments.output_root / "evidence" / name
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    root = shade.fresh_runtime_root(arguments.output_root)
    environment = shade.isolated_environment(root)
    shade.write_virtual_output_config(Path(environment["XDG_CONFIG_HOME"]), spec)
    osk_evidence = output / "osk.json"
    desktop_file = _write_desktop_entry(output, arguments.osk, osk_evidence)
    # GTK must use its Wayland text-input context, not a host IM module.
    environment.pop("GTK_IM_MODULE", None)
    environment.pop("QT_IM_MODULE", None)
    environment.update({
        "SHADE_OUT": str(output), "SHADE_KIND": "gtk-csd", "SHADE_FLOW": arguments.flow,
        "SHADE_PIXEL_WIDTH": str(spec.pixel_width), "SHADE_PIXEL_HEIGHT": str(spec.pixel_height),
        "SHADE_SCALE": f"{spec.scale:.12g}", "PYTHONDONTWRITEBYTECODE": "1",
        "QINDAQT_OSK_DESKTOP_FILE": str(desktop_file),
        "QINDAQT_OSK_EVIDENCE_FILE": str(osk_evidence),
        # The development seat claims touch only for rows that drive fingers.
        "QINDAQT_DEVELOPMENT_INPUT_TOUCH": "1"})
    driver = HERE / "touch_osk_driver.py"
    if not driver.stat().st_mode & 0o111:
        print(f"session driver is not executable: {driver}", file=sys.stderr)
        return 1
    command = [
        str(arguments.launcher), "--plugin-root", str(arguments.plugin_root),
        "--kwin", str(shade.capability_free_kwin(arguments.kwin, arguments.output_root)),
        "--virtual", "--width", str(spec.logical_width), "--height", str(spec.logical_height),
        "--scale", f"{spec.scale:.12g}", "--output-count", str(spec.output_count),
        "--test-scenario", str(arguments.scenario), "--no-lockscreen", "--no-global-shortcuts",
        "--session", str(driver)]
    with shade.running_private_session_bus(root, Path(dbus_daemon), environment):
        try:
            completed = subprocess.run(command, env=environment, text=True, capture_output=True,
                                       timeout=420, check=False)
        except subprocess.TimeoutExpired as expired:
            (output / "session-stdout.log").write_bytes(expired.stdout or b"")
            (output / "session-stderr.log").write_bytes(expired.stderr or b"")
            print(f"private session timed out after 420 s; see {output}", file=sys.stderr)
            return 1
    (output / "session-stdout.log").write_text(completed.stdout, encoding="utf-8")
    (output / "session-stderr.log").write_text(completed.stderr, encoding="utf-8")
    return _report(output, name, completed.returncode, arguments)


def _write_desktop_entry(output: Path, osk: Path, evidence: Path) -> Path:
    desktop_file = output / "org.qindaqt.OnScreenKeyboard.desktop"
    desktop_file.write_text(
        "[Desktop Entry]\nType=Application\nName=QindaQt On-Screen Keyboard (private session)\n"
        f"Exec={osk} --evidence {evidence}\nNoDisplay=true\nX-KDE-Wayland-VirtualKeyboard=true\n",
        encoding="utf-8")
    return desktop_file


def _report(output: Path, name: str, session_exit: int, arguments) -> int:
    evidence_path = output / "evidence.json"
    if not evidence_path.exists():
        print(f"no evidence written (session exit {session_exit}); see {output}", file=sys.stderr)
        return 1
    evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
    failures = _evaluate(evidence, arguments.flow, arguments.plugin_root)
    if not (output / "osk.json").exists():
        failures.append("the keyboard never wrote its evidence file (was it launched?)")
    summary = {"row": name, "sessionExit": session_exit, "output": str(output),
               "verdictCount": len(evidence.get("verdicts", {})),
               "passedVerdicts": sum(1 for v in evidence.get("verdicts", {}).values() if v is True),
               "failures": failures}
    print("QINDAQT_TOUCH_OSK=" + json.dumps(summary, sort_keys=True))
    return 1 if failures else 0


def _parse_arguments():
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--launcher", type=Path, required=True)
    parser.add_argument("--plugin-root", type=Path, required=True)
    parser.add_argument("--kwin", type=Path, required=True)
    parser.add_argument("--osk", type=Path, required=True)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--flow", choices=sorted(MINIMUM_VERDICTS), required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--name")
    return parser.parse_args()


def _evaluate(evidence, flow: str, plugin_root: Path) -> list[str]:
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


if __name__ == "__main__":
    raise SystemExit(main())
