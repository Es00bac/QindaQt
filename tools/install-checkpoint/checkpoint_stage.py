#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build and verify a staged install beneath the ignored build tree."""

from __future__ import annotations

import json
import os
from pathlib import Path
import subprocess
import sys
from typing import Any

from checkpoint_audit import (
    cmake_cache, desktop_entry, installed_package, makeopts, preflight_report,
    project_commit, sha256_file, source_settings_route_ids, source_versions,
    state_locations, unit_state, validate_build_paths, write_executable,
)
from checkpoint_contract import (
    CMAKE_OPTIONS, EXPECTED_BINARIES, INSTALL_PREFIX, ROLLBACK_SCRIPT, SERVICES,
)
from checkpoint_install import (
    audit_cmake_install_scripts, validate_session_entry, validate_staged_paths,
)
from checkpoint_rollback import render_rollback_script, render_snapshot_script


def run_logged(
    label: str,
    command: list[str],
    log_path: Path,
    environment: dict[str, str] | None = None,
) -> None:
    """Run a potentially noisy build step into the ignored review log."""
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open("w", encoding="utf-8") as log:
        result = subprocess.run(
            command,
            check=False,
            stdout=log,
            stderr=subprocess.STDOUT,
            text=True,
            env=environment,
        )
    if result.returncode != 0:
        tail = log_path.read_text(encoding="utf-8", errors="replace").splitlines()[-80:]
        raise RuntimeError(
            f"{label} failed with exit status {result.returncode}; log: {log_path}\n"
            + "\n".join(tail)
        )
    print(f"{label} succeeded; log: {log_path}", file=sys.stderr)


def staged_install_invocation(
    build_dir: Path,
    stage_root: Path,
    base_environment: dict[str, str] | None = None,
) -> tuple[list[str], dict[str, str]]:
    environment = dict(os.environ if base_environment is None else base_environment)
    environment["DESTDIR"] = str(stage_root.resolve())
    return ["cmake", "--install", str(build_dir)], environment


def build_then_install_with_final_audit(
    build_dir: Path,
    stage_root: Path,
    build_command: list[str],
    build_log: Path,
    install_log: Path,
) -> tuple[dict[str, Any], list[str], dict[str, str]]:
    """Re-audit generated install scripts after build regeneration, just before install."""
    run_logged("production build", build_command, build_log)
    cache = cmake_cache(build_dir)
    if cache.get("CMAKE_INSTALL_PREFIX") != str(INSTALL_PREFIX):
        raise RuntimeError(
            f"post-build CMake install prefix must remain {INSTALL_PREFIX}; "
            f"found {cache.get('CMAKE_INSTALL_PREFIX')}"
        )
    final_audit = audit_cmake_install_scripts(build_dir, stage_root, INSTALL_PREFIX)
    install, environment = staged_install_invocation(build_dir, stage_root)
    run_logged("staged install", install, install_log, environment)
    return final_audit, install, environment


def stage(repo: Path, build_dir: Path, stage_root: Path) -> dict[str, Any]:
    validate_build_paths(repo, build_dir, stage_root)
    if stage_root.exists():
        raise ValueError(f"stage root already exists; preserve or remove it explicitly before staging: {stage_root}")
    make = makeopts()
    if not make["available"]:
        raise ValueError("portageq envvar MAKEOPTS is unavailable; refusing a guessed direct-build limit")
    build_dir.parent.mkdir(parents=True, exist_ok=True)
    log_dir = build_dir.parent / "logs"
    configure = ["cmake", "-S", str(repo), "-B", str(build_dir), *CMAKE_OPTIONS]
    build = ["cmake", "--build", str(build_dir), "--", *make["tokens"]]
    run_logged("configure", configure, log_dir / "configure.log")
    cache = cmake_cache(build_dir)
    if cache.get("CMAKE_INSTALL_PREFIX") != str(INSTALL_PREFIX):
        raise RuntimeError(
            f"configured CMake install prefix must remain {INSTALL_PREFIX}; found {cache.get('CMAKE_INSTALL_PREFIX')}"
        )
    pre_build_install_audit = audit_cmake_install_scripts(build_dir, stage_root, INSTALL_PREFIX)
    # AGENT-CONTRACT: CMake keeps the package's /usr install prefix, while
    # DESTDIR roots every generated absolute destination below the ignored
    # stage tree. Keep the prebuild audit and repeat it after build regeneration
    # immediately before cmake --install uses the generated scripts.
    install_destination_audit, install, install_environment = build_then_install_with_final_audit(
        build_dir,
        stage_root,
        build,
        log_dir / "build.log",
        log_dir / "install.log",
    )
    staged_entries = validate_staged_paths(stage_root, list(stage_root.rglob("*")))
    install_manifest_path = build_dir / "install_manifest.txt"
    if not install_manifest_path.is_file():
        raise RuntimeError(f"CMake did not write the install manifest: {install_manifest_path}")
    installed_manifest_paths = [
        Path(line)
        for line in install_manifest_path.read_text(encoding="utf-8", errors="replace").splitlines()
        if line.strip()
    ]
    validate_staged_paths(stage_root, installed_manifest_paths)
    install_root = stage_root / "usr"
    expected = [install_root / "bin" / name for name in EXPECTED_BINARIES]
    missing = [str(path) for path in expected if not path.is_file() or not os.access(path, os.X_OK)]
    audio_units = list(stage_root.rglob(SERVICES["Audio1"]["unit"]))
    network_units = list(stage_root.rglob(SERVICES["Network1"]["unit"]))
    settings_desktops = list(stage_root.rglob("org.qindaqt.Settings.desktop"))
    session_entries = list(stage_root.rglob("qindaqt.desktop"))
    dbus_units = [*stage_root.rglob("org.qindaqt.Audio1.service"), *stage_root.rglob("org.qindaqt.Network1.service"), *stage_root.rglob("org.qindaqt.Settings1.service")]
    if missing or not audio_units or not network_units or not settings_desktops or len(session_entries) != 1:
        raise RuntimeError(f"staged install is incomplete; missing executables={missing}; unit/desktop counts={[len(audio_units), len(network_units), len(settings_desktops), len(session_entries)]}")
    audio_text = audio_units[0].read_text()
    network_text = network_units[0].read_text()
    if "BusName=org.qindaqt.Audio1" not in audio_text or "ExecStart=/usr/bin/qindaqt-audio-service" not in audio_text:
        raise RuntimeError("staged Audio1 user unit does not target the production binary and bus name")
    if "BusName=org.qindaqt.Network1" not in network_text or "ExecStart=/usr/bin/qindaqt-network-service" not in network_text:
        raise RuntimeError("staged Network1 user unit does not target the production binary and bus name")
    settings_entry = desktop_entry(settings_desktops[0])
    if settings_entry.get("exec") != "qindaqt-settings":
        raise RuntimeError("staged Settings desktop entry does not launch qindaqt-settings")
    if len(dbus_units) < 3:
        raise RuntimeError("staged Audio1, Network1, and Settings1 D-Bus activation files are required")
    expected_activations = {
        "org.qindaqt.Audio1.service": ("Name=org.qindaqt.Audio1", "Exec=/usr/bin/qindaqt-audio-service"),
        "org.qindaqt.Network1.service": ("Name=org.qindaqt.Network1", "Exec=/usr/bin/qindaqt-network-service"),
        "org.qindaqt.Settings1.service": ("Name=org.qindaqt.Settings1", "Exec=/usr/bin/qindaqt-settings-service"),
    }
    for filename, expected_lines in expected_activations.items():
        matches = list(stage_root.rglob(filename))
        if len(matches) != 1:
            raise RuntimeError(f"expected exactly one staged {filename}; found {len(matches)}")
        text = matches[0].read_text()
        if any(line not in text for line in expected_lines):
            raise RuntimeError(f"staged D-Bus activation file {filename} has unexpected Name/Exec")
    session_entry = validate_session_entry(
        session_entries[0], install_root / "bin" / "qindaqt-wm"
    )
    report = preflight_report(repo, build_dir, stage_root)
    report["rollback"]["executionHost"] = report["executionHost"]["hostname"]
    report["rollback"]["hostBound"] = True
    report["rollback"]["deploymentScope"] = "host-local evidence; generated recipes are never portable to another host"
    report["stageBuild"] = {
        "testing": "disabled; checkpoint Python tests are run separately",
        "makeopts": make["value"],
        "installPrefix": str(INSTALL_PREFIX),
        "stageRoot": str(stage_root),
        "commands": {"configure": configure, "build": build, "install": install},
        "installEnvironment": {"DESTDIR": str(stage_root.resolve())},
        "logs": {"configure": str(log_dir / "configure.log"), "build": str(log_dir / "build.log"), "install": str(log_dir / "install.log")},
        "preBuildInstallAudit": pre_build_install_audit,
        "installDestinationAudit": install_destination_audit,
        "stagedPathCount": len(staged_entries),
        "installManifestPathCount": len(installed_manifest_paths),
    }
    report["stagedInstall"] = {
        "source": project_commit(repo),
        "versions": source_versions(repo),
        "stageRoot": str(stage_root),
        "installPrefix": str(INSTALL_PREFIX),
        "executables": [str(path) for path in expected],
        "binarySha256": {path.name: sha256_file(path) for path in expected},
        "units": [str(path) for path in [*audio_units, *network_units]],
        "unitSha256": {path.name: sha256_file(path) for path in [*audio_units, *network_units]},
        "dbusActivationFiles": [str(path) for path in sorted(dbus_units)],
        "settingsDesktop": settings_entry,
        "sessionEntry": session_entry,
        "sessionBinary": str(install_root / "bin" / "qindaqt-wm"),
        "routeIds": source_settings_route_ids(repo),
    }
    package = installed_package()
    active = []
    for spec in SERVICES.values():
        state = unit_state(spec["unit"])
        if state.get("ActiveState") == "active":
            active.append(spec["unit"])
    config = state_locations()
    snapshot_paths = [config[key] for key in ("settings1", "audioConsole", "audioVban", "audioMacros", "audioPresets", "audioUnitDropIns", "networkUnitDropIns")]
    base = stage_root.parent
    checksum = base / "pre-live-state.sha256"
    snapshot_script = base / "capture-live-state.sh"
    rollback_script = base / ROLLBACK_SCRIPT.name
    manifest = base / "stage-manifest.json"
    write_executable(snapshot_script, render_snapshot_script(snapshot_paths, base / "pre-live-state.tar", checksum))
    if package.get("rollbackAvailable"):
        write_executable(
            rollback_script,
            render_rollback_script(
                package,
                active,
                base / "pre-live-state.tar",
                checksum,
                report["executionHost"]["hostname"],
            ),
        )
        report["rollback"]["rollbackScript"] = str(rollback_script)
        report["rollback"]["captureScript"] = str(snapshot_script)
        report["rollback"]["archive"] = str(base / "pre-live-state.tar")
        report["rollback"]["captureCommand"] = f"{snapshot_script} COPY-QINDAQT-USER-STATE"
        report["rollback"]["rollbackCommand"] = f"{rollback_script} RESTORE-QINDAQT-DESKTOP"
    else:
        report["rollback"]["rollbackScript"] = None
        report["rollback"]["rollbackUnavailableReason"] = (
            "prior installed/repository ebuild or source archive is not present and hashable"
        )
    report["manifest"] = str(manifest)
    manifest.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return report
