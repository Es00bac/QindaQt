# SPDX-License-Identifier: GPL-3.0-or-later
"""Executable GlobalShortcuts registration/routing chain for Gabbee under QindaQt routing.

Assembles, entirely inside a private run root:

  private dbus-daemon -> staged portals.conf (GlobalShortcuts=kde) ->
  real xdg-desktop-portal frontend -> fake KDE backend (claims the reviewed
  kde selector's bus name) -> Gabbee's real PortalPushToTalkBinding client.

Then asserts: (1) Gabbee reports both shortcuts registered through the real
frontend, (2) the frontend forwards the backend's Activated/Deactivated
signals so Gabbee's pressed/released callbacks fire, and (3) the backend
journal records the CreateSession/BindShortcuts calls Gabbee made.

AGENT-CONTRACT: host-safe by construction — no display, no microphone, no
input injection, and every D-Bus/XDG endpoint lives under --run-root.  The
production qindaqt-portals.conf is only read; the routing copy is staged into
the private run root, mirroring accepted portal candidate 8215a8cd.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from gabbee_probe_support import (  # noqa: E402
    FAKE_CONTROL_BUS_NAME,
    FRONTEND_BUS_NAME,
    PhaseResult,
    ResultDocument,
    gabbee_probe_python_environment,
    gabbee_source_root,
    gabbee_venv_python,
    stage_portals_configuration,
    write_fake_backend_service,
    write_private_bus_config,
)

CONTROL_PATH = "/org/qindaqt/test/gabbee_portal_fake"
FRONTEND_CANDIDATES = (
    "/usr/libexec/xdg-desktop-portal",
    "/usr/lib64/libexec/xdg-desktop-portal",
    "/usr/lib/xdg-desktop-portal",
)


def find_frontal_binary(explicit: str | None) -> Path:
    if explicit:
        candidate = Path(explicit)
        if not candidate.is_file():
            raise SystemExit(f"portal frontend override is missing: {candidate}")
        return candidate
    for candidate in FRONTEND_CANDIDATES:
        if Path(candidate).is_file():
            return Path(candidate)
    found = shutil.which("xdg-desktop-portal")
    if found:
        return Path(found)
    raise SystemExit("xdg-desktop-portal frontend is not installed on this host")


def wait_for_bus_name(bus_address: str, name: str, timeout: float) -> bool:
    import dbus  # deferred: allows unit runs without a live bus

    bus = dbus.bus.BusConnection(bus_address)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if bus.name_has_owner(name):
            return True
        time.sleep(0.2)
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run-root", type=Path, required=True)
    parser.add_argument("--result-path", type=Path, default=None)
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument("--gabbee-root", default=None)
    parser.add_argument("--gabbee-venv-python", default=None)
    parser.add_argument("--portal-frontend", default=None)
    parser.add_argument("--keep-run-root", action="store_true")
    parser.add_argument(
        "--existing-bus-address",
        default=None,
        help="reuse a caller-owned private session bus (in-sandbox mode); the "
        "caller's XDG directories are inherited and no daemon is spawned",
    )
    arguments = parser.parse_args()
    gabbee_root = gabbee_source_root(arguments.gabbee_root)
    venv_python = gabbee_venv_python(arguments.gabbee_venv_python)
    frontend = find_frontal_binary(arguments.portal_frontend)

    bus = _prepare_bus_layout(arguments, venv_python)
    environments = _prepare_environments(arguments, gabbee_root, venv_python, bus)

    processes: list[subprocess.Popen] = []
    document = ResultDocument(run_id=bus["run_root"].name, mode="portal-chain")
    try:
        processes = _spawn_chain_processes(frontend, venv_python, environments, bus)
        preflight = PhaseResult(
            "portal-chain-preflight",
            _wait_for_chain_names(bus["address"]),
            "fake backend and portal frontend own their private-bus names",
            {
                "fakeBackendBusName": FAKE_CONTROL_BUS_NAME,
                "frontendBusName": FRONTEND_BUS_NAME,
                "conf": environments["conf"],
            },
        )
        document.add(preflight)
        if not preflight.ok:
            print(json.dumps(document.document, indent=2, sort_keys=True))
            return 1
        document.add(_registration_phase(venv_python, environments, bus["run_root"]))
        document.add(_journal_phase(bus["address"]))
    finally:
        for process in reversed(processes):
            process.terminate()
        for process in processes:
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
        if not arguments.keep_run_root:
            shutil.rmtree(bus["run_root"], ignore_errors=True)

    if arguments.result_path:
        document.write(arguments.result_path)
    print(json.dumps(document.document, indent=2, sort_keys=True))
    return 0 if document.outcome() else 1


def _prepare_bus_layout(arguments: argparse.Namespace, venv_python: Path) -> dict:
    """Create the private run root/bus (or adopt the caller's bus)."""

    run_root = arguments.run_root.resolve()
    if run_root.exists() and any(run_root.iterdir()):
        raise SystemExit(f"run root is not empty: {run_root}")
    if arguments.existing_bus_address:
        run_root.mkdir(parents=True, exist_ok=True)
        return {
            "run_root": run_root,
            "address": arguments.existing_bus_address,
            "config": None,
            "socket": None,
        }
    dbus_daemon = shutil.which("dbus-daemon")
    if not dbus_daemon:
        raise SystemExit("dbus-daemon is not available")
    # AGENT-NOTE: unix socket paths are capped near 104 bytes; deep worktree
    # paths exceed that, so reject them with the exact reason up front.
    if len(str(run_root / "runtime/bus")) > 100:
        raise SystemExit(
            f"run root is too deep for a unix bus socket: {run_root} — use a short path such as /tmp"
        )
    for child in ("runtime", "config", "data", "home"):
        (run_root / child).mkdir(parents=True, exist_ok=True)
    layout = write_private_bus_config(run_root)
    write_fake_backend_service(
        layout["serviceDir"],
        venv_python=venv_python,
        fake_script=Path(__file__).resolve().parent / "gabbee_portal_fake.py",
    )
    return {
        "run_root": run_root,
        "address": str(layout["address"]),
        "config": layout["config"],
        "socket": run_root / "runtime" / "bus",
    }


def _prepare_environments(
    arguments: argparse.Namespace, gabbee_root: Path, venv_python: Path, bus: dict
) -> dict:
    production_conf = (
        arguments.source_root / "src/services/portal/data/qindaqt-portals.conf"
    )
    if not production_conf.is_file():
        raise SystemExit(f"production portal selector conf is missing: {production_conf}")
    conf_destination = (
        Path(os.environ["XDG_CONFIG_HOME"]) / "xdg-desktop-portal/portals.conf"
        if arguments.existing_bus_address
        else bus["run_root"] / "config/xdg-desktop-portal/portals.conf"
    )
    conf = stage_portals_configuration(production_conf, conf_destination)

    if arguments.existing_bus_address:
        # In-sandbox mode: inherit the caller's XDG roots; only pin the bus,
        # portal suppression, and a minimal PATH.
        common = {
            "DBUS_SESSION_BUS_ADDRESS": bus["address"],
            "XDG_CURRENT_DESKTOP": "QindaQt",
            "QT_NO_XDG_DESKTOP_PORTAL": "1",
            "GTK_USE_PORTAL": "0",
            "PATH": "/usr/bin:/bin",
        }
    else:
        common = {
            "DBUS_SESSION_BUS_ADDRESS": bus["address"],
            "XDG_RUNTIME_DIR": str(bus["run_root"] / "runtime"),
            "XDG_CONFIG_HOME": str(bus["run_root"] / "config"),
            "XDG_DATA_HOME": str(bus["run_root"] / "data"),
            "XDG_DATA_DIRS": f"{bus['run_root'] / 'data'}:/usr/local/share:/usr/share",
            "HOME": str(bus["run_root"] / "home"),
            "XDG_CURRENT_DESKTOP": "QindaQt",
            # AGENT-GUARD: keep Qt tooling away from any ambient portal stack;
            # this chain owns the only portal frontend on this bus.
            "QT_NO_XDG_DESKTOP_PORTAL": "1",
            "GTK_USE_PORTAL": "0",
            "PATH": "/usr/bin:/bin",
        }
    python = gabbee_probe_python_environment(
        source_root=gabbee_root,
        venv_python=venv_python,
        system_site_packages=Path("/usr/lib/python3.14/site-packages"),
    )
    return {"conf": conf, "common": common, "python": python}


def _spawn_chain_processes(
    frontend: Path, venv_python: Path, environments: dict, bus: dict
) -> list[subprocess.Popen]:
    processes: list[subprocess.Popen] = []
    if bus["config"] is not None:
        daemon = subprocess.Popen(
            [
                shutil.which("dbus-daemon") or "dbus-daemon",
                "--config-file",
                str(bus["config"]),
                "--nofork",
                "--nopidfile",
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            env=environments["common"],
        )
        processes.append(daemon)
        deadline = time.monotonic() + 10
        while not bus["socket"].exists() and time.monotonic() < deadline:
            time.sleep(0.05)
        if not bus["socket"].exists():
            raise SystemExit("private dbus-daemon never created its socket")

    # AGENT-GUARD: with an existing bus the fake must be spawned directly;
    # its D-Bus activation would otherwise run outside this process tree.
    fake = subprocess.Popen(
        [str(venv_python), str(Path(__file__).parent / "gabbee_portal_fake.py")],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        env={**os.environ, **environments["common"], **environments["python"]}
        if bus["config"] is None
        else {**environments["common"], **environments["python"]},
    )
    processes.append(fake)

    frontend_log = open(bus["run_root"] / "frontend.log", "w", encoding="utf-8")
    processes.append(
        subprocess.Popen(
            [str(frontend)],
            stdout=frontend_log,
            stderr=subprocess.STDOUT,
            env=environments["common"],
        )
    )
    return processes


def _wait_for_chain_names(bus_address: str) -> bool:
    sys.path.insert(0, "/usr/lib/python3.14/site-packages")
    return bool(
        wait_for_bus_name(bus_address, FAKE_CONTROL_BUS_NAME, 15)
        and wait_for_bus_name(bus_address, FRONTEND_BUS_NAME, 20)
    )


def _registration_phase(venv_python: Path, environments: dict, run_root: Path) -> PhaseResult:
    driver = subprocess.run(
        [str(venv_python), str(Path(__file__).parent / "gabbee_portal_driver.py")],
        capture_output=True,
        text=True,
        timeout=90,
        env={**environments["common"], **environments["python"]},
    )
    driver_output = {}
    for line in driver.stdout.splitlines():
        try:
            candidate = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(candidate, dict):
            driver_output = candidate
    registration_ok = bool(
        driver_output.get("registered")
        and driver_output.get("activatedRoutedPressed")
        and driver_output.get("deactivatedRoutedReleased")
    )
    return PhaseResult(
        "shortcut-registration-and-routing",
        registration_ok and driver.returncode == 0,
        "Gabbee portal client registered both shortcuts and routed Activated/Deactivated",
        {
            "driverReturnCode": driver.returncode,
            "driver": driver_output,
            "driverStderrTail": driver.stderr[-2000:],
            "frontendLogTail": (
                (run_root / "frontend.log").read_text(encoding="utf-8", errors="replace")[-2000:]
            ),
        },
    )


def _journal_phase(bus_address: str) -> PhaseResult:
    import dbus

    bus = dbus.bus.BusConnection(bus_address)
    control = bus.get_object(FAKE_CONTROL_BUS_NAME, CONTROL_PATH)
    journal = json.loads(str(control.GetJournal()))
    bind_calls = [
        entry for entry in journal["journal"] if entry["event"] == "BindShortcuts"
    ]
    create_calls = [
        entry for entry in journal["journal"] if entry["event"] == "CreateSession"
    ]
    emitted = [entry["signal"] for entry in journal["emitted"]]
    journal_ok = (
        bool(create_calls)
        and len(bind_calls) == 1
        and set(bind_calls[0]["shortcutIds"]) == {"push_to_talk", "command"}
        and emitted == ["Activated", "Deactivated"]
    )
    return PhaseResult(
        "backend-journal",
        journal_ok,
        "fake KDE backend observed Gabbee's session and bind calls",
        {"journal": journal},
    )


if __name__ == "__main__":
    raise SystemExit(main())
