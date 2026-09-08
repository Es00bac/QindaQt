#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Canonical MIME-type glyphs: a shared document body with a type-specific mark."""
from qinda_icon_shapes import APRICOT, BLUE, DOC_D, DOC_FOLD_D, INK, JADE, PORCELAIN, VIOLET, Icon, dot, filled, rect, stroke, strokes

GROUP = "mimetypes"


def _doc(mark_symbolic: str, mark_color: str, accent: str) -> Icon:
    return Icon(GROUP, stroke(DOC_D) + stroke(DOC_FOLD_D) + mark_symbolic,
                filled(DOC_D, fill=PORCELAIN) + filled(DOC_FOLD_D, fill=accent) + mark_color)


CANON = {
    "text-x-generic": _doc(strokes("M24 30h20", "M24 38h20", "M24 46h12", width=3),
                            strokes("M24 30h20", "M24 38h20", "M24 46h12", color=BLUE, width=3), BLUE),
    "text-x-script": _doc(stroke("M24 36 18 40 24 44") + stroke("M38 36 44 40 38 44"),
                           stroke("M24 36 18 40 24 44", color=INK) + stroke("M38 36 44 40 38 44", color=INK), INK),
    "application-pdf": _doc(
        stroke("M26 42v8h12v-8") + stroke("M32 32v14") + stroke("M27 40 32 46 37 40"),
        stroke("M26 42v8h12v-8", color=APRICOT) + stroke("M32 32v14", color=APRICOT) + stroke("M27 40 32 46 37 40", color=APRICOT), APRICOT,
    ),
    "application-zip": _doc(
        stroke("M32 26v28", width=3) + strokes("M28 30h8", "M28 36h8", "M28 42h8", width=2),
        stroke("M32 26v28", color=VIOLET, width=3) + strokes("M28 30h8", "M28 36h8", "M28 42h8", color=VIOLET, width=2), VIOLET,
    ),
    "image-x-generic": _doc(
        stroke("M20 42 26 34 31 39 36 32 42 42", width=3) + dot(38, 26, 2),
        stroke("M20 42 26 34 31 39 36 32 42 42", color=JADE, width=3) + dot(38, 26, 2, JADE), JADE,
    ),
    "audio-x-generic": _doc(
        dot(24, 42, 3) + stroke("M27 42V28l10-2", width=3) + dot(37, 40, 3),
        dot(24, 42, 3, BLUE) + stroke("M27 42V28l10-2", color=BLUE, width=3) + dot(37, 40, 3, BLUE), BLUE,
    ),
    "video-x-generic": _doc(filled("M26 32 40 38 26 44z"), filled("M26 32 40 38 26 44z", fill=APRICOT), APRICOT),
    "application-x-desktop": _doc(
        rect(26, 34, 12, 10, rx=2) + dot(32, 39, 1.5),
        rect(26, 34, 12, 10, rx=2, fill=JADE) + dot(32, 39, 1.5, PORCELAIN), JADE,
    ),
    "text-x-readme": _doc(
        stroke("M32 34v12M26 37l12 6M38 37l-12 6", width=3),
        stroke("M32 34v12M26 37l12 6M38 37l-12 6", color=VIOLET, width=3), VIOLET,
    ),
}

ALIASES = {
    "application-x-archive": ("application-zip", None),
    "application-octet-stream": ("text-x-generic", None),
    "text-plain": ("text-x-generic", None),
    "inode-directory": ("folder", "mimetypes"),
}
