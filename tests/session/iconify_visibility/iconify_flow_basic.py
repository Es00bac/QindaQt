# SPDX-License-Identifier: GPL-3.0-or-later
"""Roll an ordinary server-decorated window up to its icon chip and back (ADR-0203).

Order: a real wheel notch away from the user over the title bar iconifies
(window hidden, chip painted at the title bar's leading edge, vacated area
ghost-free and input-transparent); an ordinary drag moves the chip and the
restore frame with it; a wheel toward the user over the chip unrolls under
the chip; a real xdg-activation token unrolls; a double-click unrolls; a
compositor scene restart keeps the chip; closing the window while iconified
leaves nothing behind. Every step is judged by captured pixels, real presses,
the window inventory, and the hybrid diagnostics.
"""

from __future__ import annotations

import signal
import time

from iconify_control import (WHEEL_AWAY, WHEEL_TOWARD, approximately, chip_center, chip_ink_present,
                             chip_of, chip_probe_area, frame_probe_area, iconified_windows,
                             restore_frame_of, wait_iconified, wait_unrolled, wheel)
from shade_control import button, content_point, pointer, rect, title_point, wait_for
from shade_fixtures import BACKDROP_COLOUR, BACKDROP_TITLE
from shade_framebuffer import area_mismatch, pixel_matches
from shade_session import NEUTRAL_BACKDROP_POINT, ShadeSession, window_summary

WINDOW_TITLE = "Iconify window A"
WINDOW_COLOUR = "#b03a2e"
DRAG_DELTA = (220.0, 140.0)
INK_FRACTION = 0.5


def run(session: ShadeSession) -> None:
    control = session.control
    process = session.fixtures.gtk(WINDOW_TITLE, WINDOW_COLOUR, "ssd", 560, 380)
    wait_for("window mapped", lambda: WINDOW_TITLE in control.windows(), 30)
    time.sleep(1.0)
    session.wait_frames_settled([WINDOW_TITLE])
    inventory = control.windows()
    window = inventory[WINDOW_TITLE]
    window_id = window["id"]
    session.step("mapped", window=window_summary(window))
    session.capture("01-mapped")
    session.verdict("fixtureIsServerDecorated", bool(window.get("serverDecorated")))

    frame_before = rect(window["geometry"])
    chip = _iconify(session, "02-iconified", frame_before, first=True)
    chip_after, expected_frame = _drag_stage(session, window_id, frame_before, chip)
    _wheel_unroll_stage(session, window_id, chip_after, expected_frame)
    _activation_stage(session, process, window_id)
    _double_click_stage(session, window_id)
    frame_at_iconify = _scene_restart_stage(session, window_id)
    _close_stage(session, process, frame_at_iconify)


def _drag_stage(session: ShadeSession, window_id: str, frame_before, chip):
    """Ordinary drag: the chip and the restore frame move together."""
    start = chip_center(chip)
    end = (start[0] + DRAG_DELTA[0], start[1] + DRAG_DELTA[1])
    session.pointer.drag(start, end)
    time.sleep(0.5)
    chip_after = chip_of(session, window_id)
    frame = session.capture("03-chip-dragged")
    ghost = area_mismatch(frame, frame_probe_area(frame_before),
                          chip_probe_area(chip_after) if chip_after else None, BACKDROP_COLOUR)
    ink = chip_ink_present(frame, chip_after, BACKDROP_COLOUR) if chip_after else None
    expected_chip = (chip[0] + DRAG_DELTA[0], chip[1] + DRAG_DELTA[1], chip[2], chip[3])
    expected_frame = (frame_before[0] + DRAG_DELTA[0], frame_before[1] + DRAG_DELTA[1],
                      frame_before[2], frame_before[3])
    session.step("chip-dragged", chip=chip_after, restoreFrame=restore_frame_of(session, window_id),
                 ghost=ghost, ink=ink)
    session.verdict("chipDragMovesChip", approximately(chip_after, expected_chip, 2.0))
    session.verdict("chipDragKeepsRestoreFrameUnderChip",
                    approximately(restore_frame_of(session, window_id), expected_frame, 2.0))
    session.verdict("ghostFreeAfterChipDrag", ghost["mismatched"] == 0)
    session.verdict("chipInkVisibleAfterDrag", bool(ink) and ink["fraction"] > INK_FRACTION)
    if chip_after is None:
        raise RuntimeError("the chip vanished during its drag")
    return chip_after, expected_frame


def _wheel_unroll_stage(session: ShadeSession, window_id: str, chip, expected_frame) -> None:
    """Wheel toward the user over the chip: unroll under the chip, activated."""
    control = session.control
    wheel(session, chip_center(chip), WHEEL_TOWARD)
    wait_unrolled(session, window_id)
    time.sleep(0.6)
    restored = control.windows()[WINDOW_TITLE]
    frame = session.capture("04-unrolled")
    session.step("unrolled", window=window_summary(restored), expected=expected_frame,
                 hybrid=control.hybrid().get("iconifiedWindows"))
    session.verdict("unrollRestoresGeometryUnderChip",
                    approximately(rect(restored["geometry"]), expected_frame, 2.0))
    session.verdict("unrollShowsWindow", not restored["hidden"])
    session.verdict("unrollRestoresContent",
                    pixel_matches(frame, content_point(restored["geometry"]), WINDOW_COLOUR))
    session.verdict("unrollActivates", bool(restored["active"]))


def _activation_stage(session: ShadeSession, process, window_id: str) -> None:
    """A real activation token (KWin's activateWindow clears hidden) unrolls."""
    control = session.control
    frame_at_iconify = rect(control.windows()[WINDOW_TITLE]["geometry"])
    chip = _iconify(session, "05-iconified-again", frame_at_iconify)
    _keep_chip_clear_of(session, window_id, chip, NEUTRAL_BACKDROP_POINT)
    session.mint_backdrop_token()
    process.send_signal(signal.SIGUSR1)
    wait_unrolled(session, window_id, 6.0)
    time.sleep(0.8)
    inventory = control.windows()
    frame = session.capture("06-activation-unrolled")
    session.step("activation-unrolled", window=window_summary(inventory[WINDOW_TITLE]),
                 events=session.client_events()[-4:])
    session.verdict("activationUnrolls",
                    not inventory[WINDOW_TITLE]["hidden"] and bool(inventory[WINDOW_TITLE]["active"]))
    session.verdict("activationRestoresGeometry",
                    approximately(rect(inventory[WINDOW_TITLE]["geometry"]), frame_at_iconify, 2.0))
    session.verdict("activationRestoresContent",
                    pixel_matches(frame, content_point(inventory[WINDOW_TITLE]["geometry"]),
                                  WINDOW_COLOUR))


def _double_click_stage(session: ShadeSession, window_id: str) -> None:
    """A double-click on the chip body unrolls."""
    control = session.control
    frame_at_iconify = rect(control.windows()[WINDOW_TITLE]["geometry"])
    chip = _iconify(session, "07-iconified-for-double-click", frame_at_iconify)
    centre = chip_center(chip)
    control.inject([pointer(*centre)])
    time.sleep(0.05)
    for _ in range(2):
        control.inject([button("left", True)])
        time.sleep(0.04)
        control.inject([button("left", False)])
        time.sleep(0.08)
    wait_unrolled(session, window_id, 6.0)
    time.sleep(0.6)
    inventory = control.windows()
    session.capture("08-double-click-unrolled")
    session.step("double-click-unrolled", window=window_summary(inventory[WINDOW_TITLE]))
    session.verdict("doubleClickUnrolls",
                    not inventory[WINDOW_TITLE]["hidden"] and bool(inventory[WINDOW_TITLE]["active"]))


def _scene_restart_stage(session: ShadeSession, window_id: str):
    """A compositor scene restart recreates every WindowItem; the chip returns."""
    control = session.control
    frame_at_iconify = rect(control.windows()[WINDOW_TITLE]["geometry"])
    _iconify(session, "09-iconified-before-scene-restart", frame_at_iconify)
    reply = control.call("ReinitializeCompositingForTest")
    time.sleep(2.5)
    inventory = control.windows()
    hybrid = control.hybrid()
    chip_after = chip_of(session, window_id)
    frame = session.capture("10-after-scene-restart")
    ink = chip_ink_present(frame, chip_after, BACKDROP_COLOUR) if chip_after else None
    ghost = area_mismatch(frame, frame_probe_area(frame_at_iconify),
                          chip_probe_area(chip_after) if chip_after else None, BACKDROP_COLOUR)
    session.step("after-scene-restart", reply=reply, chip=chip_after, ink=ink, ghost=ghost,
                 hidden=inventory[WINDOW_TITLE]["hidden"],
                 visibleIconChipCount=hybrid.get("visibleIconChipCount"))
    session.verdict("sceneRestartKeepsIconified",
                    reply.get("status") == "scheduled" and chip_after is not None
                    and bool(inventory[WINDOW_TITLE]["hidden"]))
    session.verdict("sceneRestartKeepsChipVisible",
                    hybrid.get("visibleIconChipCount") == 1 and bool(ink)
                    and ink["fraction"] > INK_FRACTION)
    session.verdict("sceneRestartGhostFree", ghost["mismatched"] == 0)
    return frame_at_iconify


def _close_stage(session: ShadeSession, process, frame_at_iconify) -> None:
    """Closing the window while iconified leaves no chip and no record."""
    control = session.control
    process.terminate()
    wait_for("window closed", lambda: WINDOW_TITLE not in control.windows(), 8.0)
    time.sleep(0.8)
    hybrid = control.hybrid()
    frame = session.capture("11-closed-while-iconified")
    ghost = area_mismatch(frame, frame_probe_area(frame_at_iconify), None, BACKDROP_COLOUR)
    session.step("closed-while-iconified", iconified=hybrid.get("iconifiedWindows"),
                 visibleIconChipCount=hybrid.get("visibleIconChipCount"), ghost=ghost)
    session.verdict("closeWhileIconifiedForgets",
                    not hybrid.get("iconifiedWindows") and hybrid.get("visibleIconChipCount") == 0)
    session.verdict("closeWhileIconifiedGhostFree", ghost["mismatched"] == 0)


def _iconify(session: ShadeSession, capture_name: str, frame_before, first: bool = False):
    """One real wheel notch away from the user over the window's title bar."""
    control = session.control
    inventory = control.windows()
    window_id = inventory[WINDOW_TITLE]["id"]
    point = title_point(inventory, WINDOW_TITLE)
    if point is None:
        raise RuntimeError("no uncovered title-bar point on the window")
    wheel(session, point, WHEEL_AWAY)
    chip = wait_iconified(session, window_id)
    inventory = control.windows()
    hybrid = control.hybrid()
    frame = session.capture(capture_name)
    ghost = area_mismatch(frame, frame_probe_area(frame_before), chip_probe_area(chip),
                          BACKDROP_COLOUR)
    ink = chip_ink_present(frame, chip, BACKDROP_COLOUR)
    probe = session.probe_click("vacated-content", content_point(frame_before_geometry(frame_before)))
    session.step(capture_name, wheelPoint=list(point), chip=chip, ghost=ghost, ink=ink, probe=probe,
                 window=window_summary(inventory[WINDOW_TITLE]),
                 iconified=iconified_windows(session),
                 visibleIconChipCount=hybrid.get("visibleIconChipCount"))
    if first:
        session.verdict("windowHiddenAfterWheel", bool(inventory[WINDOW_TITLE]["hidden"]))
        session.verdict("chipAnchoredAtTitleLeadingEdge",
                        abs(chip[0] - frame_before[0]) <= 1.5 and abs(chip[1] - frame_before[1]) <= 1.5)
        session.verdict("chipVisible", hybrid.get("visibleIconChipCount") == 1)
        session.verdict("chipInkVisible", ink["fraction"] > INK_FRACTION)
        session.verdict("ghostFreeAfterIconify", ghost["mismatched"] == 0)
        session.verdict("inputPassesThroughWhileIconified",
                        probe["pressReceivers"] == [BACKDROP_TITLE])
        session.verdict("notActiveWhileIconified", not inventory[WINDOW_TITLE]["active"])
    elif not inventory[WINDOW_TITLE]["hidden"]:
        raise RuntimeError(f"{capture_name}: the window did not roll up")
    return chip


def frame_before_geometry(frame_before):
    return {"x": frame_before[0], "y": frame_before[1], "width": frame_before[2],
            "height": frame_before[3]}


def _keep_chip_clear_of(session: ShadeSession, window_id: str, chip, point):
    """Drag the chip away when it sits where the backdrop is about to be clicked."""
    centre = chip_center(chip)
    if abs(centre[0] - point[0]) > 80 or abs(centre[1] - point[1]) > 80:
        return chip
    session.pointer.drag(centre, (centre[0] + 260.0, centre[1] + 200.0))
    time.sleep(0.4)
    moved = chip_of(session, window_id)
    session.step("chip-moved-clear-of-backdrop-click", chip=moved)
    return moved or chip
