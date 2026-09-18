#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Outer runner for the touch-chrome rows (ADR-0193).

The shade harness with fingers: the same private virtual session, private bus,
capability-free kwin copy and framebuffer captures, driven by
touch_session_driver.py, which injects touch contacts through the
development input device instead of a pointer.
"""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "shade_visibility"))

import run_shade_visibility as shade  # noqa: E402

MINIMUM_VERDICTS = {"chrome": 7, "enabled": 7}


def main() -> int:
    arguments = _parse_arguments()
    missing = shade.missing_executable(arguments.kind)
    if missing:
        print(f"touch-chrome {arguments.kind}: fixture executable {missing} is not installed "
              "(coverage missing, not claimed)", file=sys.stderr)
        return shade.SKIP_CODE
    dbus_daemon = shutil.which("dbus-daemon")
    if not dbus_daemon:
        print("touch-chrome rows require dbus-daemon", file=sys.stderr)
        return shade.SKIP_CODE
    spec = shade.load_virtual_spec(arguments.scenario)
    name = arguments.name or f"touch-{arguments.flow}-{arguments.kind}-{arguments.scenario.stem}"
    output = arguments.output_root / "evidence" / name
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    root = shade.fresh_runtime_root(arguments.output_root)
    environment = shade.isolated_environment(root)
    shade.write_virtual_output_config(Path(environment["XDG_CONFIG_HOME"]), spec)
    environment.update({
        "SHADE_OUT": str(output), "SHADE_KIND": arguments.kind, "SHADE_FLOW": arguments.flow,
        "SHADE_PIXEL_WIDTH": str(spec.pixel_width), "SHADE_PIXEL_HEIGHT": str(spec.pixel_height),
        "SHADE_SCALE": f"{spec.scale:.12g}", "PYTHONDONTWRITEBYTECODE": "1",
        # The development seat claims touch only for rows that drive fingers.
        "QINDAQT_DEVELOPMENT_INPUT_TOUCH": "1"})
    driver = HERE / "touch_session_driver.py"
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
    if arguments.flow == "enabled" and not arguments.settings_service:
        print("the enabled flow needs --settings-service", file=sys.stderr)
        return 1
    with shade.running_private_session_bus(root, Path(dbus_daemon), environment):
        service = None
        if arguments.settings_service:
            service = _start_settings_service(arguments, environment, output)
        try:
            completed = subprocess.run(command, env=environment, text=True, capture_output=True,
                                       timeout=420, check=False)
        except subprocess.TimeoutExpired as expired:
            (output / "session-stdout.log").write_bytes(expired.stdout or b"")
            (output / "session-stderr.log").write_bytes(expired.stderr or b"")
            print(f"private session timed out after 420 s; see {output}", file=sys.stderr)
            return 1
        finally:
            if service is not None:
                service.terminate()
                try:
                    service.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    service.kill()
    (output / "session-stdout.log").write_text(completed.stdout, encoding="utf-8")
    (output / "session-stderr.log").write_text(completed.stderr, encoding="utf-8")
    evidence_path = output / "evidence.json"
    if not evidence_path.exists():
        print(f"no evidence written (session exit {completed.returncode}); see {output}",
              file=sys.stderr)
        return 1
    evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
    failures = _evaluate(evidence, arguments.flow, arguments.plugin_root)
    summary = {"row": name, "sessionExit": completed.returncode, "output": str(output),
               "verdictCount": len(evidence.get("verdicts", {})),
               "passedVerdicts": sum(1 for v in evidence.get("verdicts", {}).values() if v is True),
               "failures": failures}
    print("QINDAQT_TOUCH_CHROME=" + json.dumps(summary, sort_keys=True))
    return 1 if failures else 0


def _parse_arguments():
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--launcher", type=Path, required=True)
    parser.add_argument("--plugin-root", type=Path, required=True)
    parser.add_argument("--kwin", type=Path, required=True)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--kind", choices=sorted(shade.KINDS), required=True)
    parser.add_argument("--flow", choices=sorted(MINIMUM_VERDICTS), required=True)
    # The "enabled" flow drives the preference through a real Settings1 on the
    # private bus; the service binary comes from the build tree.
    parser.add_argument("--settings-service", type=Path)
    parser.add_argument("--settings-schema-dir", type=Path)
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


def _start_settings_service(arguments, environment: dict[str, str], output: Path) -> subprocess.Popen:
    """Run the real resident Settings1 on the private bus, isolated by XDG roots.

    The service keeps its user overrides under the private XDG_CONFIG_HOME and
    reads the schema from --settings-schema-dir (the source tree's
    data/settings), so nothing touches the live desktop's settings.
    """
    service_environment = dict(environment)
    if arguments.settings_schema_dir:
        service_environment["QINDAQT_SETTINGS_SCHEMA_DIR"] = str(arguments.settings_schema_dir)
    log = (output / "settings-service.log").open("w", encoding="utf-8")
    process = subprocess.Popen([str(arguments.settings_service)], env=service_environment,
                               stdout=log, stderr=subprocess.STDOUT)
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"settings service exited early ({process.returncode}); see {output}")
        probe = subprocess.run(
            ["dbus-send", "--session", "--dest=org.freedesktop.DBus", "--print-reply",
             "/org/freedesktop/DBus", "org.freedesktop.DBus.NameHasOwner",
             "string:org.qindaqt.Settings1"], env=environment, capture_output=True, text=True,
            check=False)
        if "boolean true" in probe.stdout:
            return process
        time.sleep(0.1)
    process.terminate()
    raise RuntimeError("settings service did not own org.qindaqt.Settings1 within 10 s")


if __name__ == "__main__":
    raise SystemExit(main())
