# SPDX-License-Identifier: GPL-3.0-or-later
"""Shared fail-closed validation for visible shell-polish evidence."""

from __future__ import annotations

from typing import Any, Mapping


def validate_task_list(
    value: Any, *, expected_applications: set[str] | None = None,
    expected_count: int | None = None,
) -> None:
    if (
        not isinstance(value, Mapping)
        or set(value) != {"phase", "generation", "windowCount", "buttons"}
        or value.get("phase") != "ready"
        or not isinstance(value.get("generation"), str)
        or not value["generation"].isascii()
        or not value["generation"].isdecimal()
        or str(int(value["generation"], 10)) != value["generation"]
        or int(value["generation"], 10) <= 0
        or not isinstance(value.get("windowCount"), int)
        or isinstance(value.get("windowCount"), bool)
        or value["windowCount"] < 1
        or not isinstance(value.get("buttons"), list)
        or len(value["buttons"]) != value["windowCount"]
    ):
        raise ValueError("task list is not ready")
    if expected_count is not None and value["windowCount"] != expected_count:
        raise ValueError("task list has unexpected visible window count")
    buttons = value["buttons"]
    if any(
        not isinstance(button, Mapping)
        or set(button) != {"applicationId", "iconName", "iconResolved"}
        or not isinstance(button.get("applicationId"), str)
        or not button["applicationId"]
        or button["applicationId"] == "qindaqt-shell"
        or not isinstance(button.get("iconName"), str)
        or not button["iconName"]
        or button.get("iconResolved") is not True
        for button in buttons
    ):
        raise ValueError("task button icon is unresolved or shell-owned")
    if expected_applications is not None and {
        button["applicationId"] for button in buttons
    } != expected_applications:
        raise ValueError("task list applications do not match the scenario")


def validate_quieting(value: Any) -> None:
    if value != {
        "enabled": False,
        "hasBaseline": True,
        "state": "ready",
        "canToggle": True,
        "statusText": "",
        "errorText": "",
    }:
        raise ValueError("notification quieting is not ready and off")


def validate_panel_applets(value: Any, *, require_qindaqt_shelf: bool) -> None:
    if not isinstance(value, list):
        raise ValueError("panel applets are malformed")
    task_lists_by_panel: dict[str, int] = {}
    launcher_count = 0
    task_list_count = 0
    for applet in value:
        if not isinstance(applet, Mapping) or set(applet) != {
            "panelId", "appletId", "plugin", "ready", "entryPoint",
        }:
            raise ValueError("panel applet shape is malformed")
        plugin = applet.get("plugin")
        if plugin in {
            "application-launcher", "grouped-task-list", "dock-task-list",
            "centered-task-list",
        }:
            raise ValueError("legacy panel applet alias was not normalized")
        if plugin == "launcher" and (
            applet.get("ready") is not True
            or applet.get("entryPoint") != "qindaqt.applets.launcher"
        ):
            raise ValueError("launcher applet is unavailable")
        if plugin == "launcher":
            launcher_count += 1
        if plugin == "task-list":
            panel_id = applet.get("panelId")
            if not isinstance(panel_id, str) or not panel_id:
                raise ValueError("task-list panel identity is malformed")
            task_lists_by_panel[panel_id] = task_lists_by_panel.get(panel_id, 0) + 1
            if task_lists_by_panel[panel_id] > 1:
                raise ValueError("panel contains duplicate task lists")
            if (
                applet.get("ready") is not True
                or applet.get("entryPoint") != "qindaqt.applets.task-list"
            ):
                raise ValueError("task-list applet is unavailable")
            task_list_count += 1
    if launcher_count == 0 or task_list_count == 0:
        raise ValueError("panel controls are incomplete")
    if not require_qindaqt_shelf:
        return
    shelf = [applet for applet in value if applet.get("panelId") == "smart-shelf"]
    if (
        len([applet for applet in shelf if applet.get("plugin") == "launcher"]) != 1
        or len([applet for applet in shelf if applet.get("plugin") == "task-list"]) != 1
    ):
        raise ValueError("smart shelf applet composition is malformed")
