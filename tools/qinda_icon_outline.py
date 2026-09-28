#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Derive a line-drawn `-symbolic` cut from multi-color artwork.

AGENT-CONTRACT: used for first-party marks whose color art layers several
fills. Every painted shape becomes an INK outline with a transparent inside,
so the runtime's source-in recolor (qinda_icon_shapes.py) can never turn a
painted-over "hole" into solid ink. Tiny dots stay filled so eyes and LEDs
survive at 16 px. Background plates (a shape tagged `data-plate="1"`) are
dropped: a symbolic glyph is the mark, never its tile.
"""
import xml.etree.ElementTree as ET

from qinda_icon_shapes import INK

_NS = "http://www.w3.org/2000/svg"
_SHAPES = {"path", "rect", "circle", "ellipse", "polygon", "polyline", "line"}


def outline_symbolic(color_art: str, width: float = 3.4) -> str:
    root = ET.fromstring(f'<svg xmlns="{_NS}">{color_art}</svg>')
    for parent in list(root.iter()):
        for child in list(parent):
            if child.get("data-plate") == "1" or child.tag.endswith("defs"):
                parent.remove(child)
    for element in root.iter():
        tag = element.tag.rsplit("}", 1)[-1]
        if tag not in _SHAPES:
            continue
        tiny = tag == "circle" and float(element.get("r", "9")) < 3.5
        element.set("fill", INK if tiny else "none")
        element.set("stroke", INK)
        if element.get("stroke-width") is None or float(element.get("stroke-width", "0")) > width:
            element.set("stroke-width", str(width))
        element.set("stroke-linejoin", "round")
        element.set("stroke-linecap", "round")
        element.attrib.pop("fill-rule", None)
        element.attrib.pop("opacity", None)
    return "".join(ET.tostring(child, encoding="unicode") for child in root).replace(f' xmlns="{_NS}"', "")


def plate(x: float, y: float, w: float, h: float, rx: float, fill: str) -> str:
    """A background plate: color art only, dropped from the symbolic cut."""
    return f'<rect data-plate="1" x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}"/>'
