# SPDX-License-Identifier: GPL-3.0-or-later
"""Canonical notification-shell evidence shared by desktop session units."""

from __future__ import annotations


def closed_notification_shell(shell_pid: int, output_name: str) -> dict[str, object]:
    return {
        "status": "ok",
        "evidence": {
            "owner": ":1.20",
            "servicePid": str(shell_pid),
            "shellPid": str(shell_pid),
            "tokens": {
                "ready": True,
                "qstRevision": 1,
                "generation": "1",
                "sourceThemeId": "qinda-dark",
                "backgroundBase": "#171a18",
            },
            "taskList": {
                "phase": "ready",
                "generation": "1",
                "windowCount": 2,
            },
            "presentation": {
                "privatePresentationAllowed": True, "centerOpen": False,
            },
            "centerOpenedCount": "0",
            "centerWindow": {
                "exists": True, "visible": False, "outputName": output_name,
            },
        },
    }
