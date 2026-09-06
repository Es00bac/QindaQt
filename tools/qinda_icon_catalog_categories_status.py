#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Canonical category and status glyphs.

AGENT-NOTE: earlier drafts drew every `applications-*` category as the same
mountain silhouette. Each category below is its own object; only the
"offline = base glyph + diagonal slash" and "settings-for-X" conventions are
deliberately reused, and only where the reused motif is genuinely the same
semantic modifier.
"""
from qinda_icon_shapes import (
    APRICOT, BELL_CLAPPER_D, BELL_D, BLUE, INK, JADE, PORCELAIN, VIOLET,
    Icon, dot, filled, ring, rect, stroke, strokes,
)

CATEGORY_GROUP = "categories"
STATUS_GROUP = "status"

CANON = {
    "applications-development": Icon(
        CATEGORY_GROUP, stroke("M22 22 12 32 22 42") + stroke("M42 22 52 32 42 42") + stroke("M28 46 36 18"),
        stroke("M22 22 12 32 22 42", color=JADE, width=6) + stroke("M42 22 52 32 42 42", color=JADE, width=6) + stroke("M28 46 36 18", color=INK, width=5),
    ),
    "applications-graphics": Icon(
        CATEGORY_GROUP, stroke("M18 46 36 28", width=6) + stroke("M34 26 44 16 48 20 38 30z"),
        stroke("M18 46 36 28", color=INK, width=6) + filled("M34 26 44 16 48 20 38 30z", fill=APRICOT),
    ),
    "applications-internet": Icon(
        CATEGORY_GROUP, ring(32, 32, 20) + stroke("M12 32h40") + strokes("M32 12c-8 5-8 35 0 40", "M32 12c8 5 8 35 0 40", width=4),
        ring(32, 32, 20, width=7, color=BLUE) + stroke("M12 32h40", width=4) + strokes("M32 12c-8 5-8 35 0 40", "M32 12c8 5 8 35 0 40", width=4),
    ),
    "applications-multimedia": Icon(
        CATEGORY_GROUP, ring(32, 32, 20) + filled("M26 22 46 32 26 42z"),
        ring(32, 32, 20, width=7, color=VIOLET) + filled("M26 22 46 32 26 42z", fill=VIOLET),
    ),
    "applications-office": Icon(
        CATEGORY_GROUP, rect(26, 14, 22, 34, rx=3) + rect(16, 20, 22, 34, rx=3),
        rect(26, 14, 22, 34, rx=3, fill=JADE) + rect(16, 20, 22, 34, rx=3, fill=PORCELAIN, stroke_color=INK, width=3),
    ),
    "applications-science": Icon(
        CATEGORY_GROUP, stroke("M28 12h8v14l10 22a4 4 0 0 1-4 6H22a4 4 0 0 1-4-6l10-22z") + stroke("M25 34h14", width=3),
        filled("M28 12h8v14l10 22a4 4 0 0 1-4 6H22a4 4 0 0 1-4-6l10-22z", fill=BLUE) + stroke("M25 34h14", color=PORCELAIN, width=3),
    ),
    "applications-system": Icon(
        CATEGORY_GROUP,
        ring(24, 24, 10) + strokes("M24 16v4", "M24 28v4", "M16 24h4", "M28 24h4", width=3) + rect(34, 30, 20, 20, rx=3) + stroke("M40 36 44 40 40 44", width=3),
        ring(24, 24, 10, width=5, color=INK) + strokes("M24 16v4", "M24 28v4", "M16 24h4", "M28 24h4", width=3)
        + rect(34, 30, 20, 20, rx=3, fill=INK) + stroke("M40 36 44 40 40 44", color=JADE, width=3),
    ),
    "applications-utilities": Icon(
        CATEGORY_GROUP, stroke("M44 14a8 8 0 0 1-10 10L18 40l-4 4 6 6 4-4 16-16a8 8 0 0 1 10-10z"),
        filled("M44 14a8 8 0 0 1-10 10L18 40l-4 4 6 6 4-4 16-16a8 8 0 0 1 10-10z", fill=APRICOT),
    ),
    "network-offline": Icon(
        STATUS_GROUP,
        ring(32, 32, 18) + stroke("M14 32h36") + strokes("M32 14c-8 5-8 35 0 40", "M32 14c8 5 8 35 0 40", width=4) + stroke("M12 12 52 52", width=6),
        ring(32, 32, 18, width=6, color=BLUE) + stroke("M14 32h36", width=4)
        + strokes("M32 14c-8 5-8 35 0 40", "M32 14c8 5 8 35 0 40", width=4) + stroke("M12 12 52 52", color=APRICOT, width=6),
    ),
    "network-transmit-receive": Icon(
        STATUS_GROUP, stroke("M22 44V16") + stroke("M16 22 22 16 28 22") + stroke("M42 20v28") + stroke("M36 42 42 48 48 42"),
        stroke("M22 44V16", color=JADE, width=6) + stroke("M16 22 22 16 28 22", color=JADE, width=6)
        + stroke("M42 20v28", color=BLUE, width=6) + stroke("M36 42 42 48 48 42", color=BLUE, width=6),
    ),
    "network-error": Icon(
        STATUS_GROUP,
        ring(28, 28, 14) + stroke("M16 28h24", width=3) + strokes("M28 16c-5 3-5 19 0 24", "M28 16c5 3 5 19 0 24", width=3)
        + stroke("M50 16v12", width=4) + dot(50, 34, 2),
        ring(28, 28, 14, width=5, color=BLUE) + stroke("M16 28h24", width=3) + strokes("M28 16c-5 3-5 19 0 24", "M28 16c5 3 5 19 0 24", width=3)
        + stroke("M50 16v12", color=APRICOT, width=4) + dot(50, 34, 2, APRICOT),
    ),
    "security-high": Icon(
        STATUS_GROUP, stroke("M32 10 52 18v14c0 14-9 22-20 26-11-4-20-12-20-26V18z") + stroke("M24 34 30 40 42 26", width=4),
        filled("M32 10 52 18v14c0 14-9 22-20 26-11-4-20-12-20-26V18z", fill=JADE) + stroke("M24 34 30 40 42 26", color=PORCELAIN, width=4),
    ),
    "emblem-default": Icon(STATUS_GROUP, ring(32, 32, 18) + stroke("M22 33 29 41 44 24", width=5),
                             ring(32, 32, 18, width=6, color=JADE) + stroke("M22 33 29 41 44 24", width=5)),
    "emblem-important": Icon(STATUS_GROUP, ring(32, 32, 18) + stroke("M32 22v14", width=5) + dot(32, 42, 2.5),
                               ring(32, 32, 18, width=6, color=APRICOT) + stroke("M32 22v14", width=5) + dot(32, 42, 2.5)),
    "emblem-synchronized": Icon(
        STATUS_GROUP,
        stroke("M46 20a16 16 0 1 1 -28 4") + stroke("M20 16l-2 10 10-2") + stroke("M18 44a16 16 0 1 1 28-4") + stroke("M44 48l2-10-10 2"),
        stroke("M46 20a16 16 0 1 1 -28 4", color=BLUE, width=6) + stroke("M20 16l-2 10 10-2", color=BLUE, width=6)
        + stroke("M18 44a16 16 0 1 1 28-4", color=BLUE, width=6) + stroke("M44 48l2-10-10 2", color=BLUE, width=6),
    ),
    "process-working": Icon(
        STATUS_GROUP, stroke("M32 12a20 20 0 1 1 -14 6"),
        stroke("M32 12a20 20 0 1 1 -14 6", color=VIOLET, width=7),
    ),
    "software-update-available": Icon(
        STATUS_GROUP, stroke("M32 12v26") + stroke("M22 28 32 38 42 28") + stroke("M16 46h32", width=4),
        stroke("M32 12v26", color=JADE, width=6) + stroke("M22 28 32 38 42 28", color=JADE, width=6) + stroke("M16 46h32", color=INK, width=4),
    ),
    "notification-new": Icon(
        STATUS_GROUP, stroke(BELL_D, width=5) + stroke(BELL_CLAPPER_D, width=5) + dot(46, 14, 4),
        filled(BELL_D, fill=JADE) + stroke(BELL_CLAPPER_D, color=INK, width=4) + dot(46, 14, 4, APRICOT),
    ),
}

ALIASES = {
    "emblem-warning": ("dialog-warning", "status"),
}
