#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Canonical application and launcher glyphs for the QindaQt icon theme.

AGENT-NOTE: the `preferences-system-*` family shares `gear_badge()` because
each name genuinely means "settings for <subsystem>" -- the badge is a real
semantic device, not an index-derived decoration. See
tools/qinda_icon_shapes.py for the transparency rule these fragments follow.

AGENT-GUARD: first-party identity here is a BLUE body with at most one AMBER
accent (see the palette contract in tools/qinda_icon_shapes.py). Marks
painted *on top of* a BLUE body stay PORCELAIN or INK: AMBER and BLUE have
nearly equal luminance, so an amber mark on a blue body survives only by hue.
AMBER is placed where it meets the surface (tails, ticks, prompts, pens).
"""
from qinda_icon_shapes import (
    AMBER, APRICOT, BELL_CLAPPER_D, BELL_D, BLUE, BLUETOOTH_D, FOLDER_D,
    INK, PORCELAIN, VIOLET, Icon, dot, filled, gear_badge, ring, rect, stroke, strokes,
)

GROUP = "apps"


def _q_logo(accent: str, tail: str) -> Icon:
    # A steep, short, thick tail crossing *into* the ring reads as a monogram
    # Q; a shallow, thin, external tangent line is the universal magnifier
    # glyph (see edit-find). Keep this shape unambiguously the former.
    symbolic = ring(32, 28, 16, width=9) + stroke("M33 37 38 54", width=10)
    color = (f'<circle cx="32" cy="28" r="21" fill="{accent}"/>'
             f'<circle cx="32" cy="28" r="10" fill="{PORCELAIN}"/>'
             f'<path d="M33 37 38 54" stroke="{tail}" stroke-width="11" stroke-linecap="round"/>')
    return Icon(GROUP, symbolic, color)


CANON = {
    "start-here-kde": _q_logo(BLUE, AMBER),
    "applications-other": Icon(
        GROUP,
        rect(11, 11, 18, 18, rx=5) + rect(35, 11, 18, 18, rx=5) + rect(11, 35, 18, 18, rx=5) + rect(35, 35, 18, 18, rx=5),
        # Palette samplers (this grid, the theme dots, the color dial) show the
        # brand accents only; jade is a status color and never appears here.
        rect(11, 11, 18, 18, rx=5, fill=AMBER) + rect(35, 11, 18, 18, rx=5, fill=BLUE)
        + rect(11, 35, 18, 18, rx=5, fill=APRICOT) + rect(35, 35, 18, 18, rx=5, fill=VIOLET),
    ),
    "application-x-executable": Icon(
        GROUP,
        rect(14, 10, 36, 44, rx=10) + stroke("M26 24 38 32 26 40"),
        rect(14, 10, 36, 44, rx=10, fill=BLUE) + stroke("M26 24 38 32 26 40", color=PORCELAIN, width=6),
    ),
    "preferences-system": Icon(
        GROUP,
        ring(32, 32, 14) + strokes("M32 12v6", "M32 46v6", "M12 32h6", "M46 32h6"),
        f'<circle cx="32" cy="32" r="22" fill="{BLUE}"/><circle cx="32" cy="32" r="9" fill="{PORCELAIN}"/>'
        + strokes("M32 6v8", "M32 50v8", "M6 32h8", "M50 32h8", color=AMBER, width=6),
    ),
    "preferences-desktop-theme": Icon(
        GROUP,
        ring(24, 24, 8) + ring(40, 24, 8) + ring(32, 40, 8),
        dot(24, 24, 9, AMBER) + dot(40, 24, 9, BLUE) + dot(32, 40, 9, VIOLET),
    ),
    "preferences-desktop-color": Icon(
        GROUP,
        # Three swatch dots inside a ring reads as a color-picker dial; a
        # single diagonal line across a ring reads as a prohibition sign.
        ring(32, 32, 16) + dot(24, 26, 3) + dot(40, 26, 3) + dot(32, 40, 3),
        f'<path d="M32 10a22 22 0 0 1 0 44z" fill="{APRICOT}"/><path d="M32 10a22 22 0 0 0 0 44z" fill="{BLUE}"/>'
        f'<circle cx="32" cy="32" r="7" fill="{PORCELAIN}"/>',
    ),
    "preferences-desktop-display": Icon(
        GROUP,
        rect(11, 12, 34, 26, rx=4) + stroke("M22 46h12M28 38v8") + ring(50, 12, 5, width=4),
        rect(11, 12, 34, 26, rx=4, fill=BLUE) + stroke("M22 46h12M28 38v8", color=INK, width=5)
        + ring(50, 12, 5, width=4, color=APRICOT),
    ),
    "preferences-desktop-plasma": Icon(
        GROUP,
        rect(12, 22, 26, 26, rx=5) + rect(26, 14, 26, 26, rx=5),
        rect(12, 22, 26, 26, rx=5, fill=VIOLET) + rect(26, 14, 26, 26, rx=5, fill=BLUE),
    ),
    "preferences-plugin": Icon(
        GROUP,
        stroke("M18 20h10v-2a4 4 0 0 1 8 0v2h10v10h2a4 4 0 0 1 0 8h-2v10H36v-2a4 4 0 0 0-8 0v2H18V38h-2a4 4 0 0 1 0-8h2z", width=4),
        filled("M18 20h10v-2a4 4 0 0 1 8 0v2h10v10h2a4 4 0 0 1 0 8h-2v10H36v-2a4 4 0 0 0-8 0v2H18V38h-2a4 4 0 0 1 0-8h2z", fill=BLUE)
        + stroke("M18 20h10v-2a4 4 0 0 1 8 0v2h10v10h2a4 4 0 0 1 0 8h-2v10H36v-2a4 4 0 0 0-8 0v2H18V38h-2a4 4 0 0 1 0-8h2z", width=3, color=INK),
    ),
    "preferences-system-network": Icon(
        GROUP,
        ring(32, 32, 18) + stroke("M14 32h36") + strokes("M32 14c-10 6-10 30 0 36", "M32 14c10 6 10 30 0 36", width=5),
        f'<circle cx="32" cy="32" r="22" fill="{BLUE}"/>'
        + strokes("M12 32h40", "M32 12c-11 7-11 33 0 40", "M32 12c11 7 11 33 0 40", color=PORCELAIN, width=4),
    ),
    "preferences-system-notifications": Icon(
        GROUP,
        stroke(BELL_D, width=5) + stroke(BELL_CLAPPER_D, width=5) + gear_badge(),
        filled(BELL_D, fill=BLUE) + stroke(BELL_CLAPPER_D, color=INK, width=4) + gear_badge(APRICOT),
    ),
    "preferences-system-power-management": Icon(
        GROUP,
        rect(11, 22, 38, 22, rx=5) + stroke("M49 29v8") + gear_badge(),
        rect(11, 22, 38, 22, rx=5, fill=BLUE) + stroke("M49 29v8", color=INK, width=5) + gear_badge(APRICOT),
    ),
    "preferences-system-windows": Icon(
        GROUP,
        rect(10, 14, 36, 30, rx=4) + stroke("M10 22h36") + gear_badge(),
        rect(10, 14, 36, 30, rx=4, fill=BLUE) + stroke("M10 22h36", color=PORCELAIN, width=4) + gear_badge(APRICOT),
    ),
    "preferences-system-bluetooth": Icon(
        GROUP,
        stroke(BLUETOOTH_D, width=5) + gear_badge(),
        stroke(BLUETOOTH_D, color=BLUE, width=6) + gear_badge(APRICOT),
    ),
    "preferences-system-search": Icon(
        GROUP,
        ring(26, 26, 12) + stroke("M35 35 48 48") + gear_badge(),
        ring(26, 26, 12, width=7, color=BLUE) + stroke("M35 35 48 48", color=INK, width=7) + gear_badge(APRICOT),
    ),
    "notifications": Icon(
        GROUP,
        stroke(BELL_D, width=6) + stroke(BELL_CLAPPER_D, width=6),
        filled(BELL_D, fill=BLUE) + stroke(BELL_CLAPPER_D, color=INK, width=5),
    ),
    "clock": Icon(
        GROUP,
        ring(32, 32, 18) + stroke("M32 32V19M32 32 43 39"),
        f'<circle cx="32" cy="32" r="22" fill="{BLUE}"/>'
        + stroke("M32 32V17M32 32 44 40", color=PORCELAIN, width=5),
    ),
    "system-file-manager": Icon(
        GROUP,
        stroke(FOLDER_D, width=5) + stroke("M22 38h20", width=4),
        filled(FOLDER_D, fill=BLUE) + stroke("M22 38h20", color=PORCELAIN, width=5),
    ),
    "accessories-text-editor": Icon(
        GROUP,
        rect(14, 8, 36, 48, rx=6) + strokes("M22 22h20", "M22 32h20", "M22 42h12", width=4) + stroke("M44 44 50 50", width=4) + dot(50, 50, 2),
        rect(14, 8, 36, 48, rx=6, fill=PORCELAIN) + strokes("M22 22h20", "M22 32h20", "M22 42h12", color=BLUE, width=4)
        + stroke("M44 44 50 50", color=AMBER, width=4),
    ),
    "utilities-terminal": Icon(
        GROUP,
        rect(10, 12, 44, 40, rx=8) + stroke("M20 26 28 34 20 42", width=5) + stroke("M36 42h10", width=5),
        rect(10, 12, 44, 40, rx=8, fill=INK) + stroke("M20 26 28 34 20 42", color=AMBER, width=5)
        + stroke("M36 42h10", color=AMBER, width=5),
    ),
}

ALIASES = {
    "application-launcher": ("start-here-kde", None),
    "org.qindaqt.FileManager": ("system-file-manager", None),
    "org.qindaqt.TextEditor": ("accessories-text-editor", None),
    "org.qindaqt.Settings": ("preferences-system", None),
    "org.qindaqt.Terminal": ("utilities-terminal", None),
}
