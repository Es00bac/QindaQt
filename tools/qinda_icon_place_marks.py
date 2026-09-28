#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Single-tone emblems stamped on the front of special folders.

AGENT-CONTRACT: every mark is a function of one color and returns a
fragment that sits inside the folder front's emblem box (x 19..45,
y 28..52). A mark must be ONE tone: the same fragment is drawn in the
folder's emblem tone for color artwork and in INK over an outline folder
for the `-symbolic` cut, where the runtime's source-in recolor would turn
any second "hole" color into solid ink (see qinda_icon_shapes.py). Real
holes use `fill-rule="evenodd"` or strokes.
"""
from qinda_icon_shapes import circle_d, dot, filled, ring, rect, stroke, strokes


def home(c: str) -> str:
    # Roof, walls and a real evenodd door so the house reads at 16 px.
    return filled("M32 27 18 39h4v12h20V39h4zM29 51v-7h6v7z", c, rule="evenodd")


def desktop(c: str) -> str:
    return rect(19, 29, 26, 17, rx=2.5, fill=c) + filled("M28 46h8l1.5 5h-11z", c)


def documents(c: str) -> str:
    return filled("M22 27h14l6 6v19H22zM26 36v2.6h12V36zm0 5v2.6h12V41zm0 5v2.6h8V46z", c, rule="evenodd")


def download(c: str) -> str:
    return stroke("M32 28v15", 4.5, c) + stroke("M24 36l8 8 8-8", 4.5, c) + stroke("M21 50h22", 4, c)


def pictures(c: str) -> str:
    return filled("M19 50l9-12 6 7 4-5 7 10z", c) + dot(37, 33, 4, c)


def videos(c: str) -> str:
    return filled("M20 30h24v20H20zM28 34v12l10-6z", c, rule="evenodd")


def music(c: str) -> str:
    return dot(26, 47, 4.5, c) + dot(39, 44, 4.5, c) + stroke("M30 47V31l13-3v16", 3.5, c, cap="butt")


def books(c: str) -> str:
    # Two pages with a real gutter gap, never a painted spine line.
    return filled("M19 31q7-3 12 1v19q-5-4-12-1zM33 32q5-4 12-1v19q-7-3-12 1z", c)


def templates(c: str) -> str:
    # A set square over a ruler: a tool for making things from a pattern.
    return filled("M20 51V28l23 23zM25 46h7l-7-7z", c, rule="evenodd") + rect(34, 28, 7, 16, rx=1, fill=c)


def publicshare(c: str) -> str:
    return (dot(26, 34, 4, c) + dot(38, 34, 4, c)
            + filled("M18 50q0-10 8-10t8 10zM30 50q0-10 8-10t8 10z", c))


def projects(c: str) -> str:
    # A lightbulb: projects are ideas in progress, unlike code in development.
    return filled("M32 27a9 9 0 0 0-5 16.5V46h10v-2.5A9 9 0 0 0 32 27z", c) + rect(27, 48, 10, 3.5, rx=1.5, fill=c)


def games(c: str) -> str:
    return filled("M24 32h16q6 0 7 7l1 7q0 4-4 3l-6-5H26l-6 5q-4 1-4-3l1-7q1-7 7-7z"
                  "M24 36v3h-3v3h3v3h3v-3h3v-3h-3v-3z" + circle_d(39, 39, 2) + circle_d(43.5, 43, 2), c, rule="evenodd")


def development(c: str) -> str:
    return strokes("M26 32l-7 8 7 8", "M38 32l7 8-7 8", "M35 29l-6 22", width=3.6, color=c)


def remote(c: str) -> str:
    return ring(32, 40, 10.5, 3, c) + strokes("M21.5 40h21", "M32 29.5q-7 10.5 0 21", "M32 29.5q7 10.5 0 21", width=2.4, color=c)


def cloud(c: str) -> str:
    return filled("M24 49q-7 0-7-6.5t6.5-6.5q1-8 9-8t9.5 7.5q6.5 0 6.5 6.5T42 49z", c)


def work(c: str) -> str:
    return filled("M27 33v-3q0-2 2-2h6q2 0 2 2v3h8v17H19V33zM30 31v2h4v-2z", c, rule="evenodd")


def favorites(c: str) -> str:
    return filled("M32 27l4 8.2 9 1.3-6.5 6.3 1.5 9-8-4.2-8 4.2 1.5-9-6.5-6.3 9-1.3z", c)


def locked(c: str) -> str:
    return filled("M23 38h18v13H23z", c) + stroke("M26.5 38v-4q0-6 5.5-6t5.5 6v4", 3.5, c)


def backup(c: str) -> str:
    return stroke("M22 40a10 10 0 1 0 3-7", 3.6, c) + filled("M19 28l1 9 8-3z", c) + stroke("M32 34v6l4 3", 3, c)


def camera(c: str) -> str:
    return filled("M20 34h6l3-4h6l3 4h6v16H20z" + circle_d(32, 42, 5), c, rule="evenodd")


def applications(c: str) -> str:
    return rect(19, 29, 11, 11, rx=3, fill=c) + rect(34, 29, 11, 11, rx=3, fill=c) + dot(24.5, 46, 4.5, c) + rect(34, 41, 11, 11, rx=3, fill=c)


def wine(c: str) -> str:
    return filled("M24 28h16l-1 9q-1 6-7 6t-7-6z", c) + stroke("M32 43v6M26 51h12", 3, c)
