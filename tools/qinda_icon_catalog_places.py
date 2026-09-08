#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Canonical place glyphs: folders with semantic marks, trash, hosts.

AGENT-NOTE: `user-trash` renders `TRASH_D`, a bin silhouette, never the
folder outline -- conflating the two was the withdrawn candidate's defect.
"""
from qinda_icon_shapes import APRICOT, BLUE, FOLDER_D, INK, PORCELAIN, TRASH_D, Icon, dot, filled, ring, rect, stroke, strokes

GROUP = "places"


def _folder(mark_symbolic: str, mark_color: str) -> Icon:
    material = (f'<defs><linearGradient id="folder-material" x2="0" y2="1">'
                f'<stop stop-color="{PORCELAIN}"/><stop offset="1" stop-color="{APRICOT}"/>'
                f'</linearGradient></defs>')
    return Icon(GROUP, stroke(FOLDER_D) + mark_symbolic,
                material + filled(FOLDER_D, fill=APRICOT)
                + rect(11, 23, 42, 28, rx=7, fill="url(#folder-material)")
                + stroke("M16 27h32", color=PORCELAIN, width=2)
                + mark_color)


CANON = {
    "folder": _folder("", ""),
    "folder-documents": _folder(strokes("M20 34h16", "M20 40h10", width=3), strokes("M20 34h16", "M20 40h10", color=INK, width=3)),
    "folder-download": _folder(stroke("M28 28v10") + stroke("M22 34 28 40 34 34"), stroke("M28 28v10", color=INK) + stroke("M22 34 28 40 34 34", color=INK)),
    "folder-music": _folder(
        dot(24, 40, 3) + stroke("M27 40V24l10-2", width=3) + dot(37, 38, 3),
        dot(24, 40, 3, INK) + stroke("M27 40V24l10-2", color=INK, width=3) + dot(37, 38, 3, INK),
    ),
    "folder-pictures": _folder(
        stroke("M20 42 28 32 34 38 40 30 46 42", width=3) + dot(40, 26, 2),
        stroke("M20 42 28 32 34 38 40 30 46 42", color=INK, width=3) + dot(40, 26, 2, INK),
    ),
    "folder-videos": _folder(filled("M26 30 40 36 26 42z"), filled("M26 30 40 36 26 42z", fill=INK)),
    "folder-publicshare": _folder(
        dot(26, 32, 3) + dot(38, 32, 3) + stroke("M20 42c0-5 4-8 8-8M44 42c0-5-4-8-8-8", width=3),
        dot(26, 32, 3, INK) + dot(38, 32, 3, INK) + stroke("M20 42c0-5 4-8 8-8M44 42c0-5-4-8-8-8", color=INK, width=3),
    ),
    "folder-remote": _folder(
        stroke("M22 40a6 6 0 0 1 1-12 8 8 0 0 1 15-2 6 6 0 0 1 2 14z", width=3),
        stroke("M22 40a6 6 0 0 1 1-12 8 8 0 0 1 15-2 6 6 0 0 1 2 14z", color=INK, width=3),
    ),
    "user-desktop": Icon(
        GROUP, rect(12, 12, 40, 26, rx=3) + stroke("M26 44h12M32 38v6", width=4),
        rect(12, 12, 40, 26, rx=3, fill=BLUE) + stroke("M26 44h12M32 38v6", color=INK, width=4),
    ),
    "user-home": Icon(
        GROUP, stroke("M14 32 32 16 50 32") + rect(22, 32, 20, 18) + stroke("M42 22v-8", width=4),
        stroke("M14 32 32 16 50 32", color=APRICOT, width=6) + rect(22, 32, 20, 18, fill=BLUE) + stroke("M42 22v-8", color=APRICOT, width=4),
    ),
    "user-trash": Icon(
        GROUP,
        stroke(TRASH_D) + stroke("M12 20h40") + stroke("M26 20v-4h12v4") + strokes("M24 26v20", "M32 26v20", "M40 26v20", width=3),
        filled(TRASH_D, fill=BLUE) + stroke("M12 20h40", width=4) + stroke("M26 20v-4h12v4", width=4)
        + strokes("M24 26v20", "M32 26v20", "M40 26v20", color=PORCELAIN, width=3),
    ),
    "network-server": Icon(
        GROUP, rect(16, 10, 32, 44, rx=4) + strokes("M16 22h32", "M16 34h32", "M16 46h32", width=3) + dot(44, 16, 2),
        rect(16, 10, 32, 44, rx=4, fill=BLUE) + strokes("M16 22h32", "M16 34h32", "M16 46h32", color=PORCELAIN, width=3) + dot(44, 16, 2, APRICOT),
    ),
    "computer": Icon(
        GROUP, rect(20, 8, 24, 48, rx=4) + ring(32, 16, 3) + strokes("M24 28h16", "M24 36h16", width=3),
        rect(20, 8, 24, 48, rx=4, fill=BLUE) + ring(32, 16, 3, color=PORCELAIN, width=3) + strokes("M24 28h16", "M24 36h16", color=PORCELAIN, width=3),
    ),
}

ALIASES: dict[str, tuple[str, str | None]] = {}
