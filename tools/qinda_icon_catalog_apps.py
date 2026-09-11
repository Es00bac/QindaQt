#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Canonical application and launcher glyphs for the QindaQt icon theme.

AGENT-NOTE: the `preferences-system-*` family shares `gear_badge()` because
each name genuinely means "settings for <subsystem>" -- the badge is a real
semantic device, not an index-derived decoration. See
tools/qinda_icon_shapes.py for the transparency rule these fragments follow.

AGENT-GUARD: First-party material icons use plum, pearl and apricot. Keep
small on-body marks readable by luminance and preserve real symbolic cutouts.
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
    "preferences-desktop-accessibility": Icon(
        GROUP,
        # Universal-access figure: head dot, outstretched arms, standing legs,
        # inside a ring. Distinct from the notification bell and the gear.
        ring(32, 32, 24, width=5) + dot(32, 19, 4) + strokes("M20 28h24", "M32 28v12", "M32 40l-7 10", "M32 40l7 10", width=5),
        f'<circle cx="32" cy="32" r="26" fill="{BLUE}"/>'
        + dot(32, 19, 5, PORCELAIN)
        + strokes("M20 28h24", "M32 28v12", "M32 40l-7 10", "M32 40l7 10", color=PORCELAIN, width=5),
    ),
    "preferences-desktop-wallpaper": Icon(
        GROUP,
        # A framed landscape: sun in the corner, two hills across the bottom.
        rect(9, 13, 46, 38, rx=5) + dot(22, 25, 4)
        + stroke("M12 44l11-12 8 8 7-6 14 10", width=4),
        rect(9, 13, 46, 38, rx=5, fill=VIOLET) + dot(22, 25, 5, PORCELAIN)
        + filled("M9 46l14-14 8 8 7-6 17 12v5H9z", fill=APRICOT),
    ),
    "preferences-desktop-font": Icon(
        GROUP,
        # A capital A with its crossbar; the letterform is the glyph.
        strokes("M14 50 32 12 50 50", "M22 36h20", width=6),
        rect(8, 8, 48, 48, rx=8, fill=APRICOT)
        + strokes("M16 50 32 12 48 50", "M23 36h18", color=INK, width=6),
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

# Three app identities share a material vocabulary, not a generic app tile.
# Gradients are confined to color artwork; symbolic masks remain precise glyphs.
_material = (f'<defs><linearGradient id="glass" x2="0" y2="1">'
             f'<stop stop-color="{BLUE}"/><stop offset="1" stop-color="{INK}"/>'
             f'</linearGradient><linearGradient id="paper" x2="0" y2="1">'
             f'<stop stop-color="{PORCELAIN}"/><stop offset="1" stop-color="{VIOLET}"/>'
             f'</linearGradient></defs>')
CANON["system-file-manager"] = Icon(
    GROUP, stroke("M8 22V14q0-5 5-5h12l7 7h18q6 0 6 6v26q0 7-7 7H15q-7 0-7-7z", width=4)
    + stroke("M9 25h46M24 43h16", width=4),
    _material + filled("M8 23V14q0-5 5-5h12l7 7h18q6 0 6 6v25H8z", fill=AMBER)
    + rect(12, 20, 40, 29, rx=5, fill=PORCELAIN)
    + filled("M7 27q0-5 6-5h38q6 0 6 5v21q0 8-8 8H15q-8 0-8-8z", fill="url(#glass)")
    + stroke("M12 27h40", color=PORCELAIN, width=2)
    + stroke("M25 44h14", color=PORCELAIN, width=4))
CANON["accessories-text-editor"] = Icon(
    GROUP, stroke("M15 8h23l11 11v32q0 5-5 5H15q-5 0-5-5V13q0-5 5-5zM38 8v13h11", width=4)
    + stroke("M19 28h17M19 36h12M38 49l13-19 6 5-13 19-8 3z", width=3),
    _material + rect(13, 12, 38, 46, rx=7, fill=BLUE)
    + filled("M15 6h23l11 11v32q0 5-5 5H15q-5 0-5-5V11q0-5 5-5z", fill="url(#paper)")
    + filled("M38 6v12h11z", fill=BLUE)
    + strokes("M19 27h17", "M19 35h13", "M19 43h9", color=INK, width=3)
    + filled("M37 48l14-20 7 5-14 20-9 4z", fill=AMBER)
    + stroke("M38 49l5 4", color=INK, width=2))
CANON["utilities-terminal"] = Icon(
    GROUP, rect(7, 10, 50, 45, rx=10, width=4)
    + stroke("M8 21h48M19 30l8 7-8 7M35 44h10", width=4),
    _material + rect(7, 10, 50, 45, rx=10, fill="url(#glass)")
    + rect(10, 13, 44, 39, rx=8, stroke_color=BLUE, width=1.5)
    + stroke("M12 20h40", color=PORCELAIN, width=1.5)
    + dot(15,16,1.2,PORCELAIN) + dot(20,16,1.2,PORCELAIN)
    + stroke("M19 29l8 7-8 7", color=PORCELAIN, width=4)
    + stroke("M35 43h10", color=AMBER, width=4))

ALIASES = {
    "application-launcher": ("start-here-kde", None),
    "org.qindaqt.FileManager": ("system-file-manager", None),
    "org.qindaqt.TextEditor": ("accessories-text-editor", None),
    "org.qindaqt.Settings": ("preferences-system", None),
    "org.qindaqt.Terminal": ("utilities-terminal", None),
}
