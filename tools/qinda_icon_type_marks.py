#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Content glyphs drawn on the shared file-type page.

AGENT-CONTRACT: every glyph is a function of one tone and draws inside the
page's glyph box (x 17..47, y 14..42), above the colored band. Like the
folder emblems, a glyph is single-tone so the same function serves the color
art (band-derived tone) and the `-symbolic` cut (INK); holes are evenodd or
stroke gaps, never a second fill color. A type that genuinely needs two
colors (Python's two snakes, a terminal's green prompt) supplies a separate
color-only variant in qinda_icon_catalog_mimetypes.py.
"""
from qinda_icon_shapes import circle_d, dot, filled, gear_d, rect, ring, stroke, strokes


def _s(*paths: str, w: float = 3.4):
    return lambda c: strokes(*paths, width=w, color=c)


text_lines = _s("M19 18h26", "M19 25h26", "M19 32h26", "M19 39h17")
paragraphs = _s("M25 18h20", "M19 24.5h26", "M19 31h26", "M19 37.5h19")
markdown = _s("M18 39V19l7.5 9 7.5-9v20", "M41 19v18", "M36 32l5 5 5-5", w=3.6)
c_lang = _s("M40 20q-3-4-8.5-4Q20 16 20 28t11.5 12q5.5 0 8.5-4", w=4.4)
cpp = _s("M31 21q-2-4-6.5-4Q18 17 18 28t6.5 11q4.5 0 6.5-4", "M33 28h6.5M36.25 24.75v6.5", "M40.5 28h6.5M43.75 24.75v6.5", w=3.4)
header = _s("M23 16v24", "M23 29q2-6 7.5-6t7.5 6v11", w=4.2)
javascript = _s("M27 18v14q0 7-7 7", "M44 20q-2-3-5.5-3Q34 17 34 22q0 4 5 5.5t5 5.5q0 5-5.5 5q-4 0-6-3", w=3.8)
typescript = _s("M17 18h14M24 18v21", "M45 20q-2-3-5.5-3Q35 17 35 22q0 4 5 5.5t5 5.5q0 5-5.5 5q-4 0-6-3", w=3.8)
go_lang = _s("M31 20.5q-2-3.5-6-3.5Q18 17 18 28t7 11q6 0 6.5-7h-6", "M39 17q7 0 7 11t-7 11-7-11 7-11z", w=3.6)
html = _s("M25 20l-7 8 7 8", "M39 20l7 8-7 8", "M35 17l-6 22", w=3.6)
css = _s("M28 17l-3 22", "M37 17l-3 22", "M20 24h22", "M19 32h22", w=3.2)
json = lambda c: (strokes("M27 17q-4 0-4 4v4q0 3-3 3 3 0 3 3v4q0 4 4 4",
                          "M37 17q4 0 4 4v4q0 3 3 3-3 0-3 3v4q0 4-4 4", width=3.2, color=c)
                  + dot(32, 24, 2.2, c) + dot(32, 33, 2.2, c))
xml = lambda c: (rect(26, 14, 12, 8, rx=1.5, fill=c) + rect(17, 33, 12, 8, rx=1.5, fill=c)
                 + rect(35, 33, 12, 8, rx=1.5, fill=c) + strokes("M32 22v5", "M23 27h18", "M23 27v6", "M41 27v6", width=2.6, color=c))
yaml = _s("M18 18h4", "M26 18h18", "M24 25h4", "M32 25h12", "M24 32h4", "M32 32h10", "M18 39h4", "M26 39h14", w=3.2)
toml = _s("M24 16h-5v24h5", "M40 16h5v24h-5", "M25 23h14", "M25 29h10", "M25 35h12", w=3.2)
rust = lambda c: filled(gear_d(32, 28, 14, 10.5, 9) + circle_d(32, 28, 5), c, rule="evenodd")
java = lambda c: (filled("M19 25h22v5q0 10-11 10T19 30z", c) + stroke("M41 27.5q5 0 5 4t-5 4", 3, c)
                  + strokes("M25 21q-2-2.5 0-5", "M30 21q-2-2.5 0-5", "M35 21q-2-2.5 0-5", width=2.4, color=c))
shell = _s("M20 21l7 6-7 6", "M30 36h12", w=3.8)
shebang = _s("M24 17l-3 22", "M30 17l-3 22", "M17 24h16", "M16 32h16", "M40 17v14", w=3.2)
script = lambda c: shebang(c) + dot(40, 38, 2.2, c)
csv = lambda c: "".join(strokes(f"M18 {y}h6", f"M31 {y}h5", f"M43 {y}h3", width=3.4, color=c)
                        + stroke(f"M28 {y - 1}q1.2 3.5-1 6", 2.2, c) + stroke(f"M40 {y - 1}q1.2 3.5-1 6", 2.2, c)
                        for y in (19, 28, 37))
sheet = lambda c: rect(18, 16, 28, 24, rx=1.5, stroke_color=c, width=2.6) + strokes("M18 24h28", "M18 32h28", "M27.5 16v24", "M37 16v24", width=2.2, color=c)
slides = lambda c: (rect(17, 15, 30, 20, rx=1.5, stroke_color=c, width=2.8)
                    + strokes("M24 30v-5", "M30 30v-9", "M36 30v-7", "M32 35v5", "M26 40.5h12", width=3, color=c))
word = _s("M17 18l4.5 21 5.5-15.5 5.5 15.5 4.5-21", "M41 22h6", "M41 29h6", w=3.6)
excel = _s("M20 18l15 21", "M35 18l-15 21", "M40 22h6", "M40 29h6", "M40 36h6", w=3.6)
powerpoint = lambda c: stroke("M22 39V18h8q7.5 0 7.5 6.5T30 31h-8", 3.8, c) + dot(43, 33, 4, c)
pdf = lambda c: stroke("M20 39c5-4 9-12 9-18 0-4-3-4-3-1 0 7 9 15 17 16 3 0 3-3 0-3-8 0-17 3-21 7-2 2-1 3 1 1", 2.8, c)
epub = lambda c: filled("M17 19q8-4 14 1v20q-6-4-14-1zM33 20q6-5 14-1v20q-8-3-14 1z", c)
mobi = lambda c: filled("M21 15h21q2 0 2 2v24H24q-3 0-3-3zM33 15v11l3-2.2 3 2.2V15z", c, rule="evenodd") + stroke("M24 41q-3 0-3-3", 1.5, c)
comic = lambda c: filled("M17 16h30v18H31l-8 7v-7h-6zM30.5 19h3.4v8.5h-3.4zM30.5 29.5h3.4v3h-3.4z", c, rule="evenodd")
png = lambda c: "".join(rect(18 + 9 * (i % 3), 15 + 9 * (i // 3), 9, 9, fill=c) for i in range(9) if i % 2 == 0)
jpeg = lambda c: filled("M17 41l9-14 6 8 5-6 9 12z", c) + dot(38, 21, 4.2, c)
gif = lambda c: rect(17, 15, 22, 17, rx=2, stroke_color=c, width=2.6) + filled("M24 21h23v20H24zM31.5 25.5v11l8.5-5.5z", c, rule="evenodd")
webp = lambda c: filled("M17 41v-9q7.5-9 15 0t15 0v9z", c) + dot(25, 21, 4.2, c) + stroke("M34 18h12", 2.6, c)
vector = lambda c: (stroke("M20 38q4-24 24 0", 2.8, c) + stroke("M20 20h24", 2, c)
                    + rect(16.5, 34.5, 7, 7, fill=c) + rect(40.5, 34.5, 7, 7, fill=c) + dot(20, 20, 3, c) + dot(44, 20, 3, c))
aperture = lambda c: filled(circle_d(32, 28, 13) + "M32 21l6 3.5v7L32 35l-6-3.5v-7z", c, rule="evenodd")
picture = lambda c: rect(17, 16, 30, 24, rx=2, stroke_color=c, width=2.6) + filled("M20 37l7-9 5 6 4-4 8 7z", c) + dot(39, 22, 3, c)
note = lambda c: dot(24, 36, 5, c) + stroke("M28.5 36V17l12 4v4l-12-4", 3.6, c)
bars = lambda c: strokes("M19 25v6", "M24 20v16", "M29 16v24", "M34 22v12", "M39 18v20", "M44 25v6", width=3.2, color=c)
sine = _s("M16 28q4-13 8 0t8 0 8 0 8 0", w=3.4)
square_wave = _s("M16 35h5V21h7v14h7V21h7v14h5", w=3.2)
speaker = lambda c: filled("M17 23h6l8-7v24l-8-7h-6z", c) + strokes("M36 22q4 6 0 12", "M40 18q7 10 0 20", width=3, color=c)
screen_play = lambda c: filled("M17 17h30v22H17zM28.5 22v12l9.5-6z", c, rule="evenodd")
film = lambda c: filled("M19 14h26v28H19z" + "".join(f"M21 {y}h3v3h-3zM40 {y}h3v3h-3z" for y in (16.5, 23, 29.5, 36))
                        + "M28.5 22v12l9-6z", c, rule="evenodd")
circle_play = lambda c: filled(circle_d(32, 28, 13) + "M28.5 22v12l9.5-6z", c, rule="evenodd")
clapper = lambda c: filled("M17 24h30v16H17z", c) + filled("M17 17l29-4 1 6-29 4z", c)
zipper = lambda c: "".join(rect(28 if i % 2 else 32, 14 + 3.2 * i, 4, 2.4, fill=c) for i in range(7)) + filled("M28 36h8v8h-8zM30.5 38.5v3h3v-3z", c, rule="evenodd")
box_outline = _s("M19 22l13-6 13 6v14l-13 6-13-6z", "M19 22l13 6 13-6", "M32 28v14", w=2.8)
seven = _s("M21 18h21l-12 22", w=5)
stacked = lambda c: rect(18, 16, 28, 7, rx=1.5, fill=c) + rect(18, 25, 28, 7, rx=1.5, fill=c) + rect(18, 34, 28, 7, rx=1.5, fill=c) + stroke("M36 13v31", 2.4, c)
disc = lambda c: filled(circle_d(32, 28, 14) + circle_d(32, 28, 3.6), c, rule="evenodd")
cube = _s("M32 14l13 7v15l-13 7-13-7V21z", "M19 21l13 7 13-7", "M32 28v15", w=2.8)
layers = _s("M32 15l14 6.5-14 6.5-14-6.5z", "M18 28l14 6.5 14-6.5", "M18 34.5l14 6.5 14-6.5", w=2.8)
package = lambda c: filled("M18 23h28v18H18zM28 27v4h8v-4z", c, rule="evenodd") + filled("M20 16h24l3 5H17z", c)
swirl = _s("M37 33a8 8 0 1 1 2-9q2 6-4 8.5-4 1-5.5-2.5-1-3.5 2.5-4.5", w=3.6)
font_tt = _s("M17 18h16", "M25 18v21", "M39 23v13q0 3 3.5 3", "M35 27h8", w=3.6)
font_aa = lambda c: strokes("M17 39l7.5-21 7.5 21", "M20 32h9", width=3.6, color=c) + ring(41, 34, 4.5, 3.2, c) + stroke("M45.5 29v10", 3.2, c)
magnet = lambda c: stroke("M22 16v10a10 10 0 0 0 20 0V16", 6, c, cap="butt")
calendar = lambda c: (rect(18, 17, 28, 23, rx=2, stroke_color=c, width=2.6) + rect(18, 17, 28, 6, fill=c)
                      + "".join(dot(x, y, 1.9, c) for x in (24, 32, 40) for y in (29, 35)))
vcard = lambda c: (rect(16, 18, 32, 22, rx=2.5, stroke_color=c, width=2.6) + dot(25, 26, 3.4, c)
                   + filled("M19.5 36q0-5 5.5-5t5.5 5z", c) + strokes("M35 25h8", "M35 31h6", width=2.6, color=c))
rings4 = lambda c: "".join(ring(x, y, 6.5, 2.8, c) for x, y in ((26.5, 22.5), (37.5, 22.5), (26.5, 33.5), (37.5, 33.5)))
layout = lambda c: (rect(18, 15, 28, 26, rx=1.5, stroke_color=c, width=2.6) + rect(21.5, 18.5, 9, 9, fill=c)
                    + strokes("M34 20h8.5", "M34 25h8.5", "M21.5 32h21", "M21.5 37h14", width=2.4, color=c))
notebook = lambda c: (filled("M22 15h22v26H22zM27 20v4h13v-4z", c, rule="evenodd")
                      + "".join(stroke(f"M18 {y}h6", 2.4, c) for y in (19, 25, 31, 37)))
# A ledger page and a coin: bookkeeping, distinct from the database cylinder.
coins = lambda c: (rect(17, 15, 20, 26, rx=1.5, stroke_color=c, width=2.4) + stroke("M21 22h12M21 28h12M21 34h8", 2.2, c)
                   + filled(circle_d(40, 34, 8) + circle_d(40, 34, 4.5), c, rule="evenodd"))
flowchart = lambda c: (rect(26, 14, 12, 8, rx=1.5, fill=c) + rect(17, 33, 12, 8, rx=4, fill=c)
                       + filled("M41 31l6 6-6 6-6-6z", c) + strokes("M32 22v5.5", "M23 27.5h18", "M23 27.5v5.5", "M41 27.5v3.5", width=2.4, color=c))
gantt = lambda c: rect(18, 16, 14, 5, rx=1.5, fill=c) + rect(24, 24, 16, 5, rx=1.5, fill=c) + rect(33, 32, 13, 5, rx=1.5, fill=c) + stroke("M17 41h30", 2, c)
form = lambda c: (strokes("M18 18h9", "M18 27h9", "M18 36h9", width=2.6, color=c)
                  + "".join(rect(30, y - 3.5, 16, 7, rx=1.5, stroke_color=c, width=2.2) for y in (18, 27, 36)))
# Lid and body are separate shapes: the rim is a real gap, not a painted line.
cylinder = lambda c: (filled("M19 19q0-5 13-5t13 5-13 5-13-5z", c)
                      + filled("M19 24.5q0 5 13 5t13-5V37q0 5-13 5t-13-5z", c))
key = lambda c: filled(circle_d(25, 25, 8) + circle_d(25, 25, 3), c, rule="evenodd") + stroke("M31 30l13 11M38.5 36.5l4-4M42 39.5l3-3", 3.4, c)
blend = lambda c: filled(circle_d(34, 30, 10) + circle_d(34, 30, 4.5), c, rule="evenodd") + strokes("M17 26h10", "M20 20l9 5", width=3.4, color=c)
launcher = lambda c: filled("M20 16h24q3 0 3 3v19q0 3-3 3H20q-3 0-3-3V19q0-3 3-3zM28 22v12l9-6z", c, rule="evenodd")
info = lambda c: dot(32, 19, 3, c) + stroke("M28 27h4v12M28 39h8", 3.6, c)
generic = text_lines
PY_TOP_D = "M20 25q0-8 8-8h7q4 0 4 4v5H28v3h-8z"
PY_BOTTOM_D = "M44 31q0 8-8 8h-7q-4 0-4-4v-5h11v-3h8z"
python = lambda c: filled(PY_TOP_D, c) + filled(PY_BOTTOM_D, c)
