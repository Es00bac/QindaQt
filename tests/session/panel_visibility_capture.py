# SPDX-License-Identifier: GPL-3.0-or-later
"""Command boundary for mixed-ABI panel framebuffer capture."""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Mapping

from desktop_session_sandbox import SandboxContractError


def visibility_probe_command(
    arguments: argparse.Namespace, parent_environment: Mapping[str, str],
) -> list[str]:
    """Bind a capture child to the authenticated parent Weston loader path."""

    library_path = parent_environment.get("LD_LIBRARY_PATH", "")
    entries = library_path.split(":")
    if not library_path or any(
        not entry or not Path(entry).is_absolute() for entry in entries
    ):
        raise SandboxContractError(
            "parent Weston library path is not an exact absolute search path"
        )
    # AGENT-CONTRACT: The C++ probe stays on the system-KWin application
    # environment. Its screenshot child alone receives the private parent's
    # loader path, preventing either side of the mixed-ABI proof from borrowing
    # the other's libweston/libkwin closure.
    command = [
        str(arguments.visibility_probe),
        str(arguments.weston_screenshooter),
        library_path,
    ]
    control_file = getattr(arguments, "visibility_control_file", None)
    if control_file is not None:
        control_path = Path(control_file)
        if not control_path.is_absolute():
            raise SandboxContractError("panel control file path must be absolute")
        command.extend(("--control-file", str(control_path)))
    return command
