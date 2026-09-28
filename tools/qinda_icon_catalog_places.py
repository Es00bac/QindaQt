#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Canonical place glyphs: special folders, colored folders, trash and hosts.

AGENT-NOTE: one folder grammar (darker back flap, a paper sheet, a lit front
body, one single-tone emblem) is shared by every folder so a sidebar reads as
a family; each special folder is told apart by BOTH its body color and its
emblem, never color alone (accessibility rule in docs/wiki/shell/icon-theme.md).
`user-trash` renders `TRASH_D`, a bin silhouette, never the folder outline --
conflating the two was a withdrawn candidate's defect.

AGENT-CONTRACT: the desktop-icon surface asks for user-home,
folder-documents, folder-download, folder-pictures, folder-videos,
folder-music, user-trash, user-trash-full, computer and network-workgroup;
tools/validate_qinda_icon_theme.py pins them. QindaThemes copies this module
verbatim (tools/icon_sources/) and re-tones the same true colors per family.
"""
import qinda_icon_place_marks as marks
from qinda_icon_color import luminance, shade
from qinda_icon_shapes import FOLDER_D, INK, TRASH_D, Icon, dot, filled, rect, stroke, strokes

GROUP = "places"

FOLDER_BACK_D = "M5 15q0-5 5-5h13.5q2 0 3.4 1.5L31 16h23q5 0 5 5v29H5z"
FOLDER_FRONT_D = "M4 26q0-4 4-4h48q4 0 4 4l-2.6 26.5Q57 57 52.5 57h-41Q7 57 6.6 52.5z"
PAPER = "#fbf8f1"


def emblem_tone(body: str) -> str:
    """Dark emblem on a light body, light emblem on a dark body."""
    return shade(body, -0.5) if luminance(body) > 0.3 else shade(body, 0.78)


def folder(body: str, mark=None) -> Icon:
    color = (filled(FOLDER_BACK_D, shade(body, -0.22))
             + rect(9, 17, 46, 22, rx=2.5, fill=PAPER)
             + filled(FOLDER_FRONT_D, body)
             + stroke("M8.5 25.5h47", 1.6, shade(body, 0.4))
             + (mark(emblem_tone(body)) if mark else ""))
    symbolic = stroke(FOLDER_D, width=5) + (mark(INK) if mark else "")
    return Icon(GROUP, symbolic, color)


# Special folders: (emblem, body color). Hues are spread round the wheel so
# neighbours in a sidebar differ in hue as well as in emblem.
SPECIAL = {
    "folder": (None, "#e9a445"),
    "user-home": (marks.home, "#e4613f"),
    "user-desktop": (marks.desktop, "#5c6fd6"),
    "folder-documents": (marks.documents, "#3d8bdb"),
    "folder-download": (marks.download, "#1fa39a"),
    "folder-pictures": (marks.pictures, "#4dab4f"),
    "folder-videos": (marks.videos, "#d23f57"),
    "folder-music": (marks.music, "#d24fa0"),
    "folder-books": (marks.books, "#a5693b"),
    "folder-templates": (marks.templates, "#a184d8"),
    "folder-publicshare": (marks.publicshare, "#8fb534"),
    "folder-projects": (marks.projects, "#e2b52c"),
    "folder-games": (marks.games, "#8b55d1"),
    "folder-development": (marks.development, "#4a5568"),
    "folder-remote": (marks.remote, "#4f8fa8"),
    "folder-cloud": (marks.cloud, "#5ab1f0"),
    "folder-applications": (marks.applications, "#7c6ce0"),
    "folder-work": (marks.work, "#2f5c8f"),
    "folder-favorites": (marks.favorites, "#ec6a8c"),
    "folder-locked": (marks.locked, "#6e6a8a"),
    "folder-backup": (marks.backup, "#3e7c6b"),
    "folder-camera": (marks.camera, "#9c6b9e"),
    "folder-wine": (marks.wine, "#8e2f45"),
}
COLORED = {
    "blue": "#3d8bdb", "green": "#4dab4f", "red": "#de4b4b", "orange": "#ee8a35",
    "yellow": "#edc435", "purple": "#8b55d1", "pink": "#ec6a9e", "grey": "#96a0ac",
    "black": "#3b3f4a", "brown": "#a5693b", "teal": "#1fa39a", "cyan": "#35b6d8",
}

CANON = {name: folder(body, mark) for name, (mark, body) in SPECIAL.items()}
CANON.update({f"folder-{name}": folder(body) for name, body in COLORED.items()})

# Trash: one bin, the full variant adds crumpled paper and a lifted lid.
_BIN = "#8fa1b3"
_trash_ribs = strokes("M25 27v24", "M32 27v24", "M39 27v24", width=3, color=shade(_BIN, -0.35))
CANON["user-trash"] = Icon(
    GROUP,
    stroke(TRASH_D) + stroke("M12 20h40") + stroke("M26 20v-4h12v4") + strokes("M24 26v20", "M32 26v20", "M40 26v20", width=3),
    filled(TRASH_D, _BIN) + filled("M16 20h32l-.4 4H16.4z", shade(_BIN, -0.2)) + _trash_ribs
    + rect(10, 15, 44, 6, rx=3, fill=shade(_BIN, -0.3)) + rect(26, 10, 12, 6, rx=2, fill=shade(_BIN, -0.3)),
)
CANON["user-trash-full"] = Icon(
    GROUP,
    stroke(TRASH_D) + stroke("M12 20h40") + filled("M18 17a6 6 0 0 1 12 0zM33 16a7 7 0 0 1 14 0z")
    + strokes("M24 26v20", "M32 26v20", "M40 26v20", width=3),
    # Open bin overflowing: a sheet and crumpled balls rise well above the rim
    # so "full" reads at 24 px, not only at 64.
    filled("M24 22l2-20 17 2.5-2.5 19.5z", PAPER) + stroke("M28 8l11 1.5M27.6 12.5l10.5 1.4M27.2 17l8 1", 1.6, "#9fb0c0")
    + dot(20, 19, 8.5, "#f1eadc") + stroke("M14 17l5 3.5 4-5.5M18 24l3.5-4", 1.6, "#bcae98")
    + dot(44, 18, 9, "#f6f1e6") + stroke("M38 15.5l5.5 3.5 5-5M42 25l3-5", 1.6, "#bcae98")
    + filled(TRASH_D, _BIN) + _trash_ribs + rect(12, 18, 40, 5, rx=2.5, fill=shade(_BIN, -0.3)),
)
CANON["computer"] = Icon(
    GROUP,
    rect(7, 9, 50, 34, rx=4, width=5) + stroke("M26 55h12M32 43v12", width=5),
    rect(6, 8, 52, 36, rx=4, fill="#39404f") + rect(10, 12, 44, 27, rx=1.5, fill="#4d9de6")
    + filled("M10 39V30l11-8 9 7 8-5 16 9v6z", "#5bc07d") + dot(44, 20, 3.5, "#ffd66b")
    + filled("M27 44h10l2 9H25z", "#8a93a3") + rect(18, 52, 28, 5, rx=2.5, fill="#6c7585"),
)
CANON["network-workgroup"] = Icon(
    GROUP,
    dot(32, 16, 10) + rect(5, 34, 20, 14, rx=2, width=4) + rect(39, 34, 20, 14, rx=2, width=4)
    + strokes("M32 26v8", "M15 34v-4h34v4", "M11 53h8", "M45 53h8", width=4),
    strokes("M32 25v6", "M15 34v-3h34v3", width=3, color="#6b7485")
    + dot(32, 16, 11, "#3f8fd9") + stroke("M21 16h22M32 5q-6 11 0 22M32 5q6 11 0 22", 2, "#bfe0ff")
    + rect(4, 33, 22, 16, rx=2, fill="#39404f") + rect(7, 36, 16, 10, fill="#4dab4f")
    + rect(38, 33, 22, 16, rx=2, fill="#39404f") + rect(41, 36, 16, 10, fill="#e9a445")
    + rect(10, 51, 10, 4, rx=1.5, fill="#6c7585") + rect(44, 51, 10, 4, rx=1.5, fill="#6c7585"),
)
CANON["network-server"] = Icon(
    GROUP, rect(16, 10, 32, 44, rx=4) + strokes("M16 22h32", "M16 34h32", "M16 46h32", width=3) + dot(44, 16, 2),
    rect(14, 7, 36, 50, rx=4, fill="#39404f")
    + "".join(rect(18, y, 28, 9, rx=1.5, fill="#566074") + dot(41, y + 4.5, 1.8, "#5bd08a") for y in (12, 24, 36))
    + stroke("M21 52h22", 2, "#6c7585"),
)

ALIASES = {
    "folder-ebooks": ("folder-books", None),
    "folder-downloads": ("folder-download", None),
    "folder-public": ("folder-publicshare", None),
    "folder-video": ("folder-videos", None),
    "folder-images": ("folder-pictures", None),
    "folder-image": ("folder-pictures", None),
    "folder-photo": ("folder-pictures", None),
    "folder-sound": ("folder-music", None),
    "folder-text": ("folder-documents", None),
    "folder-code": ("folder-development", None),
    "folder-script": ("folder-development", None),
    "folder-network": ("folder-remote", None),
    "folder-important": ("folder-favorites", None),
    "folder-bookmark": ("folder-favorites", None),
    "folder-template": ("folder-templates", None),
    "folder-magenta": ("folder-pink", None),
    "folder-violet": ("folder-purple", None),
    "folder-gray": ("folder-grey", None),
    "user-bookmarks": ("folder-favorites", None),
    "trashcan_empty": ("user-trash", None),
    "trashcan_full": ("user-trash-full", None),
}
