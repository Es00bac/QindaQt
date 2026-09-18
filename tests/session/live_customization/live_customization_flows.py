# SPDX-License-Identifier: GPL-3.0-or-later
"""The live-customization nested flows: what each row does and judges.

`menu` drives every Meta+right-click menu through the private seat (panel,
applet and desktop menus; add, move, remove, undo) and replays the same
intents through the Settings route's own editor host for the parity verdict.
`editmode` enters edit mode, drags an applet onto another panel, leaves edit
mode and undoes, again with a parity replay. Keyboard positions follow the
menus' documented entry order.
"""

from __future__ import annotations

import configparser
import os
import time
from pathlib import Path

from live_customization_session import LiveSession
from shade_control import wait_for

# Panel menu entries (PanelCustomizeMenu.qml): keyboard positions and count.
PANEL_ADD_APPLET, PANEL_EDIT_MODE, PANEL_UNDO, PANEL_COUNT = 0, 4, 5, 8
# Applet menu entries (AppletCustomizeMenu.qml).
APPLET_MOVE_END, APPLET_REMOVE, APPLET_COUNT = 2, 6, 8
# Desktop menu entries (DesktopCustomizeMenu.qml); Add panel has four edges.
DESKTOP_ADD_PANEL, DESKTOP_UNDO, DESKTOP_COUNT, DESKTOP_EDGES = 0, 4, 6, 4
# PanelContent's along-axis inset plus a margin into the first chip.
FIRST_CHIP_INSET = 20.0


def chord_seeded(session: LiveSession) -> None:
    parser = configparser.ConfigParser(strict=False, interpolation=None)
    parser.optionxform = str  # type: ignore[assignment]
    parser.read(Path(os.environ["XDG_CONFIG_HOME"]) / "kwinrc", encoding="utf-8")
    seeded = parser.get("MouseBindings", "CommandAll3", fallback="")
    session.step("kwinrc", commandAll3=seeded)
    session.verdict("chord-seeded-command-all-3-nothing", seeded == "Nothing")


def empty_bar_point(session: LiveSession) -> tuple[float, float]:
    bar = session.panel_rects()["bar"]
    return (bar["x"] + bar["width"] * 0.8, bar["y"] + bar["height"] / 2)


def first_chip_point(session: LiveSession, role: str = "bar") -> tuple[float, float]:
    rect = session.panel_rects()[role]
    return (rect["x"] + FIRST_CHIP_INSET, rect["y"] + rect["height"] / 2)


def run_menu(session: LiveSession) -> None:
    chord_seeded(session)
    before = session.capture("before")
    profile = session.profile()
    session.verdict("user-store-absent-before-any-action", session.profile_bytes() is None)

    # Panel menu > Add applet > first palette entry.
    session.chord(*empty_bar_point(session))
    opened = session.capture_still("panel-menu")
    session.verdict("panel-menu-capture-differs", opened.sha256 != before.sha256)
    session.menu_select(PANEL_ADD_APPLET, PANEL_COUNT, sub_index=-1, capture_name="panel-menu-keyed")
    profile = session.wait_profile_change("profile after add applet")
    bar_ids = session.applet_ids(profile, "bar")
    session.verdict("add-applet-persisted", len(bar_ids) == 3 and bar_ids[:2] == ["launcher", "clock"])
    added = session.panel(profile, "bar")["applets"][-1] if len(bar_ids) == 3 else {}
    session.step("added-applet", applet=added)

    # Applet menu on the first chip (the launcher) > Move to end.
    session.chord(*first_chip_point(session))
    applet_menu = session.capture_still("applet-menu")
    session.verdict("applet-menu-capture-differs", applet_menu.sha256 != before.sha256)
    session.menu_select(APPLET_MOVE_END, APPLET_COUNT, capture_name="applet-menu-keyed")
    profile = session.wait_profile_change("profile after move to end")
    session.verdict("move-to-end-persisted",
                    session.applet_zone(profile, "bar", "launcher") == "end"
                    and session.applet_ids(profile, "bar") == bar_ids)

    # Applet menu on the (now first) clock chip > Remove.
    session.chord(*first_chip_point(session))
    session.menu_select(APPLET_REMOVE, APPLET_COUNT)
    profile = session.wait_profile_change("profile after remove")
    session.step("bar-after-remove", ids=session.applet_ids(profile, "bar"),
                 zones={applet["id"]: applet.get("settings", {}).get("zone")
                        for applet in session.panel(profile, "bar")["applets"]})
    session.verdict("remove-applet-persisted",
                    session.applet_ids(profile, "bar") == ["launcher", added.get("id")])

    # Panel menu > Undo restores the clock in one step.
    session.chord(*empty_bar_point(session))
    session.menu_select(PANEL_UNDO, PANEL_COUNT)
    profile = session.wait_profile_change("profile after undo")
    session.verdict("undo-persisted", session.applet_ids(profile, "bar") == bar_ids
                    and session.applet_zone(profile, "bar", "launcher") == "end")

    # Desktop menu > Add panel > At the top, then Undo from the desktop menu.
    width, height = session.config.logical_size
    session.chord(width / 2, height / 2)
    session.capture("desktop-menu")
    session.menu_select(DESKTOP_ADD_PANEL, DESKTOP_COUNT, sub_index=0, sub_count=DESKTOP_EDGES)
    profile = session.wait_profile_change("profile after desktop add panel")
    new_panel = session.panel(profile, "panel-1")
    session.verdict("desktop-add-panel-persisted",
                    len(profile.get("panels", [])) == 3 and (new_panel or {}).get("edge") == "top")
    try:
        wait_for("third panel mapped", lambda: len(session.mapped_panels()) >= 3, 20)
        session.verdict("desktop-add-panel-mapped", True)
    except RuntimeError:
        session.step("surfaces-after-add-panel",
                     surfaces=session.control.call("DevelopmentShellSurfaces"))
        session.verdict("desktop-add-panel-mapped", False)
    session.capture("after-add-panel")
    session.chord(width / 2, height / 2)
    session.menu_select(DESKTOP_UNDO, DESKTOP_COUNT)
    profile = session.wait_profile_change("profile after desktop undo")
    session.verdict("desktop-undo-persisted", len(profile.get("panels", [])) == 2)
    session.capture("after")

    # Parity: the same intents through the Settings route's own host.
    launcher_settings = {"zone": "end"}
    session.parity("menu", [
        {"kind": "insert", "panel": "bar", "zone": "start", "plugin": added.get("plugin"),
         "instanceId": added.get("id")},
        {"kind": "apply"},
        {"kind": "appletSettings", "panel": "bar", "applet": "launcher", "zone": "end",
         "settings": launcher_settings},
        {"kind": "apply"},
        {"kind": "remove", "panel": "bar", "applet": "clock"},
        {"kind": "apply"},
        {"kind": "undo"}, {"kind": "apply"},
        {"kind": "addPanel", "panel": "panel-1", "panelId": "panel-1", "edge": "top"},
        {"kind": "apply"},
        {"kind": "undo"}, {"kind": "apply"},
    ])


def run_editmode(session: LiveSession) -> None:
    chord_seeded(session)
    before = session.capture("before")
    rects = session.panel_rects()

    # Panel menu > Enter edit mode: handles and the Done/Undo bar appear.
    session.chord(*empty_bar_point(session))
    session.menu_select(PANEL_EDIT_MODE, PANEL_COUNT)
    time.sleep(0.8)
    editing = session.capture("edit-mode")
    session.verdict("edit-mode-capture-differs", editing.sha256 != before.sha256)

    # Drag the launcher chip from the bar onto the tray's end third.
    tray = rects["tray"]
    session.drag(first_chip_point(session, "bar"),
                 (tray["x"] + tray["width"] * 0.9, tray["y"] + tray["height"] / 2))
    profile = session.wait_profile_change("profile after drag")
    session.verdict("drag-to-other-panel-persisted",
                    session.applet_ids(profile, "bar") == ["clock"]
                    and session.applet_ids(profile, "tray") == ["tray-clock", "launcher"]
                    and session.applet_zone(profile, "tray", "launcher") == "end")
    session.capture("after-drag")

    # Tray menu > Exit edit mode (keyboard parity for the Done button).
    session.chord(tray["x"] + tray["width"] * 0.3, tray["y"] + tray["height"] / 2)
    session.menu_select(PANEL_EDIT_MODE, PANEL_COUNT)
    time.sleep(0.8)
    after = session.capture("after")
    session.verdict("exit-edit-mode-capture-differs", after.sha256 != editing.sha256)

    # Tray menu > Undo restores the launcher to the bar.
    session.chord(tray["x"] + tray["width"] * 0.3, tray["y"] + tray["height"] / 2)
    session.menu_select(PANEL_UNDO, PANEL_COUNT)
    profile = session.wait_profile_change("profile after undo")
    session.verdict("undo-drag-persisted",
                    session.applet_ids(profile, "bar") == ["launcher", "clock"]
                    and session.applet_ids(profile, "tray") == ["tray-clock"])

    session.parity("editmode", [
        {"kind": "move", "panel": "bar", "applet": "launcher", "targetPanel": "tray", "zone": "end"},
        {"kind": "apply"},
        {"kind": "undo"}, {"kind": "apply"},
    ])


FLOWS = {"menu": run_menu, "editmode": run_editmode}
