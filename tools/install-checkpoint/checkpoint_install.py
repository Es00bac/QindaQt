#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Validate generated CMake destinations and staged desktop entry outputs."""

from __future__ import annotations

import os
from pathlib import Path
import re
from typing import Any, Sequence

from checkpoint_audit import desktop_entry
from checkpoint_contract import INSTALL_PREFIX, SESSION_EXEC, SESSION_TRY_EXEC


_INSTALL_DESTINATION = re.compile(
    r'(?is)file\s*\(\s*INSTALL\s+DESTINATION\s+(?:"([^"]*)"|([^\s)]+))'
)
_INSTALL_COMMAND = re.compile(r"(?is)file\s*\(\s*INSTALL\b")
_FILE_OPERATION = re.compile(r"(?im)^\s*file\s*\(\s*([A-Za-z_]+)\b")
_GENERATED_COMMAND = re.compile(r"(?im)^\s*([A-Za-z_][A-Za-z_0-9]*)\s*\(")
_FILE_WRITE_TARGET = re.compile(
    r'(?ims)^\s*file\s*\(\s*(?:WRITE|APPEND)\s+(?:"([^"]*)"|([^\s)]+))'
)
_RPATH_COMMAND = re.compile(
    r"(?ims)^\s*file\s*\(\s*(RPATH_CHECK|RPATH_CHANGE)\b(.*?)\)"
)
_RPATH_FILE = re.compile(r'(?i)\bFILE\s+"([^"]+)"')
_EXECUTE_PROCESS = re.compile(
    r'(?im)^\s*execute_process\s*\(\s*COMMAND\s+"([^"]+)"\s+"([^"]+)"\s*\)'
)
_EXECUTE_PROCESS_CALL = re.compile(r"(?im)^\s*execute_process\s*\(")
_INCLUDE = re.compile(r'(?im)^\s*include\s*\(\s*"([^"]+)"(?:\s+(OPTIONAL))?\s*\)')
_INCLUDE_CALL = re.compile(r"(?im)^\s*include\s*\(")
_LIST_COMMAND = re.compile(r"(?ims)^\s*list\s*\((.*?)\)")
_SUPPORTED_GENERATED_COMMANDS = {
    "else", "elseif", "endif", "execute_process", "file", "if", "include",
    "list", "message", "set", "string", "unset",
}


def validate_session_entry(path: Path, staged_binary: Path) -> dict[str, Any]:
    entry = desktop_entry(path)
    if not entry.get("exists"):
        raise RuntimeError(f"staged QindaQt session entry is unreadable: {path}")
    if entry.get("exec") != SESSION_EXEC:
        raise RuntimeError(f"staged QindaQt session Exec must equal {SESSION_EXEC}")
    if entry.get("tryExec") != SESSION_TRY_EXEC:
        raise RuntimeError(f"staged QindaQt session TryExec must equal {SESSION_TRY_EXEC}")
    if not staged_binary.is_file() or not os.access(staged_binary, os.X_OK):
        raise RuntimeError(f"staged QindaQt session binary is missing or not executable: {staged_binary}")
    return entry


def _resolved_install_destination(destination: str, install_prefix: Path) -> Path:
    expanded = destination.replace("${CMAKE_INSTALL_PREFIX}", str(install_prefix))
    if "$" in expanded:
        raise ValueError(f"unsupported variable in generated CMake install destination: {destination}")
    resolved = Path(expanded)
    if not resolved.is_absolute():
        resolved = install_prefix / resolved
    return Path(os.path.normpath(str(resolved)))


def _staged_destination(destination: Path, stage_root: Path) -> Path:
    staged = Path(os.path.normpath(str(stage_root / str(destination).lstrip("/"))))
    if not staged.is_relative_to(stage_root):
        raise ValueError(f"generated install destination escapes the stage root: {destination}")
    return staged


def _validate_build_write_target(target: str, build_dir: Path, script: Path) -> None:
    target = target.replace("${CMAKE_INSTALL_MANIFEST}", "install_manifest.txt")
    if "$" in target:
        raise ValueError(f"unsupported variable in generated file write: {script}: {target}")
    output = Path(os.path.normpath(target))
    resolved_output = output.resolve()
    if (
        not output.is_absolute()
        or not output.is_relative_to(build_dir)
        or not resolved_output.is_relative_to(build_dir)
    ):
        raise ValueError(f"generated file write is outside the ignored build tree: {script}: {target}")


def audit_generated_install_effects(
    scripts: Sequence[Path],
    build_dir: Path,
    stage_root: Path,
    install_prefix: Path,
) -> dict[str, int]:
    """Allow only build-local manifests and DESTDIR-rooted install side effects."""
    build = build_dir.resolve()
    root = Path(os.path.abspath(stage_root))
    counts = {"buildTreeWrites": 0, "rpathChecks": 0, "destdirStripCommands": 0, "generatedIncludes": 0}
    pending = list(scripts)
    queued = {path.resolve() for path in pending}
    audited: set[Path] = set()

    while pending:
        script = pending.pop()
        script_identity = script.resolve()
        if script_identity in audited:
            continue
        if not script_identity.is_relative_to(build):
            raise ValueError(f"generated install script is outside the build tree: {script}")
        if script.is_symlink():
            raise ValueError(f"generated install script must not be a symlink: {script}")
        audited.add(script_identity)
        content = script.read_text(encoding="utf-8", errors="replace")
        commands = [match.group(1).lower() for match in _GENERATED_COMMAND.finditer(content)]
        unsupported_commands = sorted(set(commands) - _SUPPORTED_GENERATED_COMMANDS)
        if unsupported_commands:
            raise ValueError(
                f"unsupported generated CMake command(s) in {script}: {', '.join(unsupported_commands)}"
            )

        # AGENT-GUARD: CMakeCache is checked before the audit and /usr plus
        # DESTDIR must remain fixed through install execution; either mutation
        # invalidates the generated destination mapping below.
        prefix_assignments = re.findall(
            r"(?ims)^\s*set\s*\(\s*CMAKE_INSTALL_PREFIX\b(.*?)\)", content
        )
        safe_prefix_values = {f'"{install_prefix}"', str(install_prefix)}
        if any(body.strip() not in safe_prefix_values for body in prefix_assignments):
            raise ValueError(f"generated install script changes CMAKE_INSTALL_PREFIX: {script}")
        if re.search(r"(?im)^\s*(?:set|unset)\s*\(\s*ENV\{DESTDIR\}", content):
            raise ValueError(f"generated install script changes DESTDIR: {script}")
        if re.search(r"(?im)^\s*unset\s*\(\s*CMAKE_INSTALL_PREFIX\b", content):
            raise ValueError(f"generated install script unsets CMAKE_INSTALL_PREFIX: {script}")
        if re.search(r"(?im)^\s*unset\s*\(\s*CMAKE_INSTALL_MANIFEST\b", content):
            raise ValueError(f"generated install script unsets its manifest path: {script}")
        for match in re.finditer(r"(?ims)^\s*string\s*\((.*?)\)", content):
            body = " ".join(match.group(1).split())
            if "CMAKE_INSTALL_PREFIX" in body and body != (
                'REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}"'
            ):
                raise ValueError(f"generated install script mutates CMAKE_INSTALL_PREFIX: {script}: {body}")
            if re.search(r"\bCMAKE_INSTALL_MANIFEST\b", body):
                raise ValueError(f"generated install script mutates its manifest path: {script}: {body}")
        for match in _LIST_COMMAND.finditer(content):
            body = " ".join(match.group(1).split())
            if not re.fullmatch(
                r'APPEND CMAKE_ABSOLUTE_DESTINATION_FILES(?: "[^"]+")+?', body
            ):
                raise ValueError(f"unsupported generated list() mutation in {script}: {body}")
        manifest_assignments = re.findall(
            r"(?ims)^\s*set\s*\(\s*CMAKE_INSTALL_MANIFEST\b(.*?)\)", content
        )
        safe_manifest_names = {
            "install_manifest.txt",
            'install_manifest_${CMAKE_INSTALL_COMPONENT}.txt',
            'install_manifest_${CMAKE_INST_COMP_HASH}.txt',
        }
        if any(
            body.strip() not in {f'"{name}"' for name in safe_manifest_names} | safe_manifest_names
            for body in manifest_assignments
        ):
            raise ValueError(f"generated install script moves its manifest outside the build tree: {script}")

        operations = list(_FILE_OPERATION.finditer(content))
        install_calls = len(_INSTALL_COMMAND.findall(content))
        if len(operations) != install_calls + len(
            list(_FILE_WRITE_TARGET.finditer(content))
        ) + len(list(_RPATH_COMMAND.finditer(content))):
            raise ValueError(f"could not classify every generated file() command in {script}")

        for match in operations:
            operation = match.group(1).upper()
            if operation == "INSTALL":
                continue
            if operation in {"WRITE", "APPEND"}:
                target_match = _FILE_WRITE_TARGET.match(content[match.start() :])
                if not target_match:
                    raise ValueError(f"could not parse generated file({operation}) target in {script}")
                target = target_match.group(1) if target_match.group(1) is not None else target_match.group(2)
                assert target is not None
                _validate_build_write_target(target, build, script)
                counts["buildTreeWrites"] += 1
                continue
            if operation in {"RPATH_CHECK", "RPATH_CHANGE"}:
                block = _RPATH_COMMAND.match(content[match.start() :])
                target_match = _RPATH_FILE.search(block.group(2)) if block else None
                if not target_match:
                    raise ValueError(f"could not parse generated file({operation}) target in {script}")
                target = target_match.group(1)
                staged_prefix = "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/"
                if not target.startswith(staged_prefix):
                    raise ValueError(f"generated file({operation}) target is not DESTDIR-rooted: {script}: {target}")
                suffix = target[len(staged_prefix) :]
                resolved = _resolved_install_destination(f"${{CMAKE_INSTALL_PREFIX}}/{suffix}", install_prefix)
                _staged_destination(resolved, root)
                counts["rpathChecks"] += 1
                continue
            raise ValueError(f"unsupported generated file({operation}) side effect in {script}")

        execute_calls = len(list(_EXECUTE_PROCESS_CALL.finditer(content)))
        execute_matches = list(_EXECUTE_PROCESS.finditer(content))
        if execute_calls != len(execute_matches):
            raise ValueError(f"could not classify every generated execute_process() in {script}")
        for match in execute_matches:
            executable, target = match.groups()
            if executable != "/usr/bin/strip":
                raise ValueError(f"unsupported generated install process in {script}: {executable}")
            staged_prefix = "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/"
            if not target.startswith(staged_prefix):
                raise ValueError(f"generated strip target is not DESTDIR-rooted: {script}: {target}")
            suffix = target[len(staged_prefix) :]
            resolved = _resolved_install_destination(f"${{CMAKE_INSTALL_PREFIX}}/{suffix}", install_prefix)
            _staged_destination(resolved, root)
            counts["destdirStripCommands"] += 1

        include_calls = len(list(_INCLUDE_CALL.finditer(content)))
        include_matches = list(_INCLUDE.finditer(content))
        if include_calls != len(include_matches):
            raise ValueError(f"could not classify every generated include() in {script}")
        for match in include_matches:
            included = Path(match.group(1)).resolve()
            optional = match.group(2) is not None
            if not included.is_relative_to(build):
                raise ValueError(f"generated install script includes a file outside the build tree: {script}: {included}")
            if not included.is_file():
                if optional:
                    counts.setdefault("missingOptionalIncludes", 0)
                    counts["missingOptionalIncludes"] += 1
                    continue
                raise ValueError(f"generated install script includes a missing file: {script}: {included}")
            if included not in queued:
                pending.append(included)
                queued.add(included)
            counts["generatedIncludes"] += 1

        if re.search(r"(?im)^\s*cmake_language\s*\(", content):
            raise ValueError(f"unsupported generated cmake_language() side effect in {script}")
    return counts


def audit_cmake_install_scripts(
    build_dir: Path,
    stage_root: Path,
    install_prefix: Path = INSTALL_PREFIX,
) -> dict[str, Any]:
    """Audit every generated CMake install destination against the DESTDIR root."""
    build = build_dir.resolve()
    root = Path(os.path.abspath(stage_root))
    prefix = Path(os.path.normpath(str(install_prefix)))
    if not prefix.is_absolute():
        raise ValueError(f"CMake install prefix must be absolute: {install_prefix}")
    scripts: list[Path] = []
    for current, directories, files in os.walk(build, followlinks=False):
        # AGENT-NOTE: CMake build trees contain large object/dependency trees;
        # os.walk keeps the preinstall gate fast while visiting every
        # cmake_install.cmake without following linked directories. A linked
        # directory could hide another script from the containment audit.
        linked_directories = [
            name for name in directories if (Path(current) / name).is_symlink()
        ]
        if linked_directories:
            raise ValueError(
                f"generated install tree contains symlinked directories under {current}: "
                + ", ".join(linked_directories)
            )
        if "cmake_install.cmake" in files:
            script = Path(current) / "cmake_install.cmake"
            if script.is_symlink():
                raise ValueError(f"generated install script must not be a symlink: {script}")
            scripts.append(script)
    scripts.sort()
    if not scripts:
        raise ValueError(f"no generated cmake_install.cmake files found beneath {build}")

    rule_count = 0
    prefix_rule_count = 0
    literal_absolute_rules: list[dict[str, str]] = []
    for script in scripts:
        content = script.read_text(encoding="utf-8", errors="replace")
        matches = list(_INSTALL_DESTINATION.finditer(content))
        invocation_count = len(_INSTALL_COMMAND.findall(content))
        if invocation_count != len(matches):
            raise ValueError(f"could not parse every generated install destination in {script}")
        for match in matches:
            destination = match.group(1) if match.group(1) is not None else match.group(2)
            assert destination is not None
            normalized = _resolved_install_destination(destination, prefix)
            # AGENT-GUARD: DESTDIR must prefix the complete absolute CMake
            # destination, including rules such as /etc/xdg/autostart. Mapping
            # destinations by string concatenation without normalization can
            # let a crafted `..` path escape the ignored stage root.
            staged_destination = _staged_destination(normalized, root)
            rule_count += 1
            if "${CMAKE_INSTALL_PREFIX}" in destination or not Path(destination).is_absolute():
                prefix_rule_count += 1
            else:
                literal_absolute_rules.append(
                    {
                        "script": str(script.relative_to(build)),
                        "destination": destination,
                        "stagedDestination": str(staged_destination),
                    }
                )
    side_effect_audit = audit_generated_install_effects(scripts, build, root, prefix)
    return {
        "cmakeInstallScriptCount": len(scripts),
        "installRuleCount": rule_count,
        "installPrefixResolvedRuleCount": prefix_rule_count,
        "literalAbsoluteDestinationRules": literal_absolute_rules,
        "otherGeneratedSideEffects": side_effect_audit,
        "stageRoot": str(root),
        "installPrefix": str(prefix),
    }


def validate_staged_paths(stage_root: Path, paths: Sequence[Path]) -> list[str]:
    """Fail unless every lexical staged path is beneath the ignored DESTDIR root."""
    root = Path(os.path.abspath(stage_root))
    accepted: list[str] = []
    for path in paths:
        candidate = Path(os.path.abspath(path))
        if not candidate.is_relative_to(root) or candidate == root:
            raise RuntimeError(f"staged path is outside the DESTDIR root {root}: {path}")
        accepted.append(str(candidate))
    return accepted
