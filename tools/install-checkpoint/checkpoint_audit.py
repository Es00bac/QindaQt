#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only live-state audit used by the install-checkpoint CLI."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import socket
import struct
import subprocess
from typing import Any, Sequence

from checkpoint_contract import (
    BUILD, CMAKE_OPTIONS, INSTALL_PREFIX, ROOT, ROLLBACK_CONFIRMATION, ROLLBACK_SCRIPT, SERVICES,
    STAGE_ROOT,
    SNAPSHOT, SNAPSHOT_CONFIRMATION, SNAPSHOT_SCRIPT,
)

def run_capture(argv: Sequence[str], timeout: float = 8.0) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(
            list(argv), check=False, text=True, capture_output=True, timeout=timeout
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        return subprocess.CompletedProcess(list(argv), 127, "", str(error))


def sha256_file(path: Path) -> str | None:
    try:
        digest = hashlib.sha256()
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(chunk)
        return digest.hexdigest()
    except OSError:
        return None


def parse_audio_snapshot_reply(reply: str) -> int:
    """Read only the first scalar after the Snapshot tuple signature."""
    tokens = reply.split()
    if len(tokens) < 2 or not tokens[0].startswith("(u"):
        raise ValueError("Audio1 GetSnapshot reply has an unexpected signature")
    return int(tokens[1], 10)


def parse_network_snapshot_reply(reply: str) -> tuple[int, int]:
    """Return (codec version, Network1 protocol version) from its fixed header."""
    tokens = reply.split()
    if len(tokens) < 2 or tokens[0] != "ay":
        raise ValueError("Network1 GetSnapshot reply is not a byte array")
    count = int(tokens[1], 10)
    if count < 12 or len(tokens) < count + 2:
        raise ValueError("Network1 GetSnapshot reply is truncated")
    payload = bytes(int(value, 10) for value in tokens[2 : count + 2])
    if payload[:4] != b"QN1S":
        raise ValueError("Network1 GetSnapshot reply has an unknown header")
    return struct.unpack(">II", payload[4:12])


def live_bus_version(service: dict[str, str], busctl: str = "busctl") -> dict[str, Any]:
    # AGENT-CONTRACT: The checkpoint may read GetSnapshot only from an owner
    # already present on the user's bus. Addressing its unique name with
    # --auto-start=no prevents a preflight from activating or replacing a service.
    owner = run_capture(
        [
            busctl,
            "--user",
            "--no-pager",
            "call",
            "org.freedesktop.DBus",
            "/org/freedesktop/DBus",
            "org.freedesktop.DBus",
            "GetNameOwner",
            "s",
            service["bus_name"],
        ]
    )
    if owner.returncode != 0:
        return {"ownerPresent": False, "version": None}
    owner_tokens = owner.stdout.split()
    unique_name = owner_tokens[-1].strip('"') if owner_tokens else ""
    if not unique_name.startswith(":"):
        return {"ownerPresent": False, "version": None, "error": "invalid owner reply"}
    reply = run_capture(
        [
            busctl,
            "--user",
            "--no-pager",
            "--auto-start=no",
            "call",
            unique_name,
            service["object_path"],
            service["interface"],
            "GetSnapshot",
        ]
    )
    if reply.returncode != 0:
        return {"ownerPresent": True, "version": None, "error": reply.stderr.strip()[:200]}
    try:
        if service["version_kind"] == "audio":
            version: Any = {"schemaVersion": parse_audio_snapshot_reply(reply.stdout)}
        else:
            codec, protocol = parse_network_snapshot_reply(reply.stdout)
            version = {"codecVersion": codec, "protocolVersion": protocol}
        return {"ownerPresent": True, "version": version}
    except (ValueError, OverflowError) as error:
        return {"ownerPresent": True, "version": None, "error": str(error)}


def parse_properties(output: str) -> dict[str, str]:
    properties: dict[str, str] = {}
    for line in output.splitlines():
        key, separator, value = line.partition("=")
        if separator:
            properties[key] = value
    return properties


def unit_state(unit: str, systemctl: str = "systemctl") -> dict[str, Any]:
    result = run_capture(
        [
            systemctl,
            "--user",
            "show",
            unit,
            "--property=LoadState,ActiveState,SubState,UnitFileState,FragmentPath,DropInPaths,MainPID,ExecStart",
            "--no-pager",
        ]
    )
    if result.returncode != 0:
        return {"available": False, "error": result.stderr.strip()[:200]}
    return {"available": True, **parse_properties(result.stdout)}


def executable_for_pid(pid: str) -> str | None:
    if not pid.isdigit() or int(pid) <= 0:
        return None
    try:
        return str((Path("/proc") / pid / "exe").resolve(strict=True))
    except OSError:
        return None


def process_inventory() -> list[dict[str, Any]]:
    wanted = {
        "qindaqt-settings",
        "qindaqt-settings-service",
        "qindaqt-audio-service",
        "qindaqt-network-service",
        "qindaqt-session",
        "qindaqt-shell",
        "qindaqt-wm",
    }
    found: list[dict[str, Any]] = []
    proc = Path("/proc")
    try:
        entries = list(proc.iterdir())
    except OSError:
        return found
    for entry in entries:
        if not entry.name.isdigit():
            continue
        executable = executable_for_pid(entry.name)
        if executable and Path(executable).name in wanted:
            found.append({"pid": int(entry.name), "binary": Path(executable).name, "path": executable})
    return sorted(found, key=lambda item: (item["binary"], item["pid"]))


def state_locations(environment: dict[str, str] | None = None) -> dict[str, Path]:
    env = os.environ if environment is None else environment
    home = Path(env.get("HOME", str(Path.home()))).expanduser()
    config = Path(env.get("XDG_CONFIG_HOME", str(home / ".config"))).expanduser()
    data = Path(env.get("XDG_DATA_HOME", str(home / ".local/share"))).expanduser()
    return {
        "settings1": config / "qindaqt" / "settings-v2.json",
        "audioConsole": config / "qindaqt" / "audio-console.json",
        "audioVban": config / "qindaqt" / "audio-vban.json",
        "audioMacros": config / "qindaqt" / "audio-macros.json",
        "audioPresets": config / "qindaqt" / "audio-presets",
        "userData": data / "qindaqt",
        "audioUnitDropIns": config / "systemd/user/qindaqt-audio-service.service.d",
        "networkUnitDropIns": config / "systemd/user/qindaqt-network-service.service.d",
        "networkManagerProfiles": Path("/etc/NetworkManager/system-connections"),
    }


def describe_path(path: Path, *, redact_children: bool = True) -> dict[str, Any]:
    try:
        stat = path.stat()
    except OSError:
        return {"path": str(path), "exists": False}
    record: dict[str, Any] = {
        "path": str(path),
        "exists": True,
        "kind": "directory" if path.is_dir() else "file",
        "mode": oct(stat.st_mode & 0o777),
    }
    if path.is_file():
        record["bytes"] = stat.st_size
        record["sha256"] = sha256_file(path)
    elif path.is_dir():
        try:
            record["entryCount"] = sum(1 for _ in path.iterdir())
        except OSError:
            record["entryCount"] = None
        if not redact_children:
            record["entries"] = sorted(item.name for item in path.iterdir())
    return record


def desktop_entry(path: Path) -> dict[str, Any]:
    groups: dict[str, dict[str, str]] = {}
    current = ""
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return {"path": str(path), "exists": False}
    for line in lines:
        line = line.strip()
        if line.startswith("[") and line.endswith("]"):
            current = line[1:-1]
            groups.setdefault(current, {})
        elif current and "=" in line and not line.startswith("#"):
            key, value = line.split("=", 1)
            groups[current][key] = value
    entry = groups.get("Desktop Entry", {})
    actions = [value for value in entry.get("Actions", "").split(";") if value]
    return {
        "path": str(path),
        "exists": True,
        "exec": entry.get("Exec"),
        "tryExec": entry.get("TryExec"),
        "actions": actions,
        "actionExecs": {
            action: groups.get(f"Desktop Action {action}", {}).get("Exec")
            for action in actions
        },
    }


def settings_desktop_candidates(environment: dict[str, str] | None = None) -> list[Path]:
    env = os.environ if environment is None else environment
    home = Path(env.get("HOME", str(Path.home()))).expanduser()
    data_home = Path(env.get("XDG_DATA_HOME", str(home / ".local/share"))).expanduser()
    data_dirs = env.get("XDG_DATA_DIRS", "/usr/local/share:/usr/share").split(":")
    roots = [data_home, *(Path(root) for root in data_dirs if root)]
    return [root / "applications/org.qindaqt.Settings.desktop" for root in roots]


def source_versions(repo: Path = ROOT) -> dict[str, Any]:
    def integer(path: str, name: str) -> int:
        text = (repo / path).read_text(encoding="utf-8")
        match = re.search(rf"\b{name}\s*=\s*(\d+)", text)
        if not match:
            raise ValueError(f"could not find {name} in {path}")
        return int(match.group(1))

    schema = json.loads((repo / "data/settings/schema-v2.json").read_text())
    protocol_text = (repo / "src/services/settings_protocol/include/qindaqt/services/settings_protocol/settings_wire_contract.h").read_text()
    wire = re.search(r"WireSchemaVersion\s*=\s*(\d+)", protocol_text)
    if not wire:
        raise ValueError("could not find Settings1 wire schema version")
    project = (repo / "CMakeLists.txt").read_text(encoding="utf-8")
    version = re.search(r"project\(\s*QindaQt\s+VERSION\s+([^\s)]+)", project)
    if not version:
        raise ValueError("could not find project version")
    return {
        "projectVersion": version.group(1),
        "Settings1": {"wireSchemaVersion": int(wire.group(1)), "settingsSchemaVersion": int(schema["schemaVersion"])},
        "Audio1": {"schemaVersion": integer("src/services/audio_protocol/include/qindaqt/services/audio_protocol/audio_limits.h", "kSchemaVersion")},
        "Network1": {"protocolVersion": integer("src/services/network_protocol/include/qindaqt/services/network_protocol/network_limits.h", "kProtocolVersion")},
    }


def installed_package() -> dict[str, Any]:
    db = Path("/var/db/pkg/gui-wm")
    packages = sorted(path for path in db.glob("qindaqt-desktop-*") if path.is_dir())
    if not packages:
        return {"installed": False}
    package = packages[-1]
    category, pf = "gui-wm", package.name
    ebuild = package / f"{pf}.ebuild"
    repo_name = package_repository_name(package)
    repo_ebuild = repository_ebuild_path(repo_name, category, pf)
    distfile = Path("/var/cache/distfiles") / f"{pf}.tar.gz"
    ebuild_text = (
        repo_ebuild.read_text(encoding="utf-8", errors="replace")
        if repo_ebuild and repo_ebuild.is_file()
        else ""
    )
    source_commit = re.search(r'^QINDAQT_COMMIT="([0-9a-f]{40})"$', ebuild_text, re.MULTILINE)
    metadata_ebuild_hash = sha256_file(ebuild)
    repository_ebuild_hash = sha256_file(repo_ebuild) if repo_ebuild else None
    source_archive_hash = sha256_file(distfile)
    return {
        "installed": True,
        "atom": package_atom(category, pf, repo_name),
        "pf": pf,
        "repositoryName": repo_name,
        "metadataEbuildPresent": ebuild.is_file(),
        "metadataEbuildSha256": metadata_ebuild_hash,
        "repositoryEbuild": str(repo_ebuild) if repo_ebuild else None,
        "repositoryEbuildPresent": repo_ebuild.is_file() if repo_ebuild else False,
        "repositoryEbuildSha256": repository_ebuild_hash,
        "sourceCommit": source_commit.group(1) if source_commit else None,
        "sourceArchive": str(distfile),
        "sourceArchivePresent": distfile.is_file(),
        "sourceArchiveSha256": source_archive_hash,
        "rollbackAvailable": (
            ebuild.is_file()
            and repo_ebuild is not None
            and repo_ebuild.is_file()
            and distfile.is_file()
            and metadata_ebuild_hash is not None
            and repository_ebuild_hash is not None
            and source_archive_hash is not None
        ),
    }


def package_repository_name(package: Path) -> str | None:
    """Read the repository identity recorded with the installed Portage package."""
    try:
        name = (package / "repository").read_text(encoding="utf-8").strip()
    except OSError:
        return None
    return name or None


def package_atom(category: str, pf: str, repository_name: str | None) -> str:
    """Pin the rollback atom to the repository recorded in Portage's VDB."""
    qualifier = f"::{repository_name}" if repository_name else ""
    return f"={category}/{pf}{qualifier}"


def repository_ebuild_path(repository_name: str | None, category: str, pf: str) -> Path | None:
    if not repository_name:
        return None
    result = run_capture(["portageq", "get_repo_path", "/", repository_name])
    repository_root = result.stdout.strip()
    if result.returncode != 0 or not repository_root:
        return None
    return Path(repository_root) / category / "qindaqt-desktop" / f"{pf}.ebuild"


def source_settings_route_ids(repo: Path = ROOT) -> list[str]:
    source = (repo / "src/apps/settings_center/settings_route_registry.cpp").read_text(encoding="utf-8")
    return re.findall(r"\.id\s*=\s*QStringLiteral\(\"([^\"]+)\"\)", source)


def project_commit(repo: Path = ROOT) -> dict[str, Any]:
    result = run_capture(["git", "-C", str(repo), "rev-parse", "HEAD"])
    commit = result.stdout.strip() if result.returncode == 0 else None
    status = run_capture(["git", "-C", str(repo), "status", "--porcelain"])
    changed = [line[3:] for line in status.stdout.splitlines() if len(line) > 3]
    return {"commit": commit, "dirty": bool(changed), "dirtyPaths": changed}


def makeopts() -> dict[str, Any]:
    result = run_capture(["portageq", "envvar", "MAKEOPTS"])
    raw = result.stdout.strip()
    tokens = shlex.split(raw) if result.returncode == 0 else []
    return {"available": result.returncode == 0 and bool(tokens), "configured": raw, "tokens": tokens}


def cmake_cache(build_dir: Path) -> dict[str, str]:
    cache = build_dir / "CMakeCache.txt"
    values: dict[str, str] = {}
    if not cache.is_file():
        return values
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("//") or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        name = key.split(":", 1)[0]
        if name in {"CMAKE_HOME_DIRECTORY", "CMAKE_INSTALL_PREFIX", "CMAKE_BUILD_TYPE", "BUILD_TESTING"}:
            values[name] = value
    return values


def validate_build_paths(repo: Path, build_dir: Path, stage_root: Path) -> None:
    build_root = (repo / "build").resolve()
    build = build_dir.resolve()
    stage = stage_root.resolve()
    # AGENT-GUARD: the staging root and build tree must both stay beneath the
    # ignored repository build/ directory; DESTDIR contains absolute install
    # destinations such as /etc and /usr beneath this root.
    if not build.is_relative_to(build_root) or not stage.is_relative_to(build_root):
        raise ValueError(f"build and stage root must remain beneath ignored {build_root}")
    if build == build_root or stage == build_root or stage == build:
        raise ValueError("build directory and stage root must be separate descendants of build/")
    ignored = run_capture(["git", "-C", str(repo), "check-ignore", "-q", str(stage.relative_to(repo.resolve()))])
    if ignored.returncode != 0:
        raise ValueError(f"Git does not confirm the stage root is ignored: {stage}")


def preflight_report(repo: Path = ROOT, build_dir: Path = BUILD, stage_root: Path = STAGE_ROOT) -> dict[str, Any]:
    validate_build_paths(repo, build_dir, stage_root)
    versions = source_versions(repo)
    package = installed_package()
    units = {name: unit_state(spec["unit"]) for name, spec in SERVICES.items()}
    for name, state in units.items():
        state["unit"] = SERVICES[name]["unit"]
        if state.get("available"):
            pid = state.get("MainPID", "0")
            executable = executable_for_pid(pid)
            state["runningExecutable"] = executable
            state["runningExecutableSha256"] = sha256_file(Path(executable)) if executable else None
            state["liveProtocol"] = live_bus_version(SERVICES[name])
    processes = process_inventory()
    shell_running = any(item["binary"] == "qindaqt-shell" for item in processes)
    unit_active = [
        SERVICES[name]["unit"]
        for name, state in units.items()
        if state.get("ActiveState") == "active"
    ]
    runtime_versions = {
        name: state.get("liveProtocol") for name, state in units.items() if state.get("available")
    }
    comparisons = {
        "Audio1": {
            "source": versions["Audio1"],
            "live": runtime_versions.get("Audio1", {}).get("version") if runtime_versions.get("Audio1") else None,
            "matches": (
                runtime_versions["Audio1"]["version"].get("schemaVersion")
                == versions["Audio1"]["schemaVersion"]
                if runtime_versions.get("Audio1", {}).get("version")
                else None
            ),
        },
        "Network1": {
            "source": versions["Network1"],
            "live": runtime_versions.get("Network1", {}).get("version") if runtime_versions.get("Network1") else None,
            "matches": (
                runtime_versions["Network1"]["version"].get("protocolVersion")
                == versions["Network1"]["protocolVersion"]
                if runtime_versions.get("Network1", {}).get("version")
                else None
            ),
        },
    }
    session_template = repo / "data/session/qindaqt.desktop.in"
    settings_routes = source_settings_route_ids(repo)
    return {
        "executionHost": {"hostname": socket.gethostname(), "scope": "local host running this command"},
        "source": project_commit(repo),
        "sourceVersions": versions,
        "liveProtocolComparison": comparisons,
        "installedPackage": package,
        "runningProcesses": processes,
        "userUnits": units,
        "settingsDesktopEntries": [desktop_entry(path) for path in settings_desktop_candidates() if path.is_file()],
        "settingsDesktopSource": desktop_entry(repo / "src/apps/settings_center/org.qindaqt.Settings.desktop"),
        "settingsRouteIds": settings_routes,
        "sessionStartup": {
            "sourceTemplate": str(session_template),
            "sourceExec": next((line.partition("=")[2] for line in session_template.read_text().splitlines() if line.startswith("Exec=")), None),
            "installedEntry": describe_path(Path("/usr/share/wayland-sessions/qindaqt.desktop")),
            "supervisorProcessObserved": any(item["binary"] == "qindaqt-session" for item in processes),
            "shellProcessObserved": shell_running,
        },
        "savedState": {
            "paths": {name: describe_path(path) for name, path in state_locations().items()},
            "copyPolicy": "Settings1 and Audio1 state is listed without contents; NetworkManager credential profiles are never read, copied, or restored.",
        },
        "makeopts": makeopts(),
        "stage": {
            "buildDirectory": str(build_dir),
            "stageRoot": str(stage_root),
            "installPrefix": str(INSTALL_PREFIX),
            "stageRootIgnored": run_capture(["git", "-C", str(repo), "check-ignore", "-q", str(stage_root.relative_to(repo.resolve()))]).returncode == 0,
            "existingCMakeCache": cmake_cache(build_dir),
            "configureCommand": ["cmake", "-S", str(repo), "-B", str(build_dir), *CMAKE_OPTIONS],
            "buildCommand": ["cmake", "--build", str(build_dir), "--", *makeopts()["tokens"]],
            "installCommand": ["cmake", "--install", str(build_dir)],
            "installEnvironment": {"DESTDIR": str(stage_root)},
        },
        "neededRestarts": {
            "daemonReload": "Run `systemctl --user daemon-reload` after replacing packaged user unit files.",
            "activeResidentServices": unit_active,
            "restartAfterInstall": unit_active,
            "settingsProcess": "qindaqt-settings is restarted by closing and reopening it; no Settings process is currently reported unless listed above.",
            "shellAndLogin": "A logout/login is required to load the staged qindaqt-wm, qindaqt-session, and qindaqt-shell from the installed package.",
        },
        "rollback": {
            "captureScript": str(SNAPSHOT_SCRIPT),
            "archive": str(SNAPSHOT),
            "captureCommand": f"{SNAPSHOT_SCRIPT} {SNAPSHOT_CONFIRMATION}",
            "rollbackScript": str(ROLLBACK_SCRIPT),
            "rollbackCommand": f"{ROLLBACK_SCRIPT} {ROLLBACK_CONFIRMATION}",
            "priorPackage": package,
            "steps": [
            "Before any live install, stop the listed active units, then run capture-live-state.sh COPY-QINDAQT-USER-STATE to archive the Settings1 and Audio1 files.",
                "On rollback, verify the exact ebuild, source archive, and state archive hashes, re-emerge the exact previously installed qindaqt-desktop package while services remain active, then stop the listed units only for state restore and restart.",
                "The rollback script is bound to the host that generated it. Start only the services listed as active at preflight, then log out and back in to replace already-loaded shell/session processes.",
            ],
        },
        "liveChangesPerformed": False,
    }


def write_executable(path: Path, contents: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(contents, encoding="utf-8")
    path.chmod(0o700)
