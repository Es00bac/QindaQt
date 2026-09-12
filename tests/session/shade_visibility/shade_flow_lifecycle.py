# SPDX-License-Identifier: GPL-3.0-or-later
"""Lifecycle changes while a three-member client-side-decorated group is rolled up.

Order: roll up with member B active (B anchors the strip); map a transient
dialog of B; close B (anchor promotion); minimize the group from its strip;
hand member A a real activation token (KWin unminimizes and unhides it);
unroll. Every step is judged by captured pixels, real presses, and inventory.
"""

from __future__ import annotations

import signal
import subprocess
import time
from typing import Any

from shade_control import (MINIMIZE_GROUP_MENU_INDEX, ROLL_UP_MENU_INDEX, Rect, content_point,
                           rect, united, wait_for)
from shade_fixtures import (BACKDROP_COLOUR, BACKDROP_TITLE, MEMBER_A_COLOUR, MEMBER_A_TITLE,
                            MEMBER_B_COLOUR, MEMBER_B_TITLE, MEMBER_C_COLOUR, MEMBER_C_TITLE)
from shade_framebuffer import area_mismatch, pixel_matches
from shade_session import ShadeSession, window_summary

DIALOG_TITLE = MEMBER_B_TITLE + " dialog"
SURVIVORS = (MEMBER_A_TITLE, MEMBER_C_TITLE)


def run(session: ShadeSession) -> None:
    processes = _start_members(session)
    before = _group_three(session, processes)
    strip = _roll_up(session, before)
    _transient_while_shaded(session, processes, strip)
    strip = _close_anchor(session, processes, before, strip)
    strip = _minimize_then_activate(session, processes, strip)
    _unroll(session, strip)


def _screen(session: ShadeSession) -> Rect:
    return (0.0, 0.0, *session.config.logical_size)


def _strip(session: ShadeSession) -> Rect | None:
    strips = session.hybrid_summary().get("shadedStripFrames") or []
    return rect(strips[0]) if strips else None


def _strip_point(strip: Rect) -> tuple[float, float]:
    return (strip[0] + strip[2] * 0.3, strip[1] + strip[3] / 2)


def _start_members(session: ShadeSession) -> dict[str, subprocess.Popen]:
    fixtures = session.fixtures
    # AGENT-NOTE: different aspect ratios (wide, tall, squarish) mean no member
    # can ever sit entirely inside another however KWin places them, so every
    # member keeps an uncovered area a real click or dock gesture can use.
    processes = {
        MEMBER_A_TITLE: fixtures.gtk(MEMBER_A_TITLE, MEMBER_A_COLOUR, "csd", 700, 300),
        MEMBER_B_TITLE: fixtures.gtk(MEMBER_B_TITLE, MEMBER_B_COLOUR, "csd", 360, 520),
        MEMBER_C_TITLE: fixtures.gtk(MEMBER_C_TITLE, MEMBER_C_COLOUR, "csd", 480, 420),
    }
    wait_for("three members mapped",
             lambda: all(title in session.control.windows() for title in processes), 30)
    time.sleep(1.0)
    session.wait_frames_settled(list(processes))
    mapped = session.control.windows()
    session.step("mapped", members={title: window_summary(mapped[title]) for title in processes})
    session.capture("L00-mapped")
    return processes


def _group_three(session: ShadeSession, processes) -> dict[str, dict[str, Any]]:
    control = session.control

    def await_group(description: str, *titles: str) -> None:
        try:
            wait_for(description, lambda: same_group(*titles), 8)
        except RuntimeError:
            current = control.windows()
            session.step("dock-failed", members={t: window_summary(current[t]) for t in processes},
                         hybrid=session.hybrid_summary())
            session.capture("L00-dock-failed")
            raise

    def same_group(*titles: str):
        current = control.windows()
        owners = {current[title].get("containerId") for title in titles}
        return current if len(owners) == 1 and not owners & {"", None} else None

    session.dock(MEMBER_C_TITLE, MEMBER_B_TITLE)
    await_group("C and B grouped", MEMBER_C_TITLE, MEMBER_B_TITLE)
    session.wait_frames_settled(list(processes))
    session.dock(MEMBER_A_TITLE, MEMBER_B_TITLE)
    await_group("three members grouped", *processes)
    session.wait_frames_settled(list(processes))
    session.activate(MEMBER_B_TITLE)
    before = control.windows()
    session.step("grouped", members={title: window_summary(before[title]) for title in processes})
    session.capture("L01-grouped")
    return before


def _roll_up(session: ShadeSession, before) -> Rect:
    frames = [before[title]["targetGeometry"] for title in (*SURVIVORS, MEMBER_B_TITLE)]
    left, top, width, _ = united(frames)
    session.pointer.group_menu_action((left + width * 0.3, top - 14), ROLL_UP_MENU_INDEX)
    wait_for("members hidden", lambda: all(
        session.control.windows()[title]["hidden"] for title in (*SURVIVORS, MEMBER_B_TITLE)), 5)
    strip = _strip(session)
    frame = session.capture("L02-shaded")
    ghost = area_mismatch(frame, _screen(session), strip, BACKDROP_COLOUR, 4)
    session.step("shaded", strip=strip, ghost=ghost, hybrid=session.hybrid_summary())
    session.verdict("lifecycleGhostFreeAfterRollUp", strip is not None and ghost["mismatched"] == 0)
    if strip is None:
        raise RuntimeError("rolled-up group published no strip frame")
    return strip


def _transient_while_shaded(session: ShadeSession, processes, strip: Rect) -> None:
    processes[MEMBER_B_TITLE].send_signal(signal.SIGHUP)
    wait_for("dialog mapped", lambda: any(
        event["event"] == "dialog-opened" for event in session.client_events()), 5)
    time.sleep(1.5)
    inventory = session.control.windows()
    dialog = inventory.get(DIALOG_TITLE)
    frame = session.capture("L03-dialog-while-shaded")
    ghost = area_mismatch(frame, _screen(session), strip, BACKDROP_COLOUR, 4)
    session.step("dialog-while-shaded", dialog=window_summary(dialog), ghost=ghost,
                 active=session.active_titles(inventory))
    session.verdict("transientHiddenWhileShaded", ghost["mismatched"] == 0)
    session.verdict("transientNotActiveWhileShaded", not (dialog and dialog["active"]))


def _close_anchor(session: ShadeSession, processes, before, strip: Rect) -> Rect:
    processes[MEMBER_B_TITLE].terminate()
    wait_for("anchor closed", lambda: MEMBER_B_TITLE not in session.control.windows(), 8)
    time.sleep(1.0)
    inventory = session.control.windows()
    hybrid = session.hybrid_summary()
    strip = _strip(session) or strip
    frame = session.capture("L04-anchor-closed-while-shaded")
    ghost = area_mismatch(frame, _screen(session), strip, BACKDROP_COLOUR, 4)
    probes = [session.probe_click(title, content_point(before[title]["targetGeometry"]))
              for title in SURVIVORS]
    session.step("anchor-closed-while-shaded", hybrid=hybrid, ghost=ghost, probes=probes,
                 hidden={title: inventory[title]["hidden"] for title in SURVIVORS})
    session.verdict("anchorCloseKeepsStrip", hybrid.get("shadedContainerCount") == 1
                    and hybrid.get("visibleAnchoredChromeSceneItemCount") == 1)
    session.verdict("anchorCloseKeepsSurvivorsHidden",
                    all(inventory[title]["hidden"] for title in SURVIVORS))
    session.verdict("anchorCloseGhostFree", ghost["mismatched"] == 0)
    session.verdict("anchorCloseInputPassesThrough",
                    all(probe["pressReceivers"] == [BACKDROP_TITLE] for probe in probes))
    return strip


def _minimize_then_activate(session: ShadeSession, processes, strip: Rect) -> Rect:
    session.pointer.group_menu_action(_strip_point(strip), MINIMIZE_GROUP_MENU_INDEX)
    time.sleep(1.0)
    inventory = session.control.windows()
    frame = session.capture("L05-minimized-while-shaded")
    ghost = area_mismatch(frame, _screen(session), None, BACKDROP_COLOUR, 4)
    session.step("minimized-while-shaded", ghost=ghost, hybrid=session.hybrid_summary(),
                 minimized={title: inventory[title]["minimized"] for title in SURVIVORS},
                 hidden={title: inventory[title]["hidden"] for title in SURVIVORS})
    session.verdict("minimizeWhileShadedGhostFree", ghost["mismatched"] == 0)

    session.mint_backdrop_token()
    processes[MEMBER_A_TITLE].send_signal(signal.SIGUSR1)
    time.sleep(2.5)
    inventory = session.control.windows()
    hybrid = session.hybrid_summary()
    strip = _strip(session) or strip
    frame = session.capture("L06-activated-after-minimize")
    ghost = area_mismatch(frame, _screen(session), strip, BACKDROP_COLOUR, 4)
    session.step("activated-after-minimize", hybrid=hybrid, ghost=ghost,
                 minimized={title: inventory[title]["minimized"] for title in SURVIVORS},
                 hidden={title: inventory[title]["hidden"] for title in SURVIVORS},
                 active=session.active_titles(inventory), events=session.client_events()[-6:])
    session.verdict("activationAfterMinimizeKeepsHidden",
                    all(inventory[title]["hidden"] for title in SURVIVORS))
    session.verdict("activationAfterMinimizeGhostFree", ghost["mismatched"] == 0)
    session.verdict("activationAfterMinimizeNoHiddenFocus",
                    not any(inventory[title]["active"] for title in SURVIVORS))
    return strip


def _unroll(session: ShadeSession, strip: Rect) -> None:
    control = session.control
    session.pointer.group_menu_action(_strip_point(strip), ROLL_UP_MENU_INDEX)
    wait_for("survivors shown", lambda: not any(
        control.windows()[title]["hidden"] for title in SURVIVORS), 6)
    time.sleep(0.5)
    restored = control.windows()
    frame = session.capture("L07-unrolled")
    content = {
        MEMBER_A_TITLE: pixel_matches(
            frame, content_point(restored[MEMBER_A_TITLE]["targetGeometry"]), MEMBER_A_COLOUR),
        MEMBER_C_TITLE: pixel_matches(
            frame, content_point(restored[MEMBER_C_TITLE]["targetGeometry"]), MEMBER_C_COLOUR),
    }
    session.step("unrolled", content=content, active=session.active_titles(restored),
                 members={title: window_summary(restored[title]) for title in SURVIVORS},
                 hybrid=session.hybrid_summary())
    session.verdict("lifecycleUnrollRestoresContent", all(content.values()))
    session.verdict("lifecycleUnrollNotMinimized",
                    not any(restored[title]["minimized"] for title in SURVIVORS))
    session.verdict("lifecycleUnrollNoHiddenFocus", not any(
        window["hidden"] and window["active"] for window in restored.values()))
