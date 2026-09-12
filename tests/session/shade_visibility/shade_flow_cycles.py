# SPDX-License-Identifier: GPL-3.0-or-later
"""Repeated roll-up/unroll of a two-member group whose active member is `kind`.

Per cycle: roll up from the shared row's group menu, prove with captured
pixels that nothing but the strip remains over the vacated area, prove with
real presses that the backdrop (not a hidden member) receives input there,
unroll from the strip, and prove exact frames, content pixels, and focus.
The first cycle also hands the member a real xdg-activation token while
shaded -- KWin's activateWindow() clears Window::isHidden() -- and proves the
member stays hidden, unfocused, invisible, and input-transparent.
"""

from __future__ import annotations

import time

from shade_control import (MINIMIZE_GROUP_MENU_INDEX, ROLL_UP_MENU_INDEX, SHARED_ROW_HEIGHT,
                           content_point, rect, united, wait_for)
from shade_fixtures import (BACKDROP_COLOUR, BACKDROP_TITLE, MEMBER_A_COLOUR, MEMBER_A_TITLE)
from shade_framebuffer import area_mismatch, pixel_matches
from shade_session import ShadeSession, window_summary

CYCLES = 3
# Client-side shadows extend past the frame; probe generously around it.
SHADOW_MARGIN = 48.0

assert MINIMIZE_GROUP_MENU_INDEX != ROLL_UP_MENU_INDEX


def run(session: ShadeSession) -> None:
    control, pointer = session.control, session.pointer
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
    first, second = inventory[MEMBER_A_TITLE], find_member(inventory)
    title_b = second["title"]
    session.step("mapped", a=window_summary(first), b=window_summary(second))
    session.capture("01-mapped")

    session.dock(MEMBER_A_TITLE, title_b)

    def grouped():
        current = control.windows()
        owner = current[MEMBER_A_TITLE].get("containerId")
        return current if owner and owner == current[title_b].get("containerId") else None

    wait_for("members grouped", grouped, 8)
    time.sleep(0.8)
    inventory = control.windows()
    pointer.click(*content_point(inventory[title_b]["targetGeometry"]))
    wait_for("member B active", lambda: control.windows()[title_b]["active"], 5)
    before = control.windows()
    session.step("grouped-active", a=window_summary(before[MEMBER_A_TITLE]),
                 b=window_summary(before[title_b]), hybrid=session.hybrid_summary())
    session.capture("02-grouped-active")

    frames = {title: before[title]["targetGeometry"] for title in (MEMBER_A_TITLE, title_b)}
    left, top, width, height = united(list(frames.values()))
    outer = (left - 1, top - SHARED_ROW_HEIGHT, width + 2, height + SHARED_ROW_HEIGHT + 1)
    row_point = (outer[0] + outer[2] * 0.3, outer[1] + 15)
    probe_area = (outer[0] - SHADOW_MARGIN, outer[1] - SHADOW_MARGIN,
                  outer[2] + 2 * SHADOW_MARGIN, outer[3] + 2 * SHADOW_MARGIN)

    for cycle in range(CYCLES):
        focus_at_rollup: list[str] = []

        def observe_open_menu() -> None:
            # AGENT-NOTE: pressing the shared row already activates the
            # group's representative (existing chrome-press policy), so the
            # focus the group holds at roll-up is observed here, after the
            # press and before the roll-up command runs.
            focus_at_rollup.extend(t for t in session.active_titles() if t in frames)
            if cycle == 0:
                session.capture("02b-group-menu-open")

        pointer.group_menu_action(row_point, ROLL_UP_MENU_INDEX, while_open=observe_open_menu)

        def shaded():
            current = control.windows()
            return current if all(current[t]["hidden"] for t in frames) else None

        shaded_inventory = wait_for(f"members hidden (cycle {cycle})", shaded, 5)
        hybrid = session.hybrid_summary()
        strips = hybrid.get("shadedStripFrames") or []
        strip = rect(strips[0]) if strips else None
        frame = session.capture(f"03-shaded-cycle{cycle}")
        ghost = area_mismatch(frame, probe_area, strip, BACKDROP_COLOUR)
        probes = [session.probe_click("anchor-area", content_point(frames[title_b])),
                  session.probe_click("member-area", content_point(frames[MEMBER_A_TITLE]))]
        session.step(f"shaded-cycle{cycle}", strip=strip, ghost=ghost, probes=probes,
                     hidden={t: shaded_inventory[t]["hidden"] for t in frames},
                     active=session.active_titles(shaded_inventory), hybrid=hybrid)
        session.verdict(f"ghostFreeAfterRollUp{cycle}", ghost["mismatched"] == 0)
        session.verdict(f"inputPassesThroughWhileShaded{cycle}", all(
            probe["pressReceivers"] == [BACKDROP_TITLE] for probe in probes))
        session.verdict(f"stripVisible{cycle}", strip is not None
                        and hybrid.get("visibleAnchoredChromeSceneItemCount") == 1)

        if cycle == 0:
            _activation_while_shaded(session, member, frames, title_b, probe_area, strip)

        strip_point = (strip[0] + strip[2] * 0.3, strip[1] + strip[3] / 2) if strip else row_point
        pointer.group_menu_action(strip_point, ROLL_UP_MENU_INDEX)

        def unrolled():
            current = control.windows()
            return current if not any(current[t]["hidden"] for t in frames) else None

        restored = wait_for(f"members shown (cycle {cycle})", unrolled, 6)
        time.sleep(0.4)
        restored = control.windows()
        frame = session.capture(f"05-unrolled-cycle{cycle}")
        content = {MEMBER_A_TITLE: pixel_matches(frame, content_point(frames[MEMBER_A_TITLE]),
                                                 MEMBER_A_COLOUR)}
        if member.content_colour:
            content[title_b] = pixel_matches(frame, content_point(frames[title_b]),
                                             member.content_colour)
        frames_restored = all(restored[t]["targetGeometry"] == frames[t] for t in frames)
        active = session.active_titles(restored)
        session.step(f"unrolled-cycle{cycle}", framesRestored=frames_restored,
                     contentPixels=content, active=active, focusAtRollUp=focus_at_rollup,
                     hybrid=session.hybrid_summary())
        session.verdict(f"unrollRestoresFrames{cycle}", frames_restored)
        session.verdict(f"unrollRestoresContent{cycle}", all(content.values()))
        # The member that held focus when the group rolled up holds it again.
        session.verdict(f"unrollRestoresFocus{cycle}",
                        len(focus_at_rollup) == 1 and active == focus_at_rollup)
        pointer.click(*content_point(frames[title_b]))
        time.sleep(0.4)


def _activation_while_shaded(session, member, frames, title_b, probe_area, strip) -> None:
    if member.request_activation is None:
        session.step("activation-while-shaded",
                     unavailable="fixture has no scriptable activation request")
        return
    member.request_activation(session.mint_backdrop_token())
    time.sleep(2.5)
    inventory = session.control.windows()
    frame = session.capture("04-shaded-after-activation")
    ghost = area_mismatch(frame, probe_area, strip, BACKDROP_COLOUR)
    probes = [session.probe_click("anchor-area", content_point(frames[title_b])),
              session.probe_click("member-area", content_point(frames[MEMBER_A_TITLE]))]
    session.step("activation-while-shaded",
                 hidden={t: inventory[t]["hidden"] for t in frames if t in inventory},
                 active=session.active_titles(inventory), ghost=ghost, probes=probes,
                 events=session.client_events()[-6:], hybrid=session.hybrid_summary())
    session.verdict("membersStayHiddenAfterActivation",
                    all(inventory[t]["hidden"] for t in frames))
    session.verdict("hiddenMemberNotActiveAfterActivation",
                    not any(inventory[t]["active"] for t in frames))
    session.verdict("ghostFreeAfterActivation", ghost["mismatched"] == 0)
    session.verdict("inputPassesThroughAfterActivation", all(
        probe["pressReceivers"] == [BACKDROP_TITLE] for probe in probes))
