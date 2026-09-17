# SPDX-License-Identifier: GPL-3.0-or-later
"""An iconified window's chip is a Meta+Shift+Left dock source (ADR-0191).

Order: group two client-side-decorated windows; roll a server-decorated
window up and drop its chip on a member's edge (the window unrolls and joins
the container as a split); roll another up and drop its chip on a member
tile's centre (a new tab); with the backdrop gone, drop a third chip on the
bare desktop (no target: the chip stays at the drop point) and Escape-cancel
a fourth drag (the chip stays where it was).
"""

from __future__ import annotations

import time

from iconify_control import (WHEEL_AWAY, approximately, chip_center, chip_of, iconified_windows,
                             uncovered_desktop_point, wait_iconified, wheel)
from shade_control import (button, content_point, dock_drop_point, key, pointer, rect, title_point,
                           united, wait_for)
from shade_fixtures import MEMBER_B_COLOUR, MEMBER_B_TITLE, MEMBER_C_COLOUR, MEMBER_C_TITLE
from shade_framebuffer import pixel_matches
from shade_session import ShadeSession, window_summary

SPLIT_TITLE = "Iconify dock split"
SPLIT_COLOUR = "#b03a2e"
TAB_TITLE = "Iconify dock tab"
TAB_COLOUR = "#8a2eb0"
FREE_TITLE = "Iconify dock free"
FREE_COLOUR = "#2e8ab0"


def run(session: ShadeSession) -> None:
    control, pointer_driver = session.control, session.pointer
    session.fixtures.gtk(MEMBER_B_TITLE, MEMBER_B_COLOUR, "csd", 480, 360)
    session.fixtures.gtk(MEMBER_C_TITLE, MEMBER_C_COLOUR, "csd", 520, 400)
    wait_for("members mapped", lambda: all(
        title in control.windows() for title in (MEMBER_B_TITLE, MEMBER_C_TITLE)), 30)
    time.sleep(1.0)
    session.wait_frames_settled([MEMBER_B_TITLE, MEMBER_C_TITLE])
    session.dock(MEMBER_C_TITLE, MEMBER_B_TITLE)

    def grouped():
        current = control.windows()
        owner = current[MEMBER_B_TITLE].get("containerId")
        return current if owner and owner == current[MEMBER_C_TITLE].get("containerId") else None

    wait_for("members grouped", grouped, 8)
    session.wait_frames_settled([MEMBER_B_TITLE, MEMBER_C_TITLE])
    session.capture("D01-grouped")

    # 1. Chip dropped on a member's edge: the window unrolls and splits in.
    split_id = _map_and_iconify(session, SPLIT_TITLE, SPLIT_COLOUR)
    inventory = control.windows()
    container_id = inventory[MEMBER_C_TITLE]["containerId"]
    chip = chip_of(session, split_id)
    drop = dock_drop_point(inventory, SPLIT_TITLE, MEMBER_C_TITLE)
    if chip is None or drop is None:
        raise RuntimeError("no chip or no uncovered edge drop point on member C")
    session.step("chip-edge-drop", chip=chip, drop=list(drop))
    pointer_driver.drag(chip_center(chip), drop, meta_shift=True)
    wait_for("split window joined the container", lambda: control.windows().get(
        SPLIT_TITLE, {}).get("containerId") == container_id and not control.windows()[
        SPLIT_TITLE]["hidden"], 8)
    session.wait_frames_settled([SPLIT_TITLE, MEMBER_C_TITLE])
    inventory = control.windows()
    frame = session.capture("D02-docked-split")
    session.step("docked-split", split=window_summary(inventory[SPLIT_TITLE]),
                 c=window_summary(inventory[MEMBER_C_TITLE]), iconified=iconified_windows(session))
    session.verdict("chipDocksAsEdgeSplit",
                    inventory[SPLIT_TITLE]["containerId"] == container_id
                    and not inventory[SPLIT_TITLE]["hidden"])
    session.verdict("chipDockLeavesNoChip", not iconified_windows(session))
    session.verdict("chipDockRestoresContent",
                    pixel_matches(frame, content_point(inventory[SPLIT_TITLE]["targetGeometry"]),
                                  SPLIT_COLOUR))
    session.verdict("chipDockSplitsBesideTarget",
                    inventory[SPLIT_TITLE]["targetGeometry"]
                    != inventory[MEMBER_C_TITLE]["targetGeometry"])

    # 2. Chip dropped on a member tile's centre: a new tab spanning the page.
    tab_id = _map_and_iconify(session, TAB_TITLE, TAB_COLOUR)
    inventory = control.windows()
    page = united([inventory[title]["targetGeometry"]
                   for title in inventory if inventory[title].get("containerId") == container_id
                   and not inventory[title]["hidden"]])
    chip = chip_of(session, tab_id)
    tile = rect(inventory[MEMBER_C_TITLE]["targetGeometry"])
    centre = (tile[0] + tile[2] / 2, tile[1] + tile[3] / 2)
    if chip is None:
        raise RuntimeError("no chip for the tab drop")
    session.step("chip-tab-drop", chip=chip, drop=list(centre), page=page)
    pointer_driver.drag(chip_center(chip), centre, meta_shift=True)
    wait_for("tab window joined the container", lambda: control.windows().get(
        TAB_TITLE, {}).get("containerId") == container_id and not control.windows()[
        TAB_TITLE]["hidden"], 8)
    session.wait_frames_settled([TAB_TITLE])
    inventory = control.windows()
    session.capture("D03-docked-tab")
    session.step("docked-tab", tab=window_summary(inventory[TAB_TITLE]), page=page,
                 iconified=iconified_windows(session))
    session.verdict("chipDocksAsTab",
                    inventory[TAB_TITLE]["containerId"] == container_id
                    and approximately(rect(inventory[TAB_TITLE]["targetGeometry"]), page, 4.0))
    session.verdict("chipTabDockLeavesNoChip", not iconified_windows(session))

    # 3. No target under the drop: the chip stays at the drop point. The
    # maximized backdrop would itself be a dock target, so it leaves first.
    session._backdrop.terminate()  # noqa: SLF001 - the flow owns this session
    wait_for("backdrop gone", lambda: "Shade backdrop" not in control.windows(), 8)
    free_id = _map_and_iconify(session, FREE_TITLE, FREE_COLOUR)
    inventory = control.windows()
    chip = chip_of(session, free_id)
    target = uncovered_desktop_point(inventory, session.config.logical_size)
    if chip is None or target is None:
        raise RuntimeError("no chip or no bare desktop point for the free drop")
    session.step("chip-free-drop", chip=chip, drop=list(target))
    pointer_driver.drag(chip_center(chip), target, meta_shift=True)
    time.sleep(0.6)
    moved = chip_of(session, free_id)
    inventory = control.windows()
    session.capture("D04-free-drop")
    session.step("free-drop", chip=moved, window=window_summary(inventory[FREE_TITLE]))
    session.verdict("dropOutsideTargetsKeepsIconified",
                    moved is not None and bool(inventory[FREE_TITLE]["hidden"]))
    session.verdict("dropOutsideTargetsMovesChip",
                    moved is not None and abs(chip_center(moved)[0] - target[0]) <= 3.0
                    and abs(chip_center(moved)[1] - target[1]) <= 3.0)
    if moved is None:
        raise RuntimeError("the free drop lost the chip")

    # 4. Escape cancels an exact-chord chip drag: nothing moves.
    start = chip_center(moved)
    control.inject([pointer(*start)])
    time.sleep(0.05)
    control.inject([key("left-meta", True), key("left-shift", True)])
    control.inject([button("left", True)])
    for index in range(1, 9):
        control.inject([pointer(start[0] - 20.0 * index, start[1] + 12.0 * index)])
        time.sleep(0.02)
    control.inject([key("escape", True), key("escape", False)])
    time.sleep(0.1)
    control.inject([button("left", False)])
    control.inject([key("left-shift", False), key("left-meta", False)])
    time.sleep(0.5)
    after_escape = chip_of(session, free_id)
    inventory = control.windows()
    session.capture("D05-escape-cancelled")
    session.step("escape-cancelled", chip=after_escape, window=window_summary(inventory[FREE_TITLE]))
    session.verdict("escapeCancelsChipDrag", approximately(after_escape, moved, 1.0))
    session.verdict("escapeKeepsIconified", bool(inventory[FREE_TITLE]["hidden"]))


def _map_and_iconify(session: ShadeSession, title: str, colour: str) -> str:
    control = session.control
    session.fixtures.gtk(title, colour, "ssd", 460, 320)
    wait_for(f"{title} mapped", lambda: title in control.windows(), 30)
    time.sleep(0.8)
    session.wait_frames_settled([title])
    inventory = control.windows()
    window_id = inventory[title]["id"]
    point = title_point(inventory, title)
    if point is None:
        # KWin mapped it under another window: raise it with a content click.
        session.activate(title)
        inventory = control.windows()
        point = title_point(inventory, title)
    if point is None:
        raise RuntimeError(f"no uncovered title-bar point on {title}")
    wheel(session, point, WHEEL_AWAY)
    wait_iconified(session, window_id)
    session.step(f"iconified-{title}", window=window_summary(control.windows()[title]),
                 chip=chip_of(session, window_id))
    return window_id
