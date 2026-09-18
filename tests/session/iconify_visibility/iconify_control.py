# SPDX-License-Identifier: GPL-3.0-or-later
"""Iconify-specific helpers over the shade-visibility session (ADR-0203)."""

from __future__ import annotations

import time
from typing import Any

from shade_control import INPUT_MARGIN, Rect, _GRID, rect, wait_for
from shade_framebuffer import Frame, parse_colour
from shade_session import ShadeSession

# One wheel notch as libinput reports it; negative turns away from the user.
WHEEL_AWAY = -15.0
WHEEL_TOWARD = 15.0
# Client-side shadows extend past a frame; probe generously around it.
SHADOW_MARGIN = 48.0


def axis(delta: float) -> dict[str, Any]:
    return {"type": "pointer-axis", "axis": "vertical", "delta": float(delta)}


def wheel(session: ShadeSession, point: tuple[float, float], delta: float) -> None:
    """One real wheel notch at `point` through the development input device."""
    session.pointer.move(*point)
    time.sleep(0.05)
    session.control.inject([axis(delta)])
    time.sleep(0.35)


def iconified_windows(session: ShadeSession) -> list[dict[str, Any]]:
    return session.control.hybrid().get("iconifiedWindows") or []


def chip_of(session: ShadeSession, window_id: str) -> Rect | None:
    for entry in iconified_windows(session):
        if entry.get("windowId") == window_id:
            return rect(entry["chipFrame"])
    return None


def restore_frame_of(session: ShadeSession, window_id: str) -> Rect | None:
    for entry in iconified_windows(session):
        if entry.get("windowId") == window_id:
            return rect(entry["restoreFrame"])
    return None


def chip_center(chip: Rect) -> tuple[float, float]:
    return (chip[0] + chip[2] / 2, chip[1] + chip[3] / 2)


def chip_probe_area(chip: Rect) -> Rect:
    """The chip's image extent: the pill plus the close glyph overhang."""
    return (chip[0] - 4, chip[1] - 4, chip[2] + 8, chip[3] + 8)


def frame_probe_area(frame: Rect) -> Rect:
    x, y, w, h = frame
    return (x - SHADOW_MARGIN, y - SHADOW_MARGIN, w + 2 * SHADOW_MARGIN, h + 2 * SHADOW_MARGIN)


def chip_ink_present(frame: Frame, chip: Rect, backdrop: str, step: int = 2) -> dict[str, Any]:
    """Count sampled device pixels inside the pill that differ from the backdrop.

    The pill is painted with the raised surface and a border; a visible chip
    therefore shows many non-backdrop pixels inside its circle.
    """
    target = parse_colour(backdrop)
    cx, cy = chip_center(chip)
    radius = min(chip[2], chip[3]) / 2 - 2
    differing = sampled = 0
    for ly in range(int(cy - radius), int(cy + radius), 1):
        for lx in range(int(cx - radius), int(cx + radius), 1):
            if (lx - cx) ** 2 + (ly - cy) ** 2 > radius * radius:
                continue
            if (lx + ly) % step:
                continue
            px, py = int(lx * frame.scale), int(ly * frame.scale)
            if px < 0 or py < 0 or px >= frame.width or py >= frame.height:
                continue
            sampled += 1
            colour = frame.rgb(px, py)
            if any(abs(colour[index] - target[index]) > 3 for index in range(3)):
                differing += 1
    return {"differing": differing, "sampled": sampled,
            "fraction": (differing / sampled) if sampled else 0.0}


def uncovered_desktop_point(inventory: dict[str, dict[str, Any]],
                            screen: tuple[float, float]) -> tuple[float, float] | None:
    """A point no shown window (plus its input margin) covers; the bare desktop."""
    width, height = screen
    for fy in _GRID:
        for fx in _GRID:
            point = (width * fx, height * fy)
            covered = False
            for window in inventory.values():
                if window["hidden"] or window["minimized"]:
                    continue
                x, y, w, h = rect(window["geometry"])
                if (x - INPUT_MARGIN <= point[0] <= x + w + INPUT_MARGIN
                        and y - INPUT_MARGIN <= point[1] <= y + h + INPUT_MARGIN):
                    covered = True
                    break
            if not covered:
                return point
    return None


def wait_iconified(session: ShadeSession, window_id: str, timeout: float = 5.0) -> Rect:
    def chip():
        return chip_of(session, window_id)
    return wait_for(f"{window_id} iconified", chip, timeout)


def wait_unrolled(session: ShadeSession, window_id: str, timeout: float = 6.0) -> None:
    wait_for(f"{window_id} unrolled",
             lambda: chip_of(session, window_id) is None, timeout)


def approximately(actual: Rect | None, expected: Rect | None, tolerance: float = 1.5) -> bool:
    if actual is None or expected is None:
        return False
    return all(abs(a - e) <= tolerance for a, e in zip(actual, expected))
