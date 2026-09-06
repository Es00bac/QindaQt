#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Canonical action glyphs: edits, navigation, windows, dialogs, session."""
from qinda_icon_shapes import (
    APRICOT, BLUE, DOC_D, DOC_FOLD_D, FOLDER_D, INK, JADE, MOON_D, PORCELAIN, VIOLET,
    Icon, dot, filled, ring, rect, stroke, strokes,
)

GROUP = "actions"


def _window(mark_symbolic: str, mark_color: str) -> Icon:
    frame = rect(10, 10, 44, 44, rx=12)
    return Icon(GROUP, frame + mark_symbolic, rect(10, 10, 44, 44, rx=12, fill=BLUE) + mark_color)


CANON = {
    "window-close": _window(
        stroke("M22 22 42 42M42 22 22 42"),
        stroke("M22 22 42 42M42 22 22 42", color=PORCELAIN),
    ),
    "window-minimize": _window(stroke("M20 42h24"), stroke("M20 42h24", color=PORCELAIN)),
    "window-maximize": _window(rect(20, 20, 24, 24), rect(20, 20, 24, 24, stroke_color=PORCELAIN, width=5)),
    "window-restore": _window(
        rect(18, 24, 20, 20) + rect(26, 16, 20, 20),
        rect(18, 24, 20, 20, stroke_color=PORCELAIN, width=4) + rect(26, 16, 20, 20, stroke_color=PORCELAIN, width=4),
    ),
    "edit-cut": Icon(
        GROUP,
        ring(21, 45, 6) + ring(43, 45, 6) + stroke("M18 18 45 41") + stroke("M46 18 19 41"),
        ring(21, 45, 6, color=INK) + ring(43, 45, 6, color=INK) + strokes("M18 18 45 41", "M46 18 19 41", color=BLUE, width=5),
    ),
    "edit-copy": Icon(
        GROUP,
        rect(22, 8, 28, 36, rx=4) + rect(12, 18, 28, 36, rx=4),
        rect(22, 8, 28, 36, rx=4, fill=BLUE) + rect(12, 18, 28, 36, rx=4, fill=PORCELAIN, stroke_color=INK, width=3),
    ),
    "edit-paste": Icon(
        GROUP,
        rect(14, 14, 36, 42, rx=6) + rect(24, 8, 16, 10, rx=3, fill=INK) + strokes("M22 30h20", "M22 38h14", width=4),
        rect(14, 14, 36, 42, rx=6, fill=PORCELAIN, stroke_color=INK, width=3) + rect(24, 8, 16, 10, rx=3, fill=BLUE)
        + strokes("M22 30h20", "M22 38h14", color=BLUE, width=4),
    ),
    "edit-undo": Icon(
        GROUP,
        stroke("M44 24a16 16 0 1 0 -6 20") + stroke("M30 36l-8 4 2-9"),
        stroke("M44 24a16 16 0 1 0 -6 20", color=BLUE, width=7) + stroke("M30 36l-8 4 2-9", color=BLUE, width=7),
    ),
    "edit-redo": Icon(
        GROUP,
        stroke("M20 24a16 16 0 1 1 6 20") + stroke("M34 36l8 4-2-9"),
        stroke("M20 24a16 16 0 1 1 6 20", color=BLUE, width=7) + stroke("M34 36l8 4-2-9", color=BLUE, width=7),
    ),
    "edit-find": Icon(
        GROUP,
        ring(26, 26, 12) + stroke("M35 35 48 48"),
        ring(26, 26, 12, width=7, color=BLUE) + stroke("M35 35 48 48", color=INK, width=7),
    ),
    "edit-find-replace": Icon(
        GROUP,
        ring(22, 22, 10) + stroke("M30 30 40 40") + stroke("M50 10a8 8 0 1 0 -8 8") + stroke("M46 4 50 10 44 12"),
        ring(22, 22, 10, width=6, color=BLUE) + stroke("M30 30 40 40", width=6) + stroke("M50 10a8 8 0 1 0 -8 8", color=APRICOT, width=5)
        + stroke("M46 4 50 10 44 12", color=APRICOT, width=5),
    ),
    "edit-select-all": Icon(
        GROUP,
        strokes("M14 14h10", "M14 14v10", "M50 14h-10", "M50 14v10", "M14 50h10", "M14 50v-10", "M50 50h-10", "M50 50v-10", width=5),
        strokes("M14 14h10", "M14 14v10", "M50 14h-10", "M50 14v10", "M14 50h10", "M14 50v-10", "M50 50h-10", "M50 50v-10", color=BLUE, width=6),
    ),
    "document-new": Icon(
        GROUP,
        stroke(DOC_D) + stroke(DOC_FOLD_D) + strokes("M32 28v16", "M24 36h16", width=5),
        filled(DOC_D, fill=PORCELAIN) + filled(DOC_FOLD_D, fill=BLUE) + strokes("M32 28v16", "M24 36h16", color=JADE, width=5),
    ),
    "document-open": Icon(
        GROUP,
        stroke(FOLDER_D) + stroke("M26 20h14v10", width=4),
        filled(FOLDER_D, fill=BLUE) + stroke("M26 20h14v10", color=PORCELAIN, width=4),
    ),
    "document-save": Icon(
        GROUP,
        stroke("M12 10h32l8 8v36H12z") + rect(22, 12, 16, 10) + rect(18, 32, 28, 16),
        filled("M12 10h32l8 8v36H12z", fill=INK) + rect(22, 12, 16, 10, fill=PORCELAIN) + rect(18, 32, 28, 16, fill=JADE),
    ),
    "document-save-as": Icon(
        GROUP,
        stroke("M12 10h32l8 8v36H12z") + rect(22, 12, 16, 10) + rect(18, 32, 28, 16) + strokes("M50 8v8", "M46 12h8", width=4),
        filled("M12 10h32l8 8v36H12z", fill=INK) + rect(22, 12, 16, 10, fill=PORCELAIN) + rect(18, 32, 28, 16, fill=JADE)
        + strokes("M50 8v8", "M46 12h8", color=APRICOT, width=4),
    ),
    "document-print": Icon(
        GROUP,
        rect(20, 10, 24, 14) + rect(12, 24, 40, 20, rx=4) + rect(20, 44, 24, 12) + dot(46, 32, 2),
        rect(20, 10, 24, 14, fill=PORCELAIN, stroke_color=INK, width=3) + rect(12, 24, 40, 20, rx=4, fill=BLUE)
        + rect(20, 44, 24, 12, fill=PORCELAIN, stroke_color=INK, width=3) + dot(46, 32, 2, JADE),
    ),
    "go-next": Icon(GROUP, stroke("M14 32h30") + stroke("M34 18 50 32 34 46"),
                     stroke("M14 32h30", color=BLUE, width=7) + stroke("M34 18 50 32 34 46", color=BLUE, width=7)),
    "go-previous": Icon(GROUP, stroke("M50 32h-30") + stroke("M30 18 14 32 30 46"),
                         stroke("M50 32h-30", color=BLUE, width=7) + stroke("M30 18 14 32 30 46", color=BLUE, width=7)),
    "go-up": Icon(GROUP, stroke("M32 50v-30") + stroke("M18 30 32 14 46 30"),
                   stroke("M32 50v-30", color=BLUE, width=7) + stroke("M18 30 32 14 46 30", color=BLUE, width=7)),
    "go-down": Icon(GROUP, stroke("M32 14v30") + stroke("M18 34 32 50 46 34"),
                     stroke("M32 14v30", color=BLUE, width=7) + stroke("M18 34 32 50 46 34", color=BLUE, width=7)),
    "go-home": Icon(
        GROUP,
        stroke("M14 30 32 14 50 30") + rect(20, 30, 24, 20) + stroke("M28 50v-12h8v12"),
        stroke("M14 30 32 14 50 30", color=APRICOT, width=6) + rect(20, 30, 24, 20, fill=BLUE)
        + stroke("M28 50v-12h8v12", color=PORCELAIN, width=4),
    ),
    "view-grid": Icon(
        GROUP,
        "".join(dot(16 + 16 * col, 16 + 16 * row, 3) for row in range(3) for col in range(3)),
        "".join(dot(16 + 16 * col, 16 + 16 * row, 3, BLUE) for row in range(3) for col in range(3)),
    ),
    "view-list": Icon(
        GROUP,
        strokes("M22 18h28", "M22 32h28", "M22 46h28", width=5) + dot(14, 18, 2.5) + dot(14, 32, 2.5) + dot(14, 46, 2.5),
        strokes("M22 18h28", "M22 32h28", "M22 46h28", color=BLUE, width=5) + dot(14, 18, 2.5, APRICOT) + dot(14, 32, 2.5, APRICOT) + dot(14, 46, 2.5, APRICOT),
    ),
    "view-more": Icon(GROUP, dot(18, 32, 4) + dot(32, 32, 4) + dot(46, 32, 4),
                        dot(18, 32, 4, BLUE) + dot(32, 32, 4, BLUE) + dot(46, 32, 4, BLUE)),
    "view-fullscreen": Icon(
        GROUP,
        strokes("M26 26 14 14", "M14 14h8", "M14 14v8", "M38 26 50 14", "M50 14h-8", "M50 14v8",
                "M26 38 14 50", "M14 50h8", "M14 50v-8", "M38 38 50 50", "M50 50h-8", "M50 50v-8", width=5),
        strokes("M26 26 14 14", "M14 14h8", "M14 14v8", "M38 26 50 14", "M50 14h-8", "M50 14v8",
                "M26 38 14 50", "M14 50h8", "M14 50v-8", "M38 38 50 50", "M50 50h-8", "M50 50v-8", color=BLUE, width=5),
    ),
    # A single circular arrow: "refresh this view", not "restart the system"
    # (system-reboot, which keeps its own power tick) and not "two items are
    # in sync" (emblem-synchronized's double-arrow loop).
    "view-refresh": Icon(
        GROUP,
        stroke("M46 22a18 18 0 1 1 -6-13") + stroke("M42 6l2 7-7-1"),
        stroke("M46 22a18 18 0 1 1 -6-13", color=BLUE, width=7) + stroke("M42 6l2 7-7-1", color=BLUE, width=7),
    ),
    # A dashboard's characteristic asymmetric widget layout, distinct from
    # view-grid's uniform 3x3 dots and virtual-desktops' four equal tiles.
    "dashboard-show": Icon(
        GROUP,
        rect(10, 10, 44, 44, rx=6) + rect(14, 14, 20, 36, rx=3) + rect(38, 14, 16, 16, rx=3) + rect(38, 34, 16, 16, rx=3),
        rect(10, 10, 44, 44, rx=6, fill=BLUE) + rect(14, 14, 20, 36, rx=3, fill=PORCELAIN)
        + rect(38, 14, 16, 16, rx=3, fill=PORCELAIN) + rect(38, 34, 16, 16, rx=3, fill=PORCELAIN),
    ),
    "help-about": Icon(
        GROUP,
        ring(32, 32, 20) + stroke("M25 24a8 8 0 1 1 10 7c-3 2-3 4-3 7", width=5) + dot(32, 46, 2.5),
        ring(32, 32, 20, width=7, color=JADE) + stroke("M25 24a8 8 0 1 1 10 7c-3 2-3 4-3 7", width=5) + dot(32, 46, 2.5),
    ),
    "dialog-information": Icon(
        GROUP,
        ring(32, 32, 20) + dot(32, 20, 3) + stroke("M32 28v16", width=5),
        ring(32, 32, 20, width=7, color=BLUE) + dot(32, 20, 3) + stroke("M32 28v16", width=5),
    ),
    "dialog-warning": Icon(
        GROUP,
        stroke("M32 10 54 50H10z") + stroke("M32 26v14", width=5) + dot(32, 44, 2.5),
        filled("M32 10 54 50H10z", fill=APRICOT) + stroke("M32 26v14", color=INK, width=5) + dot(32, 44, 2.5),
    ),
    "dialog-error": Icon(
        GROUP,
        ring(32, 32, 20) + stroke("M24 24 40 40M40 24 24 40"),
        ring(32, 32, 20, width=7, color=APRICOT) + stroke("M24 24 40 40M40 24 24 40", width=7),
    ),
    "system-lock-screen": Icon(
        GROUP,
        rect(18, 30, 28, 22, rx=4) + stroke("M22 30v-8a10 10 0 0 1 20 0v8") + dot(32, 41, 3),
        rect(18, 30, 28, 22, rx=4, fill=BLUE) + stroke("M22 30v-8a10 10 0 0 1 20 0v8", width=6) + dot(32, 41, 3, PORCELAIN),
    ),
    "system-log-out": Icon(
        GROUP,
        rect(14, 10, 20, 44, rx=3) + stroke("M34 32h18") + stroke("M46 24 54 32 46 40"),
        rect(14, 10, 20, 44, rx=3, fill=BLUE) + stroke("M34 32h18", color=INK, width=6) + stroke("M46 24 54 32 46 40", color=INK, width=6),
    ),
    "system-reboot": Icon(
        GROUP,
        stroke("M46 22a18 18 0 1 1 -10-9") + stroke("M40 9l-4 6 8 2") + stroke("M32 10v10", width=6),
        stroke("M46 22a18 18 0 1 1 -10-9", color=JADE, width=7) + stroke("M40 9l-4 6 8 2", color=JADE, width=7)
        + stroke("M32 10v10", color=JADE, width=6),
    ),
    "system-shutdown": Icon(
        GROUP,
        stroke("M46 22a16 16 0 1 1 -28 0") + stroke("M32 14v16"),
        stroke("M46 22a16 16 0 1 1 -28 0", color=APRICOT, width=7) + stroke("M32 14v16", color=APRICOT, width=7),
    ),
    "system-suspend": Icon(GROUP, filled(MOON_D, fill=INK), filled(MOON_D, fill=VIOLET)),
    "virtual-desktops": Icon(
        GROUP,
        rect(10, 10, 20, 16, rx=2) + rect(34, 10, 20, 16, rx=2) + rect(10, 34, 20, 16, rx=2) + rect(34, 34, 20, 16, rx=2) + dot(20, 18, 2),
        rect(10, 10, 20, 16, rx=2, fill=BLUE) + rect(34, 10, 20, 16, rx=2, fill=BLUE) + rect(10, 34, 20, 16, rx=2, fill=BLUE)
        + rect(34, 34, 20, 16, rx=2, fill=BLUE) + dot(20, 18, 2, APRICOT),
    ),
}

ALIASES = {
    "system-search": ("edit-find", None),
}
