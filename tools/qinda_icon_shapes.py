#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Shared drawing primitives for the QindaQt icon catalog.

AGENT-CONTRACT: every canonical icon in tools/qinda_icon_catalog_*.py builds
its symbolic and color artwork from these primitives so both renderings stay
structurally related. Symbolic fragments must only use `stroke()`/`ring()`
(real transparent gaps) or `filled(..., rule="evenodd")` (a true transparent
hole cut by the second subpath) -- never a same-alpha shape painted on top to
fake a hole, because the runtime's symbolic recolor is `source-in`: it
replaces every opaque pixel's RGB and keeps its alpha, so a faked hole becomes
solid icon color instead of vanishing. See docs/wiki/shell/icon-theme.md.
"""
from dataclasses import dataclass

# AGENT-CONTRACT: Pearl / Smoked Plum material identity, documented in
# docs/wiki/shell/visual-identity.md. Legacy constant names remain generator
# compatibility names: BLUE is the plum body, AMBER the apricot highlight.
# Status hues retain their semantic meaning; symbolic geometry is color-free.
INK = "#211D27"
PORCELAIN = "#FAF5F0"
AMBER = "#EAB391"
JADE = "#70BFA5"
BLUE = "#9C86AA"
VIOLET = "#B9A2CC"
APRICOT = "#E8AE84"

VIEWBOX_HEADER = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">'
VIEWBOX_FOOTER = "</svg>\n"


@dataclass(frozen=True)
class Icon:
    """One canonical glyph: its theme sub-directory and both renderings."""

    group: str
    symbolic: str
    color: str


def stroke(d: str, width: float = 6, color: str = INK, cap: str = "round", join: str = "round") -> str:
    return f'<path d="{d}" fill="none" stroke="{color}" stroke-width="{width}" stroke-linecap="{cap}" stroke-linejoin="{join}"/>'


def strokes(*ds: str, width: float = 6, color: str = INK, cap: str = "round", join: str = "round") -> str:
    return "".join(stroke(d, width=width, color=color, cap=cap, join=join) for d in ds)


def filled(d: str, fill: str = INK, rule: str | None = None) -> str:
    rule_attr = f' fill-rule="{rule}"' if rule else ""
    return f'<path d="{d}" fill="{fill}"{rule_attr}/>'


def ring(cx: float, cy: float, r: float, width: float = 6, color: str = INK) -> str:
    return f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="{color}" stroke-width="{width}"/>'


def dot(cx: float, cy: float, r: float, color: str = INK) -> str:
    return f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{color}"/>'


def rect(x: float, y: float, w: float, h: float, rx: float = 0, fill: str | None = None,
         stroke_color: str | None = None, width: float = 6) -> str:
    # An outline rectangle is the common case throughout the catalog: no fill
    # and no explicit stroke means "draw it as an ink outline", matching
    # ring()'s always-stroked default. Without this, a bare rect() call is
    # fill="none" with no stroke and paints nothing at all.
    if fill is None and stroke_color is None:
        stroke_color = INK
    fill_attr = f'fill="{fill}"' if fill else 'fill="none"'
    stroke_attr = f' stroke="{stroke_color}" stroke-width="{width}"' if stroke_color else ""
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" {fill_attr}{stroke_attr}/>'


def render(icon: Icon, symbolic: bool) -> str:
    body = icon.symbolic if symbolic else icon.color
    return VIEWBOX_HEADER + body + VIEWBOX_FOOTER


# Compound paths shared verbatim by more than one canonical icon because the
# icons name the exact same object (a bell is a bell whether it announces a
# notification or a subsystem's notification settings).
BELL_D = "M32 12c-7 0-11 6-11 14v8l-5 8h32l-5-8v-8c0-8-4-14-11-14z"
BELL_CLAPPER_D = "M27 44a5 5 0 0 0 10 0"
BLUETOOTH_D = "M24 16 40 28 32 34 40 40 24 52V16z"
FOLDER_D = ("M7 23c0-5 4-9 9-9h12l6 7h14c5 0 9 4 9 9v17c0 6-4 10-10 10H17"
            "c-6 0-10-4-10-10V23z")
TRASH_D = "M16 20h32l-3 32c-.4 4-3 6-7 6H26c-4 0-6.6-2-7-6z"
DOC_D = "M16 6h22l10 10v42H16z"
DOC_FOLD_D = "M38 6v10h10"
MOON_D = "M40 14a20 20 0 1 0 0 36 16 16 0 0 1 0-36z"
GEAR_BADGE_CX, GEAR_BADGE_CY, GEAR_BADGE_R = 48, 16, 5


def gear_badge(color: str = INK) -> str:
    """Small settings-for-subsystem tick used by every preferences-system-* glyph."""
    return ring(GEAR_BADGE_CX, GEAR_BADGE_CY, GEAR_BADGE_R, width=4, color=color) + stroke(
        f"M{GEAR_BADGE_CX} {GEAR_BADGE_CY - 8}v3M{GEAR_BADGE_CX} {GEAR_BADGE_CY + 8}v-3"
        f"M{GEAR_BADGE_CX - 8} {GEAR_BADGE_CY}h3M{GEAR_BADGE_CX + 8} {GEAR_BADGE_CY}h-3",
        width=3, color=color,
    )
