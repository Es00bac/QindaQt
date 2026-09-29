#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Distinct marks for every Qinda application, keyed by its desktop icon name.

AGENT-NOTE: canonical names are the `Icon=` values the apps' desktop entries
declare (found in container-wm/src/apps, QindaOffice/data, QindaStudio/data,
QindaMPV/data, QindaQt_Apps, QindaLlamaWatch/data, Games/*/integration and
gabbee/share on qinda), so installing a family re-skins the real launchers.
Each app has its own silhouette AND hue; Sloom Studio keeps its four
interlocking rings on a dark plate (the product's own identity), and its four
workspace launchers share that plate grammar in four hues.
QindaQt Settings keeps `preferences-system` (the Settings gear is its mark).

AGENT-CONTRACT: symbolic cuts are derived by qinda_icon_outline, so the color
art may layer fills freely; background plates must use `plate()` so the
symbolic cut and the Native family's uniform tile can recognise them.
"""
from qinda_icon_outline import outline_symbolic, plate
from qinda_icon_shapes import Icon, circle_d, dot, filled, rect, ring, stroke, strokes

GROUP = "apps"
PLUM = "#6c4f8a"
APRICOT = "#e8ae84"
CREAM = "#fbf6ee"
NIGHT = "#1d1a2b"


def _mark(art: str, width: float = 3.4) -> Icon:
    return Icon(GROUP, outline_symbolic(art, width), art)


def _studio_rings() -> str:
    return "".join(ring(x, y, 9.5, 5, c) for (x, y), c in zip(
        ((25, 25), (39, 25), (25, 39), (39, 39)), ("#ff4d6d", "#ff9f1c", "#3a86ff", "#2ec4d6")))


def _workspace(tile: str, ink: str, glyph: str) -> Icon:
    return _mark(plate(5, 5, 54, 54, 14, tile) + glyph.replace("INK", ink))


CANON = {
    "org.qindaqt.QindaStudio": _mark(plate(5, 5, 54, 54, 14, NIGHT) + _studio_rings()),
    "org.qindaqt.QindaStudio.Flow": _workspace("#132a4a", "#4cc3ff",
        rect(35, 13, 11, 9, rx=2, fill="INK") + rect(35, 27.5, 11, 9, rx=2, fill="INK") + rect(35, 42, 11, 9, rx=2, fill="INK")
        + rect(18, 27.5, 11, 9, rx=2, fill="INK") + strokes("M29 32h3M32 17.5v29M32 17.5h3M32 32h3M32 46.5h3", width=2.2, color="INK")
        + ring(12, 32, 3, 2.2, "#58e08a")),
    "org.qindaqt.QindaStudio.Image": _workspace("#2a1f45", "#c59bff",
        rect(15, 14, 28, 22, rx=3, stroke_color="INK", width=2.4) + rect(20, 20, 30, 24, rx=3, fill="INK")
        + filled("M23 41l7-9 5 6 4-4 8 7z", "#2a1f45") + dot(42, 26, 3, "#2a1f45") + stroke("M41 55l10-12", 4, "#ff8fc0")),
    "org.qindaqt.QindaStudio.Paper": _workspace("#123b3a", "#5fe0c8",
        filled("M18 11h20l8 8v34H18z", "INK") + rect(22, 22, 9, 10, fill="#123b3a")
        + strokes("M34 24h8", "M34 29h8", "M22 37h20", "M22 42h14", width=2.2, color="#123b3a") + ring(12, 32, 3, 2.2, "#ffd166")),
    "org.qindaqt.QindaStudio.Video": _workspace("#3a1b2a", "#ff8a7a",
        rect(9, 26, 46, 14, rx=2, fill="INK") + "".join(rect(x, 29, 7, 8, rx=1, fill="#3a1b2a") for x in (13, 23, 33, 43))
        + stroke("M32 15v38", 2.2, "#ffd166") + filled("M28 12h8l-4 5z", "#ffd166")),
    "org.qindaqt.QindaOffice": _mark(
        '<rect x="8" y="14" width="28" height="38" rx="4" fill="#f07a2a" transform="rotate(-14 22 33)"/>'
        '<rect x="28" y="14" width="28" height="38" rx="4" fill="#1f9d57" transform="rotate(14 42 33)"/>'
        + rect(18, 9, 28, 40, rx=4, fill="#3b6fe0") + strokes("M24 20h16", "M24 27h16", "M24 34h11", width=3, color=CREAM)),
    "org.qindaqt.QindaWrite": _mark(
        filled("M12 6h27l11 11v41H12z", "#3b6fe0") + filled("M39 6v11h11z", "#9db8f5")
        + strokes("M19 22h16", "M19 29h22", "M19 36h13", width=3, color=CREAM)
        + filled("M35 52l13-19 7 5-13 19-9 3z", "#f2b53a") + stroke("M40 54l7-10", 1.6, "#8a5a10")),
    "org.qindaqt.QindaCalc": _mark(
        rect(8, 8, 48, 48, rx=10, fill="#1f9d57") + rect(14, 14, 16, 11, rx=2, fill=CREAM) + rect(34, 14, 16, 11, rx=2, fill=CREAM)
        + rect(14, 29, 16, 11, rx=2, fill=CREAM) + stroke("M36 32h13M36 32l6 7-6 7h13", 3.6, "#f2c230")),
    "org.qindaqt.QindaShow": _mark(
        strokes("M20 48l-6 12", "M44 48l6 12", "M32 48v12", width=4, color="#5a3a24")
        + rect(6, 8, 52, 38, rx=5, fill="#f07a2a") + rect(11, 13, 42, 28, rx=2, fill=CREAM)
        + filled("M27 19v16l13-8z", "#f07a2a")),
    "org.qindaqt.QindaBase": _mark(
        filled("M10 15q0-9 22-9t22 9v34q0 9-22 9t-22-9z", "#7a5bd0") + filled("M10 15q0 9 22 9t22-9", "#a58ceb")
        + strokes("M18 33h28", "M18 41h28", "M27 29v18", width=2.6, color=CREAM)),
    "org.qindaqt.QindaPlan": _mark(
        rect(7, 9, 50, 46, rx=9, fill="#159a94") + rect(13, 17, 20, 7, rx=2.5, fill=CREAM) + rect(22, 28, 22, 7, rx=2.5, fill=CREAM)
        + rect(30, 39, 20, 7, rx=2.5, fill=CREAM) + stroke("M12 43l4 4 7-8", 3.4, "#ffd166")),
    "org.qindaqt.QindaDiagram": _mark(
        strokes("M32 21v9", "M17 30h30", "M17 30v8", "M47 30v8", width=3.2, color="#6b4a1a")
        + rect(20, 6, 24, 15, rx=4, fill="#e8a03a") + dot(17, 46, 9, "#3a9ad9")
        + filled("M47 36l10 10-10 10-10-10z", "#d9534f")),
    "org.qindaqt.QindaMail": _mark(
        rect(6, 14, 52, 38, rx=5, fill="#e8584a") + filled("M6 19l26 19 26-19v-3q0-2-2-2H8q-2 0-2 2z", "#f58a7a")
        + stroke("M8 17l24 18 24-18", 2.6, CREAM) + dot(47, 44, 5, "#ffd166")),
    "org.qindaqt.QindaNote": _mark(
        rect(12, 8, 40, 50, rx=5, fill="#f5c542") + strokes("M19 22h24", "M19 30h24", "M19 38h16", width=2.6, color="#8a6a10")
        + "".join(stroke(f"M{x} 5v8", 3, "#4a4458") for x in (20, 28, 36, 44))
        + filled("M40 55l14-18 5 4-14 18-7 2z", "#e8584a")),
    "org.qindaqt.QindaBooks": _mark(
        filled("M8 12q12-5 24 2q12-7 24-2v40q-12-5-24 2q-12-7-24-2z", "#2f8f5a") + stroke("M32 14v40", 2, "#1f6b41")
        + strokes("M14 22l12 2", "M14 29l12 2", "M38 24l12-2", "M38 31l12-2", width=2.4, color=CREAM)
        + dot(46, 46, 9, "#f2c230") + stroke("M46 41v10", 2.4, "#8a6a10")),
    "org.qindaqt.QindaDeck": _mark(
        rect(7, 12, 50, 44, rx=7, fill="#2b2d3a")
        + "".join(rect(12 + 14.5 * (i % 3), 17 + 12.5 * (i // 3), 11, 9.5, rx=2.5, fill=c)
                  for i, c in enumerate(("#ff5d73", "#ffb547", "#5ee07a", "#4cc3ff", "#a07cff", "#ff8fc0", "#f2f2f2", "#5fe0c8", "#ffd166")))
        + stroke("M26 8q6-5 12 0", 2.6, "#4cc3ff")),
    "qqmpv": _mark(
        rect(5, 10, 54, 36, rx=8, fill="#5b2e91") + filled("M26 18v20l17-10z", APRICOT)
        + rect(14, 50, 36, 8, rx=4, fill="#8a5cc9") + dot(32, 54, 2.2, CREAM)),
    "org.qindaqt.Player": _mark(
        dot(32, 32, 26, "#ff8a3d") + ring(32, 32, 20, 2.4, "#ffc08a") + filled("M26 20v24l19-12z", CREAM)),
    # An MMO gaming mouse: scroll wheel, a side grid of twelve buttons and an
    # RGB underglow (Venus Pro configures bindings, DPI and lighting).
    "com.github.es00bac.venusprolinux": _mark(
        stroke("M11 45q21 22 42 0", 3.4, "#ff4fa3") + stroke("M15 51q17 15 34 0", 3.4, "#4cc3ff")
        + filled("M32 3q18 0 18 22v12q0 15-18 15T14 37V25Q14 3 32 3z", "#2c2f3a") + stroke("M32 4v17", 2, "#555a6b")
        + rect(29, 10, 6, 10, rx=3, fill="#8a8fa3")
        + "".join(rect(16 + 5 * (i % 2), 27 + 5 * (i // 2), 3.6, 3.6, rx=1, fill="#e4e6ef") for i in range(6))),
    "qqterm": _mark(
        rect(5, 9, 54, 46, rx=9, fill="#15131c") + rect(5, 9, 54, 9, rx=4, fill=PLUM)
        + strokes("M14 28l6 5-6 5", "M22 28l6 5-6 5", width=3.4, color="#ffb347") + rect(32, 36, 10, 4, rx=1, fill="#ffb347")),
    "org.qindaqt.QindaLutris": _mark(
        filled("M14 18h36q8 0 10 12l2 16q1 8-7 7l-10-9H19L9 53q-8 1-7-7l2-16q2-12 10-12z", "#f2a33a")
        + stroke("M14 33h12M20 27v12", 4, PLUM) + dot(43, 29, 3.2, "#e8584a") + dot(49, 35, 3.2, "#159a94")),
    "qindafox": _mark(
        dot(32, 34, 26, PLUM) + filled("M10 12l14 12h16l14-12-3 22-19 22-19-22z", "#ff7a2f")
        + filled("M16 34l16 22 16-22-8 3-8 7-8-7z", CREAM) + dot(24, 32, 2.6, NIGHT) + dot(40, 32, 2.6, NIGHT)
        + filled("M29 46h6l-3 4z", NIGHT)),
    "org.qindaqt.FileManager": _mark(
        filled("M5 15q0-5 5-5h13.5l7.5 6h23q5 0 5 5v29H5z", "#c98a36") + filled("M4 26q0-4 4-4h48q4 0 4 4l-2.6 27Q57 57 52.5 57h-41Q7 57 6.6 53z", "#e9a445")
        + rect(21, 28, 22, 22, rx=6, fill=PLUM) + rect(26, 33, 9, 9, rx=3, fill=APRICOT)),
    "org.qindaqt.TextEditor": _mark(
        filled("M12 6h28l12 12v40H12z", PLUM) + filled("M40 6v12h12z", "#a68cc4")
        + strokes("M19 24h18", "M19 31h24", "M19 38h12", width=3, color=CREAM)
        + strokes("M42 38v16", "M38 38h8", "M38 54h8", width=2.8, color=APRICOT)),
    "org.qindaqt.SystemMonitor": _mark(
        dot(32, 34, 26, "#2b2438") + stroke("M14 44a20 20 0 0 1 8-24", 5, "#5ee07a", cap="butt")
        + stroke("M22 20a20 20 0 0 1 20 0", 5, "#ffd166", cap="butt") + stroke("M42 20a20 20 0 0 1 8 24", 5, "#ff5d73", cap="butt")
        + stroke("M32 38l9-12", 3.4, CREAM) + dot(32, 38, 4, APRICOT)),
    "org.qindaqt.Calendar": _mark(
        rect(8, 11, 48, 46, rx=6, fill=CREAM) + filled("M8 17q0-6 6-6h36q6 0 6 6v8H8z", PLUM)
        + strokes("M20 6v10", "M44 6v10", width=4, color="#3a3347")
        + "".join(rect(x, y, 7, 6, rx=1.5, fill="#c9bcd8") for x in (15, 28.5, 42) for y in (31, 43))
        + rect(28.5, 43, 7, 6, rx=1.5, fill="#e8584a")),
    "qindaqt-viewer": _mark(
        rect(6, 10, 52, 44, rx=6, fill=PLUM) + rect(11, 15, 42, 34, rx=2, fill="#7cc4f0")
        + filled("M11 49V38l11-9 9 8 8-6 14 11v7z", "#4dab4f") + dot(42, 24, 4.5, APRICOT)),
    # The Screenshot tool (ADR-0289): the places catalog's camera silhouette
    # inside region-selection corners, in the first-party plum and apricot.
    "org.qindaqt.Screenshot": _mark(
        strokes("M8 21V8h13", "M43 8h13v13", "M56 43v13H43", "M21 56H8V43", width=4, color=APRICOT)
        + rect(15, 22, 34, 25, rx=5, fill=PLUM) + filled("M25 22l3.5-6h7l3.5 6z", PLUM)
        + dot(32, 34.5, 8, CREAM) + dot(32, 34.5, 4.5, "#3a3347") + dot(43, 27, 2, APRICOT)),
    "qindaqt-voice": _mark(
        rect(24, 6, 16, 30, rx=8, fill=PLUM) + stroke("M17 28q0 15 15 15t15-15M32 43v11M24 56h16", 3.6, "#3a3347")
        + strokes("M10 24v10", "M5 20v18", "M54 24v10", "M59 20v18", width=3, color=APRICOT)),
    "gabbee": _mark(
        filled("M8 12h48q4 0 4 4v26q0 4-4 4H28l-12 10V46H8q-4 0-4-4V16q0-4 4-4z", "#1aa3a0")
        + strokes("M18 29v0", "M24 23v12", "M30 19v20", "M36 24v10", "M42 21v16", "M48 28v2", width=3.6, color=CREAM)),
    "org.qindaqt.LlamaWatch": _mark(
        dot(32, 32, 27, "#1d3557") + stroke("M9 44l9-6 7 4 8-10 7 5 7-9 8 5", 2.4, "#5ee07a")
        + filled("M26 12l3-7 3 8h4l3-8 3 7-1 8q5 4 5 12v8q0 6-8 6h-4q-8 0-8-6v-8q0-8 5-12z", "#f3ead8")
        + dot(29, 29, 2, NIGHT) + dot(39, 29, 2, NIGHT) + filled("M31 39h6l-3 3z", "#c77b5b")),
    "qinda-patrol": _mark(
        filled("M46 30l16-8v28l-16-8z", "#ffe08a") + rect(10, 16, 36, 30, rx=12, fill="#1aa3a0")
        + rect(15, 22, 26, 17, rx=8, fill="#123b3a") + dot(23, 30, 4, "#7ff5e4") + dot(34, 30, 4, "#7ff5e4")
        + stroke("M28 16V8", 2.6, "#3a3347") + dot(28, 7, 3.4, "#ff5d73") + rect(16, 46, 8, 8, rx=3, fill="#3a3347") + rect(32, 46, 8, 8, rx=3, fill="#3a3347")),
    "studio.qinda.CircuitReef": _mark(
        plate(5, 5, 54, 54, 14, "#0e3440") + strokes("M14 54V40l6-6V24", "M26 54V44l8-8V20", "M44 54V38l-5-5V26", width=2.6, color="#3fe0d0")
        + dot(20, 22, 3, "#3fe0d0") + dot(34, 18, 3, "#3fe0d0") + dot(39, 24, 3, "#3fe0d0")
        + filled("M30 30q10-8 18 0-8 8-18 0zM48 30l7-5v10z", "#ff5d9e")),
    "studio.qinda.PrismCircuit": _mark(
        stroke("M12 38q0-20 20-20h8q12 0 12 12t-12 12H26q-8 0-8 8t8 8h26", 6, "#2b2d3a")
        + stroke("M12 38q0-20 20-20h8q12 0 12 12t-12 12H26q-8 0-8 8t8 8h26", 1.6, "#ffd166")
        + filled("M32 5l11 19H21z", "#4cc3ff") + filled("M32 5l11 19h-11z", "#ff5d9e")),
    "studio.qinda.PrismBrawl": _mark(
        filled("M32 3l6 12 13-6-3 14 13 5-12 8 8 11-14-2-3 14-8-11-8 11-3-14-14 2 8-11-12-8 13-5-3-14 13 6z", "#ff5d73")
        + filled("M32 16l14 25H18z", "#ffd166") + filled("M32 16l14 25H32z", "#ff9f1c")),
    "studio.qinda.PrismKart": _mark(
        filled("M8 38l6-12h20l6-8h10l6 14v10H8z", "#e8584a") + rect(24, 20, 8, 7, rx=2, fill="#4cc3ff")
        + dot(18, 46, 8, "#2b2d3a") + dot(46, 46, 8, "#2b2d3a") + dot(18, 46, 3, "#c9ccd6") + dot(46, 46, 3, "#c9ccd6")
        + stroke("M50 18V6", 2.2, "#3a3347") + filled("M50 6l10 3.5-10 3.5z", "#5ee07a")),
    "org.qindaqt.QindaComputerUse": _mark(
        rect(5, 8, 46, 34, rx=5, fill="#3b6fe0") + rect(9, 15, 38, 23, rx=2, fill="#dbe7ff") + rect(9, 10, 38, 3, rx=1.5, fill="#9db8f5")
        + stroke("M28 42v8M18 52h20", 3.6, "#3a3347")
        + filled("M36 26l22 13-10 3-4 11z", CREAM) + stroke("M36 26l22 13-10 3-4 11z", 2.4, "#3a3347")),
}

ALIASES = {
    "sloom-studio": ("org.qindaqt.QindaStudio", None),
    "qinda-studio": ("org.qindaqt.QindaStudio", None),
    "signal-loom": ("org.qindaqt.QindaStudio", None),
    "org.qindaqt.QQMpv": ("qqmpv", None),
    "org.qindaqt.QindaMPV": ("qqmpv", None),
    "org.qindaqt.QindaVenusPro": ("com.github.es00bac.venusprolinux", None),
    "org.qindaqt.QQTerm": ("qqterm", None),
    "org.qindaqt.QindaFox": ("qindafox", None),
    "org.qindaqt.Viewer": ("qindaqt-viewer", None),
    "org.qindaqt.Voice": ("qindaqt-voice", None),
    "org.qindaqt.Patrol": ("qinda-patrol", None),
    "studio.qinda.MegaBrawl": ("studio.qinda.PrismBrawl", None),
    "qinda-deck": ("org.qindaqt.QindaDeck", None),
}
