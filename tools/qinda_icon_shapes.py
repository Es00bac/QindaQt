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
import math
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
def _num(value: float) -> str:
    return f"{value:.2f}".rstrip("0").rstrip(".")


def circle_d(cx: float, cy: float, r: float) -> str:
    """A full circle as a subpath, for `filled(..., rule="evenodd")` holes."""
    return (f"M{_num(cx + r)} {_num(cy)}A{_num(r)} {_num(r)} 0 1 0 {_num(cx - r)} {_num(cy)}"
            f"A{_num(r)} {_num(r)} 0 1 0 {_num(cx + r)} {_num(cy)}Z")


def gear_d(cx: float, cy: float, r_out: float, r_root: float, teeth: int,
           tooth_frac: float = 0.5, taper: float = 0.72) -> str:
    """Closed toothed-gear outline: tapered teeth joined by root-circle arcs.

    AGENT-NOTE: this replaced the retired four-spoke circle (ring plus four
    ticks) that doubled as the Settings icon and the system-menu logo; Jarrod
    rejected that motif, so no icon may draw it again (plan W18, 2026-09-24).
    """
    points = []
    step = 2 * math.pi / teeth
    half_root = step * tooth_frac / 2
    half_tip = half_root * taper
    for i in range(teeth):
        a = i * step - math.pi / 2
        for angle, r in ((a - half_root, r_root), (a - half_tip, r_out),
                         (a + half_tip, r_out), (a + half_root, r_root)):
            points.append((cx + r * math.cos(angle), cy + r * math.sin(angle)))
    parts = [f"M{_num(points[0][0])} {_num(points[0][1])}"]
    for i in range(teeth):
        _, tip_a, tip_b, root_b = points[4 * i:4 * i + 4]
        nxt = points[(4 * i + 4) % len(points)]
        parts.append(f"L{_num(tip_a[0])} {_num(tip_a[1])}L{_num(tip_b[0])} {_num(tip_b[1])}"
                     f"L{_num(root_b[0])} {_num(root_b[1])}"
                     f"A{_num(r_root)} {_num(r_root)} 0 0 1 {_num(nxt[0])} {_num(nxt[1])}")
    parts.append("Z")
    return "".join(parts)


GEAR_BADGE_CX, GEAR_BADGE_CY = 48, 16
GEAR_BADGE_D = gear_d(GEAR_BADGE_CX, GEAR_BADGE_CY, 9, 6.6, 6, tooth_frac=0.55)


def gear_badge(color: str = INK) -> str:
    """Small six-tooth gear used by every preferences-system-* glyph.

    The centre is a real evenodd hole, so the symbolic recolor keeps it clear.
    """
    return filled(GEAR_BADGE_D + circle_d(GEAR_BADGE_CX, GEAR_BADGE_CY, 2.6),
                  fill=color, rule="evenodd")
