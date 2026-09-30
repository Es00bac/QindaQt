# SPDX-License-Identifier: GPL-3.0-or-later
"""Native mixed iconified/shaded gather and interactive overflow pager check."""
from __future__ import annotations

import os
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "iconify_visibility"))

from iconify_control import WHEEL_AWAY, WHEEL_TOWARD, chip_center, chip_of, wheel
from shade_control import (ROLL_UP_MENU_INDEX, SHARED_ROW_HEIGHT, badge_title_point,
                           rect, title_point, wait_for)
from shade_fixtures import BACKDROP_TITLE
from shade_session import ShadeSession, window_summary

MAXIMIZED_TITLE = "Gather maximized member"
PEER_TITLE = "Gather group peer"
ICON_TITLES = ("Metadata Fixture", "Fallback Fixture",
               *(f"Gather icon {index}" for index in range(3, 18)))
COLOURS = ("#a83232", "#324da8", "#728632", "#8a4380")


def run(session: ShadeSession) -> None:
    ready = session.config.output / "mock-panel-ready"
    fixture = os.environ.get("GATHERED_PANEL_FIXTURE", "")
    if not fixture:
        raise RuntimeError("nested gathered row requires its committed layer-shell panel fixture")
    panel_log = (session.config.output / "mock-panel.log").open("w", encoding="utf-8")
    panel = subprocess.Popen([fixture, str(ready)], stdout=panel_log, stderr=subprocess.STDOUT)
    try:
        wait_for("mock panel mapped", lambda: ready.exists(), 12)
        _run_flow(session)
    finally:
        panel.terminate()
        try:
            panel.wait(timeout=3)
        except subprocess.TimeoutExpired:
            panel.kill()
            panel.wait(timeout=3)
        panel_log.close()


def _run_flow(session: ShadeSession) -> None:
    control, pointer = session.control, session.pointer
    first = session.fixtures.gtk(MAXIMIZED_TITLE, COLOURS[0], "maximized", 560, 380)
    session.fixtures.gtk(PEER_TITLE, COLOURS[1], "ssd", 560, 380)
    wait_for("maximized group members", lambda: all(
        title in control.windows() for title in (MAXIMIZED_TITLE, PEER_TITLE)), 45)
    time.sleep(0.8)
    inventory = control.windows()
    initial = {title: rect(inventory[title]["geometry"])
               for title in (MAXIMIZED_TITLE, PEER_TITLE)}
    session.step("maximized-origin", members={
        title: window_summary(inventory[title]) for title in initial})
    session.verdict("panelReservedTopMaximizeArea",
                    initial[MAXIMIZED_TITLE][0] <= 1 and initial[MAXIMIZED_TITLE][1] > 0
                    and initial[MAXIMIZED_TITLE][2] >= session.config.logical_size[0] - 2)


    session.dock(MAXIMIZED_TITLE, PEER_TITLE)
    def grouped():
        current = control.windows()
        left = current[MAXIMIZED_TITLE].get("containerId")
        return current if left and left == current[PEER_TITLE].get("containerId") else None
    grouped_inventory = wait_for("maximized member group", grouped, 10)
    member_frames = {title: rect(grouped_inventory[title]["targetGeometry"])
                     for title in (MAXIMIZED_TITLE, PEER_TITLE)}
    left, top, width, _height = united_frames(list(member_frames.values()))
    row_point = (left + width * 0.3, top - SHARED_ROW_HEIGHT / 2)
    pointer.group_menu_action(row_point, ROLL_UP_MENU_INDEX)
    shaded = wait_for("container shaded", lambda: (
        control.windows() if all(control.windows()[t]["hidden"] for t in member_frames) else None), 8)
    session.verdict("maximizedOriginCanShade", all(shaded[t]["hidden"] for t in member_frames))

    icon_ids = []
    icon_processes = []
    for index, title in enumerate(ICON_TITLES, start=2):
        mode = "fullscreen" if title == ICON_TITLES[0] else "ssd"
        application_id = ""
        if title == ICON_TITLES[0]:
            application_id = "org.qindaqt.gather.Metadata"
        elif title == ICON_TITLES[1]:
            application_id = "org.qindaqt.gather.Fallback"
        icon_processes.append(session.fixtures.gtk(
            title, COLOURS[index % len(COLOURS)], mode, 420, 280,
            application_id=application_id))
        wait_for(f"{title} mapped", lambda: title in control.windows(), 30)
        if mode == "fullscreen":
            wait_for(f"{title} entered native fullscreen",
                     lambda: control.windows()[title].get("fullscreen"), 8)
        session.wait_frames_settled([title])
        inventory = control.windows()
        point = title_point(inventory, title)
        if point is None and mode == "fullscreen":
            frame = rect(inventory[title]["geometry"])
            point = (frame[0] + frame[2] / 2, frame[1] + frame[3] / 2)
        if point is None:
            raise RuntimeError(f"no uncovered title point for {title}")
        window_id = inventory[title]["id"]
        if mode == "fullscreen":
            control.inject([
                {"type": "pointer-absolute", "x": point[0], "y": point[1]},
                {"type": "key", "key": "left-meta", "pressed": True},
                {"type": "pointer-axis", "axis": "vertical", "delta": WHEEL_AWAY},
                {"type": "key", "key": "left-meta", "pressed": False},
            ])
        else:
            wheel(session, point, WHEEL_AWAY)
        wait_for(f"{title} iconified", lambda: chip_of(session, window_id), 6)
        if mode == "fullscreen":
            session.verdict("fullscreenOriginParticipatesInGather",
                            bool(control.windows()[title].get("fullscreen"))
                            and chip_of(session, window_id) is not None)
        icon_ids.append(window_id)

    def gathered_state():
        hybrid = control.hybrid()
        pagers = hybrid.get("minimizedGatherPagers") or []
        return hybrid if pagers and pagers[0].get("pageCount", 0) > 1 else None
    time.sleep(1.0)
    session.step("gathered-before-pager-check", hybrid=control.hybrid())
    hybrid = wait_for("mixed gathered pager", gathered_state, 10)
    pager = hybrid["minimizedGatherPagers"][0]
    session.step("mixed-page-zero", pager=pager, chips=hybrid.get("iconifiedWindows"),
                 strips=hybrid.get("shadedStripFrames"), hybrid=hybrid)
    session.capture("mixed-page-zero")
    chips = [entry for entry in hybrid.get("iconifiedWindows", []) if entry.get("chipVisible")]
    fallback_chip = next((entry for entry in chips if entry.get("windowId") == icon_ids[1]), None)
    if fallback_chip is None:
        raise RuntimeError("fallback application identity is not visible on page zero")
    fallback_frame = rect(fallback_chip["chipFrame"])
    session.capture("fallback-hover-label", hover_point=(
        fallback_frame[0] + fallback_frame[2] / 2,
        fallback_frame[1] + fallback_frame[3] / 2))
    strips = hybrid.get("shadedStripFrames") or []
    session.verdict("pagerHasOverflow", pager["pageCount"] > 1)
    session.verdict("firstPageShowsOldestIcon",
                    bool(chips) and chips[0]["windowId"] == icon_ids[0]
                    and len(chips) < len(ICON_TITLES)
                    and hybrid.get("visibleAnchoredChromeSceneItemCount") == 1)
    identities = {entry.get("windowId"): entry for entry in chips}
    session.verdict("metadataAndFallbackAppsBothGathered",
                    icon_ids[0] in identities and icon_ids[1] in identities
                    and control.windows()[ICON_TITLES[0]].get("applicationId")
                    == "org.qindaqt.gather.Metadata"
                    and control.windows()[ICON_TITLES[1]].get("applicationId")
                    == "org.qindaqt.gather.Fallback")
    session.verdict("iconFramesInsideOutput",
                    all(_inside(entry["chipFrame"], session.config.logical_size) for entry in chips))
    session.verdict("topPanelReservedBeforePager",
                    pager["frame"]["y"] > 0 and pager["frame"]["y"] < session.config.logical_size[1])
    session.verdict("shadeWasIncludedInMixedInventory", len(strips) == 1)
    visible_frames = [rect(entry["chipFrame"]) for entry in chips]
    session.verdict("visibleIconsInsideOutput",
                    bool(visible_frames)
                    and all(_inside(entry["chipFrame"], session.config.logical_size) for entry in chips))
    session.verdict("iconPageDoesNotOverlapPager",
                    not _intersects(visible_frames[0], rect(pager["frame"])))
    session.verdict("iconLanePrecedesShadeLane",
                    pager["pageCount"] == 2 and bool(chips) and bool(strips))

    next_button = pager["nextButton"]
    pointer.click(next_button["x"] + next_button["width"] / 2,
                  next_button["y"] + next_button["height"] / 2)
    last = wait_for("last gather page", lambda: _pager_page(control, pager["outputId"], 1), 6)
    last_hybrid = control.hybrid()
    last_strips = last_hybrid.get("shadedStripFrames") or []
    session.step("mixed-last-page", pager=last, chips=last_hybrid.get("iconifiedWindows"),
                 strips=last_strips, hybrid=last_hybrid)
    session.capture("mixed-last-page")
    session.verdict("visiblePagerInputReachedLastPage", last["currentPage"] == last["pageCount"] - 1)
    visible_last_chips = [entry for entry in last_hybrid.get("iconifiedWindows", [])
                          if entry.get("chipVisible")]
    session.verdict("lastPageShowsFinalIcon",
                    last["currentPage"] == last["pageCount"] - 1
                    and any(entry["windowId"] == icon_ids[-1] for entry in visible_last_chips)
                    and all(_inside(entry["chipFrame"], session.config.logical_size)
                            for entry in visible_last_chips)
                    and last_hybrid.get("visibleAnchoredChromeSceneItemCount") == 0)

    previous = last["previousButton"]
    pointer.click(previous["x"] + previous["width"] / 2,
                  previous["y"] + previous["height"] / 2)
    wait_for("first gather page restored", lambda: _pager_page(
        control, pager["outputId"], 0), 6)
    first_chip = chip_of(session, icon_ids[0])
    if first_chip is None:
        raise RuntimeError("icon page lost its first chip")
    wheel(session, chip_center(first_chip), WHEEL_TOWARD)
    fullscreen_restore = wait_for("fullscreen icon restored",
        lambda: control.windows().get(ICON_TITLES[0])
        if icon_ids[0] not in {
            entry.get("windowId") for entry in (control.hybrid().get("iconifiedWindows") or [])}
        and control.windows().get(ICON_TITLES[0], {}).get("fullscreen") else None, 6)
    session.verdict("fullscreenOriginRestoresFullscreen",
                    bool(fullscreen_restore.get("fullscreen")))
    icon_processes[0].terminate()
    wait_for("restored icon fixture closed", lambda: ICON_TITLES[0] not in control.windows(), 10)

    strip_list = wait_for("shaded strip exposed after icon close",
                         lambda: control.hybrid().get("shadedStripFrames") or None, 8)
    session.step("before-unshade", strips=strip_list, hybrid=control.hybrid(),
                 windows={title: window_summary(control.windows().get(title))
                          for title in member_frames})
    pointer.group_menu_action(badge_title_point(strip_list[0]), ROLL_UP_MENU_INDEX)
    restored = wait_for("shaded group restored", lambda: (
        control.windows() if not any(control.windows()[t]["hidden"] for t in member_frames) else None), 8)
    after = {title: rect(restored[title]["targetGeometry"]) for title in member_frames}
    session.verdict("unshadeRestoresGroupedFrames", after == member_frames)
    session.step("restored", frames=after, initial=initial, hybrid=control.hybrid())

    for process in icon_processes[1:]:
        process.terminate()
    wait_for("remaining iconified fixtures closed",
             lambda: all(title not in control.windows() for title in ICON_TITLES[1:]), 10)
    settled_hybrid = wait_for("gather page removed after restore and close", lambda: (
        control.hybrid() if not control.hybrid().get("minimizedGatherPagers")
        and control.hybrid().get("visibleIconChipCount") == 0 else None), 10)
    session.verdict("restoreAndCloseReflowMixedGather",
                    settled_hybrid.get("visibleIconChipCount") == 0)


def united_frames(frames):
    left = min(frame[0] for frame in frames)
    top = min(frame[1] for frame in frames)
    right = max(frame[0] + frame[2] for frame in frames)
    bottom = max(frame[1] + frame[3] for frame in frames)
    return left, top, right - left, bottom - top


def _pager_page(control, output_id: str, page: int):
    for entry in control.hybrid().get("minimizedGatherPagers") or []:
        if entry.get("outputId") == output_id and entry.get("currentPage") == page:
            return entry
    return None


def _intersects(first, second) -> bool:
    return (first[0] < second[0] + second[2] and second[0] < first[0] + first[2]
            and first[1] < second[1] + second[3] and second[1] < first[1] + first[3])


def _inside(frame, size) -> bool:
    return (frame["x"] >= 0 and frame["y"] >= 0
            and frame["x"] + frame["width"] <= size[0]
            and frame["y"] + frame["height"] <= size[1])
