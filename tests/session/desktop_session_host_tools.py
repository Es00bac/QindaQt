# SPDX-License-Identifier: GPL-3.0-or-later
"""Authenticated host-tool prefixes and read-only sandbox projections."""

from __future__ import annotations

import os
from pathlib import Path, PurePosixPath

from desktop_session_sandbox import ReadOnlyMount, SandboxContractError


def tool_root(tool: Path) -> Path:
    """Return the installation prefix that contains one discovered input."""

    resolved = tool.resolve(strict=True)
    parts = resolved.parts
    if ".linuxbrew" in parts:
        index = parts.index(".linuxbrew")
        return Path(*parts[: index + 1])
    if len(parts) >= 3 and parts[1] == "usr":
        return Path("/usr")
    backend_suffixes = {
        (
            library, "qt6", "plugins", "kf6", "kscreen",
            "KSC_KWayland.so",
        )
        for library in ("lib", "lib64")
    }
    if tuple(parts[-6:]) in backend_suffixes:
        # The KScreen backend is a discovered input rather than an executable;
        # bind its installation prefix, not the nearby plugin subdirectory.
        return Path(*parts[:-6])
    return resolved.parent.parent


def system_mounts(tools: list[Path]) -> tuple[ReadOnlyMount, ...]:
    roots = sorted({tool_root(tool) for tool in tools}, key=str)
    mounts = [ReadOnlyMount(root, PurePosixPath(str(root))) for root in roots]
    loader_cache = Path("/etc/ld.so.cache")
    if loader_cache.is_file():
        # AGENT-GUARD: Mount this one lookup index, never host /etc or /lib*.
        # The libraries remain constrained to the already read-only /usr roots,
        # while private runtime paths retain LD_LIBRARY_PATH precedence.
        mounts.append(ReadOnlyMount(loader_cache, PurePosixPath("/etc/ld.so.cache")))
    return tuple(mounts)


def sandbox_path_for(source: Path, mounts: tuple[ReadOnlyMount, ...]) -> str:
    resolved = source.resolve(strict=True)
    for mount in mounts:
        root = mount.source.resolve(strict=True)
        if resolved == root or root in resolved.parents:
            return str(mount.destination / resolved.relative_to(root))
    raise SandboxContractError(f"tool is not covered by a system mount: {source}")


def resolved_system_input(
    source: Path, label: str, *, executable: bool
) -> Path:
    """Canonicalize one discovered host input before it enters the sandbox."""

    try:
        resolved = source.resolve(strict=True)
    except OSError as error:
        raise SandboxContractError(f"{label} is unavailable: {source}") from error
    if not resolved.is_file() or (executable and not os.access(resolved, os.X_OK)):
        raise SandboxContractError(f"{label} is not an authenticated file")
    return resolved


def library_search_roots(
    tools: list[Path],
) -> tuple[list[str], list[str], list[str]]:
    """Return sandbox library/plugin/qml roots for non-/usr tool prefixes."""

    roots = sorted({tool_root(tool) for tool in tools}, key=str)
    library_entries: list[str] = []
    plugin_entries: list[str] = []
    qml_entries: list[str] = []
    for root in roots:
        if str(root) == "/usr":
            continue
        # tool_root returns the executable's installation prefix (`/usr` or a
        # private `usr`), so appending another `usr` would empty its searches.
        for lib_dir in (root / "lib", root / "lib64"):
            if not lib_dir.is_dir():
                continue
            library_entries.append(str(PurePosixPath(str(lib_dir))))
            for child in lib_dir.iterdir():
                if child.is_dir() and any(child.glob("lib*.so*")):
                    library_entries.append(str(PurePosixPath(str(child))))
        plugin_dir = root / "lib" / "qt6" / "plugins"
        if plugin_dir.is_dir():
            plugin_entries.append(str(PurePosixPath(str(plugin_dir))))
        qml_dir = root / "lib" / "qt6" / "qml"
        if qml_dir.is_dir():
            qml_entries.append(str(PurePosixPath(str(qml_dir))))
    return library_entries, plugin_entries, qml_entries


def _weston_module(root: Path, relative: str) -> Path:
    # AGENT-GUARD: Gentoo uses lib64; private tool prefixes may use lib. Keep
    # each module inside the authenticated prefix and reject poisoned candidates
    # rather than silently falling through to another library directory.
    for library in ("lib", "lib64"):
        candidate = root / library / relative
        if not candidate.exists() and not candidate.is_symlink():
            continue
        try:
            resolved = candidate.resolve(strict=True)
        except (OSError, RuntimeError) as error:
            raise SandboxContractError(
                f"Weston module is unavailable: {candidate}"
            ) from error
        if not resolved.is_file() or root not in resolved.parents:
            raise SandboxContractError(
                f"Weston module is outside its tool prefix or not a file: {candidate}"
            )
        return resolved
    raise SandboxContractError(
        f"Weston module is unavailable below {root}/lib or lib64: {relative}"
    )


def weston_module_map(weston: Path) -> str:
    root = tool_root(weston)
    modules = (
        ("headless-backend.so", "libweston-15/headless-backend.so"),
        ("kiosk-shell.so", "weston/kiosk-shell.so"),
    )
    return ";".join(
        f"{name}={PurePosixPath(str(_weston_module(root, relative)))}"
        for name, relative in modules
    )
