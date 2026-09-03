# SPDX-License-Identifier: GPL-3.0-or-later
"""Static ELF and QML closure checks for an installed desktop stage."""

from __future__ import annotations

import os
import platform
import re
import stat
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence


class StageClosureError(RuntimeError):
    """The staged desktop cannot resolve its runtime package closure."""


_DYNAMIC_VALUE = re.compile(r"\((NEEDED|RPATH|RUNPATH)\).*\[([^]]*)\]")
_QML_IMPORT = re.compile(r"^\s*import\s+([A-Za-z_][A-Za-z0-9_.]*)", re.MULTILINE)


@dataclass(frozen=True)
class DynamicInfo:
    needed: tuple[str, ...]
    rpath: tuple[str, ...]
    runpath: tuple[str, ...]


@dataclass(frozen=True)
class ClosureReport:
    elf_files: int
    needed_entries: int
    qml_modules: tuple[str, ...]


def parse_dynamic_output(output: str) -> DynamicInfo:
    values: dict[str, list[str]] = {"NEEDED": [], "RPATH": [], "RUNPATH": []}
    for kind, value in _DYNAMIC_VALUE.findall(output):
        if kind == "NEEDED":
            values[kind].append(value)
        else:
            values[kind].extend(part for part in value.split(":") if part)
    return DynamicInfo(
        tuple(values["NEEDED"]),
        tuple(values["RPATH"]),
        tuple(values["RUNPATH"]),
    )


def _inside(path: Path, root: Path) -> bool:
    return path == root or root in path.parents


def _regular_resolved(path: Path) -> Path | None:
    try:
        resolved = path.resolve(strict=True)
        info = resolved.stat()
    except OSError:
        return None
    return resolved if stat.S_ISREG(info.st_mode) else None


def _elf_files(stage: Path) -> tuple[Path, ...]:
    files: list[Path] = []
    for candidate in stage.rglob("*"):
        try:
            info = candidate.lstat()
        except OSError as error:
            raise StageClosureError(f"cannot inspect staged path {candidate}: {error}") from error
        if stat.S_ISLNK(info.st_mode):
            resolved = _regular_resolved(candidate)
            if resolved is None or not _inside(resolved, stage):
                raise StageClosureError(f"staged symlink escapes or is broken: {candidate}")
            continue
        if not stat.S_ISREG(info.st_mode):
            continue
        try:
            with candidate.open("rb") as stream:
                magic = stream.read(4)
        except OSError as error:
            raise StageClosureError(f"cannot read staged file {candidate}: {error}") from error
        if magic == b"\x7fELF":
            files.append(candidate)
    return tuple(sorted(files))


def _dynamic_info(readelf: Path, elf: Path) -> DynamicInfo:
    completed = subprocess.run(
        [str(readelf), "--dynamic", "--wide", str(elf)],
        text=True,
        capture_output=True,
        check=False,
        timeout=20,
        env={key: value for key, value in os.environ.items() if key != "LD_LIBRARY_PATH"},
    )
    if completed.returncode != 0:
        raise StageClosureError(f"readelf rejected {elf}: {completed.stderr.strip()}")
    return parse_dynamic_output(completed.stdout)


def _expand_search_entry(entry: str, origin: Path) -> Path:
    lib_token = "lib64" if platform.architecture()[0] == "64bit" else "lib"
    expanded = entry.replace("${ORIGIN}", str(origin)).replace("$ORIGIN", str(origin))
    expanded = expanded.replace("${LIB}", lib_token).replace("$LIB", lib_token)
    expanded = expanded.replace("${PLATFORM}", platform.machine())
    expanded = expanded.replace("$PLATFORM", platform.machine())
    path = Path(expanded)
    if not path.is_absolute():
        raise StageClosureError(f"relative ELF search path is not stage-safe: {entry!r}")
    return path


def _resolve_needed(
    elf: Path,
    info: DynamicInfo,
    needed: str,
    stage: Path,
    system_directories: Sequence[Path],
    forbidden_roots: Sequence[Path],
) -> Path:
    if "/" in needed:
        candidates = [Path(needed)]
    else:
        encoded_paths = info.runpath if info.runpath else info.rpath
        search = [_expand_search_entry(entry, elf.parent) for entry in encoded_paths]
        search.extend(system_directories)
        candidates = [directory / needed for directory in search]
    for candidate in candidates:
        resolved = _regular_resolved(candidate)
        if resolved is None:
            continue
        if _inside(resolved, stage):
            return resolved
        if any(_inside(resolved, root) for root in forbidden_roots):
            raise StageClosureError(
                f"{elf} resolves {needed} through forbidden developer path {resolved}"
            )
        if any(_inside(resolved, directory) for directory in system_directories):
            return resolved
        raise StageClosureError(f"{elf} resolves {needed} outside stage/system roots: {resolved}")
    raise StageClosureError(f"{elf} cannot resolve DT_NEEDED {needed}")


def qml_imports(sources: Iterable[Path]) -> tuple[str, ...]:
    modules: set[str] = set()
    for source in sources:
        try:
            text = source.read_text(encoding="utf-8")
        except (OSError, UnicodeError) as error:
            raise StageClosureError(f"cannot inspect QML source {source}: {error}") from error
        modules.update(
            module for module in _QML_IMPORT.findall(text) if module.startswith("QindaQt.")
        )
    return tuple(sorted(modules))


def _authenticate_qmldirs(
    qml_root: Path,
    modules: Sequence[str],
    embedded_modules: Sequence[str] = (),
) -> None:
    root = qml_root.resolve(strict=True)
    imported = set(modules)
    embedded = set(embedded_modules)
    unexpected = embedded - imported
    if unexpected:
        raise StageClosureError(
            "embedded QML module exemptions are not imported: "
            + ", ".join(sorted(unexpected))
        )
    for module in modules:
        if module in embedded:
            continue
        qmldir = root.joinpath(*module.split("."), "qmldir")
        try:
            info = qmldir.lstat()
        except OSError as error:
            raise StageClosureError(f"staged QML module {module} has no qmldir") from error
        resolved = _regular_resolved(qmldir)
        if stat.S_ISLNK(info.st_mode) or resolved is None or not _inside(resolved, root):
            raise StageClosureError(f"staged QML module {module} has an unsafe qmldir")


def verify_stage_closure(
    stage_root: Path,
    *,
    readelf: Path,
    qml_directory: str,
    qml_sources: Sequence[Path],
    system_library_directories: Sequence[Path],
    forbidden_roots: Sequence[Path],
    shell_relative: Path,
    required_shell_libraries: Sequence[str],
    embedded_qml_modules: Sequence[str] = (),
) -> ClosureReport:
    stage = stage_root.resolve(strict=True)
    tool = readelf.resolve(strict=True)
    system_dirs = tuple(path.resolve(strict=True) for path in system_library_directories)
    forbidden = tuple(path.resolve(strict=True) for path in forbidden_roots)
    elf_files = _elf_files(stage)
    if not elf_files:
        raise StageClosureError("DesktopVirtual stage contains no ELF files")

    resolved_by_elf: dict[Path, dict[str, Path]] = {}
    needed_count = 0
    for elf in elf_files:
        info = _dynamic_info(tool, elf)
        needed_count += len(info.needed)
        resolved_by_elf[elf] = {
            needed: _resolve_needed(elf, info, needed, stage, system_dirs, forbidden)
            for needed in info.needed
        }

    shell = (stage / shell_relative).resolve(strict=True)
    if shell not in resolved_by_elf:
        raise StageClosureError("staged qindaqt-shell is not an authenticated ELF file")
    shell_dependencies = resolved_by_elf[shell]
    for library in required_shell_libraries:
        resolved = shell_dependencies.get(library)
        if resolved is None:
            raise StageClosureError(f"qindaqt-shell does not declare required library {library}")
        if not _inside(resolved, stage):
            raise StageClosureError(f"qindaqt-shell resolves {library} outside DesktopVirtual")

    imports = qml_imports(qml_sources)
    _authenticate_qmldirs(stage / qml_directory, imports, embedded_qml_modules)
    return ClosureReport(len(elf_files), needed_count, imports)
