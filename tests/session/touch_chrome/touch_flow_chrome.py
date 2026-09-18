# SPDX-License-Identifier: GPL-3.0-or-later
"""Fingers on container chrome (ADR-0193), judged by inventory and pixels.

Two client-side-decorated members are grouped with a real Meta+Shift drag;
then the development input device drives fingers: a tap on the inactive tab
activates it, a finger held on the title row opens the group menu (proved by
choosing Roll up from it), a title drag moves the whole container, and
two-finger swipes roll the container up and back down.
"""

from __future__ import annotations

import time
from typing import Any

from shade_control import (ROLL_UP_MENU_INDEX, content_point, key, united, wait_for)
from shade_fixtures import BACKDROP_TITLE, MEMBER_A_COLOUR, MEMBER_A_TITLE
from shade_session import ShadeSession, window_summary

LONG_PRESS_HOLD = 0.9
SWIPE_STEPS = 8


def touch_down(finger: int, x: float, y: float) -> dict[str, Any]:
    return {"type": "touch-down", "id": finger, "x": float(x), "y": float(y)}


def touch_motion(finger: int, x: float, y: float) -> dict[str, Any]:
    return {"type": "touch-motion", "id": finger, "x": float(x), "y": float(y)}


def touch_up(finger: int) -> dict[str, Any]:
    return {"type": "touch-up", "id": finger}


class FingerDriver:
    """Real touch contacts through the development input device."""

    def __init__(self, session: ShadeSession) -> None:
        self._control = session.control

    def tap(self, point: tuple[float, float]) -> None:
        self._control.inject([touch_down(1, *point)])
        time.sleep(0.08)
        self._control.inject([touch_up(1)])
        time.sleep(0.4)

    def hold(self, point: tuple[float, float], seconds: float = LONG_PRESS_HOLD) -> None:
        self._control.inject([touch_down(1, *point)])
        time.sleep(seconds)
        self._control.inject([touch_up(1)])
        time.sleep(0.3)

    def drag(self, start: tuple[float, float], end: tuple[float, float]) -> None:
        self._control.inject([touch_down(1, *start)])
        time.sleep(0.05)
        for index in range(1, 13):
            progress = index / 12
            self._control.inject([touch_motion(1, start[0] + (end[0] - start[0]) * progress,
                                               start[1] + (end[1] - start[1]) * progress)])
            time.sleep(0.02)
        self._control.inject([touch_up(1)])
        time.sleep(0.5)

    def two_finger_swipe(self, point: tuple[float, float], dy: float) -> None:
        second = (point[0] + 60.0, point[1])
        self._control.inject([touch_down(1, *point)])
        time.sleep(0.05)
        self._control.inject([touch_down(2, *second)])
        time.sleep(0.05)
        for index in range(1, SWIPE_STEPS + 1):
            progress = index / SWIPE_STEPS
            self._control.inject([touch_motion(2, second[0], second[1] + dy * progress)])
            time.sleep(0.02)
        self._control.inject([touch_up(2)])
        time.sleep(0.05)
        self._control.inject([touch_up(1)])
        time.sleep(0.6)


def _container(session: ShadeSession, container_id: str) -> dict[str, Any] | None:
    """The hybrid process's entry for the container from the Containers call.

    The control bridge lists the same id without geometry; the hybrid entry
    carries the published chrome plan (outer frame, title row, tabs).
    """
    reply = session.control.call("Containers")
    if reply.get("status") != "ok":
        raise RuntimeError(f"Containers failed: {reply}")
    for entry in reply.get("containers") or []:
        if entry.get("id") == container_id and entry.get("authority") == "hybrid-process":
            return entry
    return None


def _shaded_count(session: ShadeSession) -> int:
    return int(session.hybrid_summary().get("shadedContainerCount") or 0)


def _rect_center(rect: dict[str, float]) -> tuple[float, float]:
    return (rect["x"] + rect["width"] / 2.0, rect["y"] + rect["height"] / 2.0)


def _free_row_x(bar: dict[str, float], tabs: list[dict[str, Any]]) -> float:
    """An x on the title row clear of tabs and controls: where a finger drags.

    Group controls sit at the row's left end and window buttons at its right
    (neither is published), so the free stretch is just right of the last
    tab, falling back to the row's 30 % point the pointer flows use.
    """
    right_edge = max((tab["x"] + tab["width"] for tab in tabs), default=bar["x"])
    candidate = right_edge + 28.0
    if candidate < bar["x"] + bar["width"] - 140.0:
        return candidate
    return bar["x"] + bar["width"] * 0.3


def _group_members(session: ShadeSession) -> tuple[str, str, Any]:
    control = session.control
    session.fixtures.gtk(MEMBER_A_TITLE, MEMBER_A_COLOUR, "csd", 640, 420)
    member = session.fixtures.member_b(session.config.kind)

    def find_member(inventory):
        for title, window in inventory.items():
            if title not in (MEMBER_A_TITLE, BACKDROP_TITLE) and member.matches(title):
                return window
        return None

    wait_for("both members mapped",
             lambda: MEMBER_A_TITLE in control.windows() and find_member(control.windows()), 90)
    time.sleep(3.0)
    inventory = control.windows()
    title_b = find_member(inventory)["title"]
    session.step("mapped", a=window_summary(inventory[MEMBER_A_TITLE]),
                 b=window_summary(inventory[title_b]))
    session.capture("01-mapped")
    session.dock(MEMBER_A_TITLE, title_b)

    def grouped():
        current = control.windows()
        owner = current[MEMBER_A_TITLE].get("containerId")
        return current if owner and owner == current[title_b].get("containerId") else None

    inventory = wait_for("members grouped", grouped, 8)
    time.sleep(0.8)
    container_id = inventory[MEMBER_A_TITLE]["containerId"]
    session.pointer.click(*content_point(control.windows()[title_b]["targetGeometry"]))
    wait_for("member B active", lambda: control.windows()[title_b]["active"], 5)
    session.capture("02-grouped")
    return container_id, title_b, grouped


def _tap_inactive_tab(session: ShadeSession, fingers: FingerDriver, container_id: str,
                      title_b: str) -> None:
    # Two docked members form one split page with one tab named after its
    # representative member ("<title> +1"). Make the other member active by
    # a real click, then tap the tab: the tap activates the page, which
    # brings its representative back to the front.
    entry = wait_for("container geometry published",
                     lambda: (c := _container(session, container_id)) and c.get("tabs") and c, 5)
    session.step("chrome", container=entry)
    session.verdict("chromePublishesTabsAndFrame",
                    bool(entry.get("outerFrame")) and bool(entry.get("outerTitleBar"))
                    and len(entry["tabs"]) >= 1)
    tab = entry["tabs"][0]
    representative = tab["title"].split(" +")[0]
    other = title_b if representative == MEMBER_A_TITLE else MEMBER_A_TITLE
    session.pointer.click(*content_point(session.control.windows()[other]["targetGeometry"]))
    wait_for("other member active", lambda: session.control.windows()[other]["active"], 5)
    tab_point = _rect_center(tab)
    fingers.tap(tab_point)
    activated = wait_for("tap activated the tab's page",
                         lambda: session.control.windows()[representative]["active"] or None, 5)
    session.step("tapped-tab", point=tab_point, representative=representative, other=other,
                 activeAfter=session.active_titles())
    session.verdict("tapActivatesTab", activated is True)
    session.capture("03-tapped")


def _hold_title_for_menu(session: ShadeSession, fingers: FingerDriver, container_id: str) -> None:
    # A finger held on the title row opens the group menu: choosing "Roll
    # up" from it rolls the container up, which no tap could do.
    # A tab owns the container menu (a right click there opens it), so the
    # finger holds on the tab; the row's controls and buttons do not.
    entry = _container(session, container_id)
    hold_point = _rect_center(entry["tabs"][0])
    fingers.hold(hold_point)
    time.sleep(0.2)
    session.capture("04a-held-menu")
    # The same key walk the pointer flow uses on the right-click menu: the
    # first Down highlights entry 0, so entry N takes N+1 presses.
    for _ in range(ROLL_UP_MENU_INDEX + 1):
        session.control.inject([key("down", True), key("down", False)])
        time.sleep(0.1)
    session.control.inject([key("enter", True), key("enter", False)])
    time.sleep(0.5)
    rolled = wait_for("menu rolled the container up", lambda: _shaded_count(session) == 1, 6)
    session.step("held-title", point=hold_point, shaded=rolled)
    session.verdict("longPressOpensGroupMenu", rolled == 1)
    session.capture("04-rolled-up-by-menu")


def _swipe_strip_down(session: ShadeSession, fingers: FingerDriver) -> None:
    strips = session.hybrid_summary().get("shadedStripFrames") or []
    session.verdict("stripPublishedWhileShaded", bool(strips))
    strip = strips[0]
    strip_point = (strip["x"] + strip["width"] * 0.3, strip["y"] + strip["height"] / 2.0)
    fingers.two_finger_swipe(strip_point, +70.0)
    wait_for("swipe down unrolled", lambda: _shaded_count(session) == 0, 6)
    session.verdict("twoFingerSwipeDownUnrolls", _shaded_count(session) == 0)
    session.capture("05-unrolled-by-swipe")


def _drag_title(session: ShadeSession, fingers: FingerDriver, container_id: str,
                title_b: str, grouped) -> tuple[float, float]:
    control = session.control
    entry = _container(session, container_id)
    before_frames = {t: control.windows()[t]["targetGeometry"] for t in (MEMBER_A_TITLE, title_b)}
    bar = entry["outerTitleBar"]
    drag_start = (_free_row_x(bar, entry["tabs"]), bar["y"] + bar["height"] / 2.0)
    delta = (160.0, 90.0)
    drag_end = (drag_start[0] + delta[0], drag_start[1] + delta[1])
    fingers.drag(drag_start, drag_end)

    def moved():
        current = control.windows()
        shifts = []
        for title in (MEMBER_A_TITLE, title_b):
            before_geometry, after_geometry = before_frames[title], current[title]["targetGeometry"]
            shifts.append((after_geometry["x"] - before_geometry["x"],
                           after_geometry["y"] - before_geometry["y"]))
        return shifts if all(abs(dx - delta[0]) <= 6 and abs(dy - delta[1]) <= 6
                             for dx, dy in shifts) else None

    shifts = wait_for("container moved by the drag", moved, 6)
    session.step("dragged-title", start=drag_start, end=drag_end, shifts=shifts,
                 unitedBefore=united(list(before_frames.values())))
    session.verdict("touchDragMovesContainer", shifts is not None)
    session.verdict("membersStayGroupedAfterDrag", grouped() is not None)
    session.capture("06-dragged")
    return drag_end


def _swipe_title_up(session: ShadeSession, fingers: FingerDriver, container_id: str,
                    drag_end: tuple[float, float]) -> None:
    bar = _container(session, container_id)["outerTitleBar"]
    swipe_point = (drag_end[0], bar["y"] + bar["height"] / 2.0)
    fingers.two_finger_swipe(swipe_point, -70.0)
    wait_for("swipe up rolled the container up", lambda: _shaded_count(session) == 1, 6)
    session.verdict("twoFingerSwipeUpRollsUp", _shaded_count(session) == 1)
    session.capture("07-rolled-up-by-swipe")


def _attempt(session: ShadeSession, name: str, verdicts: tuple[str, ...], step) -> Any:
    """Run one step; a failure records its verdicts false and the flow goes on,
    so one session yields evidence for every gesture."""
    try:
        return step()
    except Exception as error:  # noqa: BLE001 - the verdict carries the failure
        session.step(f"{name}-failed", error=str(error))
        for verdict in verdicts:
            session.verdict(verdict, False)
        return None


def run(session: ShadeSession) -> None:
    fingers = FingerDriver(session)
    container_id, title_b, grouped = _group_members(session)
    _attempt(session, "tap", ("chromePublishesTabsAndFrame", "tapActivatesTab"),
             lambda: _tap_inactive_tab(session, fingers, container_id, title_b))
    _attempt(session, "hold", ("longPressOpensGroupMenu",),
             lambda: _hold_title_for_menu(session, fingers, container_id))
    if _shaded_count(session) != 1:
        # Without the roll-up the strip swipe has nothing to unroll: roll up
        # through the pointer path so the remaining gestures stay judged.
        session.step("rolled-up-by-pointer-fallback")
        entry = _container(session, container_id)
        session.pointer.group_menu_action(_rect_center(entry["tabs"][0]), ROLL_UP_MENU_INDEX)
        wait_for("fallback rolled the container up", lambda: _shaded_count(session) == 1, 6)
    _attempt(session, "strip-swipe", ("stripPublishedWhileShaded", "twoFingerSwipeDownUnrolls"),
             lambda: _swipe_strip_down(session, fingers))
    if _shaded_count(session) != 0:
        session.step("unrolled-by-pointer-fallback")
        strips = session.hybrid_summary().get("shadedStripFrames") or []
        strip = strips[0]
        session.pointer.group_menu_action(
            (strip["x"] + strip["width"] * 0.3, strip["y"] + strip["height"] / 2.0),
            ROLL_UP_MENU_INDEX)
        wait_for("fallback unrolled the container", lambda: _shaded_count(session) == 0, 6)
    drag_end = _attempt(session, "drag", ("touchDragMovesContainer", "membersStayGroupedAfterDrag"),
                        lambda: _drag_title(session, fingers, container_id, title_b, grouped))
    if drag_end is None:
        entry = _container(session, container_id)
        bar = entry["outerTitleBar"]
        drag_end = (_free_row_x(bar, entry["tabs"]), bar["y"] + bar["height"] / 2.0)
    _attempt(session, "title-swipe", ("twoFingerSwipeUpRollsUp",),
             lambda: _swipe_title_up(session, fingers, container_id, drag_end))
