#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Color arithmetic shared by the icon catalog and every icon family.

AGENT-CONTRACT: pure functions on `#rrggbb` strings with no I/O. The place,
file-type and first-party catalogs use `shade()` to derive a folder's back
flap and emblem from one declared body color; QindaThemes' family renderers
use `to_hsl()`/`from_hsl()` to re-tone that same true color into each
family's palette. Keep the output normalised (lower-case `#rrggbb`) so
generated files stay byte-stable across runs.
"""
import colorsys


def _rgb(color: str) -> tuple[float, float, float]:
    value = color.lstrip("#")
    if len(value) == 3:
        value = "".join(c * 2 for c in value)
    if len(value) != 6:
        raise ValueError(f"not a #rrggbb color: {color!r}")
    return tuple(int(value[i:i + 2], 16) / 255 for i in (0, 2, 4))  # type: ignore[return-value]


def _hex(r: float, g: float, b: float) -> str:
    return "#" + "".join(f"{max(0, min(255, round(c * 255))):02x}" for c in (r, g, b))


def is_color(value: str | None) -> bool:
    if not value or not value.startswith("#"):
        return False
    try:
        _rgb(value)
    except ValueError:
        return False
    return True


def to_hsl(color: str) -> tuple[float, float, float]:
    """Hue in [0,1), saturation and lightness in [0,1]."""
    h, l, s = colorsys.rgb_to_hls(*_rgb(color))
    return h, s, l


def from_hsl(h: float, s: float, l: float) -> str:
    return _hex(*colorsys.hls_to_rgb(h % 1.0, max(0.0, min(1.0, l)), max(0.0, min(1.0, s))))


def mix(a: str, b: str, amount: float) -> str:
    """Linear blend: 0 returns `a`, 1 returns `b`."""
    ra, rb = _rgb(a), _rgb(b)
    return _hex(*(x + (y - x) * amount for x, y in zip(ra, rb)))


def shade(color: str, amount: float) -> str:
    """Positive `amount` mixes toward white, negative toward near-black ink."""
    return mix(color, "#ffffff", amount) if amount >= 0 else mix(color, "#1b1822", -amount)


def luminance(color: str) -> float:
    """WCAG relative luminance, used to pick a readable emblem tone."""
    def channel(c: float) -> float:
        return c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = (channel(c) for c in _rgb(color))
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def rgb255(color: str) -> tuple[int, int, int]:
    return tuple(round(c * 255) for c in _rgb(color))  # type: ignore[return-value]
