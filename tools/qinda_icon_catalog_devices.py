#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Canonical device glyphs: audio, power, input, storage, and network radios.

AGENT-NOTE: wifi-signal bar counts and battery fill widths are read from each
name's declared level, never from a catalog index, so adding or reordering an
entry cannot silently reshuffle another icon's geometry.
"""
from qinda_icon_shapes import (
    APRICOT, BLUE, BLUETOOTH_D, INK, JADE, PORCELAIN, VIOLET, Icon, dot, filled, ring, rect, stroke, strokes,
)

GROUP = "devices"
SPEAKER_D = "M12 26h10l14-12v36l-14-12H12z"
BATTERY_BODY = rect(10, 21, 44, 22, rx=5) + stroke("M54 28v8", width=5)
BATTERY_INNER_X, BATTERY_INNER_Y, BATTERY_INNER_H = 14, 25, 14
WIFI_ARCS = [
    "M27 42a7 7 0 0 1 10 0",
    "M22 36a14 14 0 0 1 20 0",
    "M17 30a21 21 0 0 1 30 0",
    "M12 24a28 28 0 0 1 40 0",
]


def _battery(level_percent: int) -> Icon:
    width = 0 if level_percent == 0 else round(36 * level_percent / 100)
    fill_symbolic = rect(BATTERY_INNER_X, BATTERY_INNER_Y, width, BATTERY_INNER_H, fill=INK) if width else ""
    fill_color = rect(BATTERY_INNER_X, BATTERY_INNER_Y, width, BATTERY_INNER_H, fill=JADE) if width else ""
    return Icon(GROUP, BATTERY_BODY + fill_symbolic, rect(10, 21, 44, 22, rx=5, stroke_color=INK, width=5) + fill_color + stroke("M54 28v8", width=5))


def _wifi_level(bars: int) -> str:
    return dot(32, 48, 3) + "".join(stroke(a, width=4) for a in WIFI_ARCS[:bars])


CANON = {
    "audio-card": Icon(
        GROUP, rect(10, 16, 44, 32, rx=4) + ring(20, 42, 3) + ring(32, 42, 3) + ring(44, 42, 3),
        rect(10, 16, 44, 32, rx=4, fill=VIOLET) + ring(20, 42, 3, color=PORCELAIN, width=3)
        + ring(32, 42, 3, color=PORCELAIN, width=3) + ring(44, 42, 3, color=PORCELAIN, width=3),
    ),
    "audio-headphones": Icon(
        GROUP, stroke("M14 34a18 18 0 0 1 36 0") + rect(10, 34, 10, 16, rx=4) + rect(44, 34, 10, 16, rx=4),
        stroke("M14 34a18 18 0 0 1 36 0", color=VIOLET, width=6) + rect(10, 34, 10, 16, rx=4, fill=VIOLET) + rect(44, 34, 10, 16, rx=4, fill=VIOLET),
    ),
    "audio-headset": Icon(
        GROUP, stroke("M14 34a18 18 0 0 1 36 0") + rect(10, 34, 10, 16, rx=4) + rect(44, 34, 10, 16, rx=4)
        + stroke("M44 46 54 54", width=4) + dot(54, 54, 2),
        stroke("M14 34a18 18 0 0 1 36 0", color=VIOLET, width=6) + rect(10, 34, 10, 16, rx=4, fill=VIOLET) + rect(44, 34, 10, 16, rx=4, fill=VIOLET)
        + stroke("M44 46 54 54", color=INK, width=4) + dot(54, 54, 2, INK),
    ),
    "audio-volume-low": Icon(
        GROUP, filled(SPEAKER_D) + stroke("M40 28q6 4 0 8", width=4),
        filled(SPEAKER_D, fill=VIOLET) + stroke("M40 28q6 4 0 8", color=VIOLET, width=4),
    ),
    "audio-volume-medium": Icon(
        GROUP, filled(SPEAKER_D) + stroke("M40 25q10 7 0 14", width=4),
        filled(SPEAKER_D, fill=VIOLET) + stroke("M40 25q10 7 0 14", color=VIOLET, width=4),
    ),
    "audio-volume-high": Icon(
        GROUP, filled(SPEAKER_D) + stroke("M40 25q10 7 0 14", width=4) + stroke("M45 19q17 13 0 26", width=4),
        filled(SPEAKER_D, fill=VIOLET) + stroke("M40 25q10 7 0 14", color=VIOLET, width=4) + stroke("M45 19q17 13 0 26", color=VIOLET, width=4),
    ),
    "audio-volume-muted": Icon(
        GROUP, filled(SPEAKER_D) + strokes("M42 26 54 38", "M54 26 42 38", width=4),
        filled(SPEAKER_D, fill=INK) + strokes("M42 26 54 38", "M54 26 42 38", color=APRICOT, width=4),
    ),
    "battery-000": _battery(0),
    "battery-020": _battery(20),
    "battery-040": _battery(40),
    "battery-060": _battery(60),
    "battery-080": _battery(80),
    "battery-100": _battery(100),
    "battery-missing": Icon(
        GROUP,
        f'<rect x="10" y="21" width="44" height="22" rx="5" fill="none" stroke="{INK}" stroke-width="5" stroke-dasharray="6 5"/>'
        + stroke("M54 28v8", width=5),
        f'<rect x="10" y="21" width="44" height="22" rx="5" fill="none" stroke="{INK}" stroke-width="5" stroke-dasharray="6 5"/>'
        + stroke("M54 28v8", width=5) + dot(32, 32, 2, APRICOT),
    ),
    "battery-caution": Icon(
        GROUP,
        BATTERY_BODY + rect(BATTERY_INNER_X, BATTERY_INNER_Y, 6, BATTERY_INNER_H, fill=INK) + stroke("M50 8 58 20H42z", width=3) + dot(50, 17, 1.4),
        rect(10, 21, 44, 22, rx=5, stroke_color=INK, width=5) + rect(BATTERY_INNER_X, BATTERY_INNER_Y, 6, BATTERY_INNER_H, fill=APRICOT)
        + stroke("M54 28v8", width=5) + filled("M50 8 58 20H42z", fill=APRICOT) + dot(50, 17, 1.4, INK),
    ),
    "battery-charging": Icon(
        GROUP, BATTERY_BODY + filled("M34 24 26 36h8l-4 12 14-16h-8z"),
        rect(10, 21, 44, 22, rx=5, stroke_color=INK, width=5) + stroke("M54 28v8", width=5) + filled("M34 24 26 36h8l-4 12 14-16h-8z", fill=JADE),
    ),
    "ac-adapter": Icon(
        GROUP, rect(24, 10, 16, 20, rx=3) + strokes("M28 10v-4", "M36 10v-4", width=4) + stroke("M32 30v20", width=5),
        rect(24, 10, 16, 20, rx=3, fill=VIOLET) + strokes("M28 10v-4", "M36 10v-4", color=INK, width=4) + stroke("M32 30v20", width=5),
    ),
    "input-keyboard": Icon(
        GROUP, rect(8, 20, 48, 26, rx=4) + "".join(rect(14 + 11 * c, 26, 7, 6, rx=1) for c in range(4))
        + "".join(rect(14 + 11 * c, 36, 7, 6, rx=1) for c in range(4)),
        rect(8, 20, 48, 26, rx=4, fill=VIOLET) + "".join(rect(14 + 11 * c, 26, 7, 6, rx=1, fill=PORCELAIN) for c in range(4))
        + "".join(rect(14 + 11 * c, 36, 7, 6, rx=1, fill=PORCELAIN) for c in range(4)),
    ),
    "input-mouse": Icon(
        GROUP, stroke("M32 12c-10 0-16 8-16 20v10c0 10 6 16 16 16s16-6 16-16V32c0-12-6-20-16-20z") + stroke("M32 20v10", width=4),
        filled("M32 12c-10 0-16 8-16 20v10c0 10 6 16 16 16s16-6 16-16V32c0-12-6-20-16-20z", fill=VIOLET) + stroke("M32 20v10", color=PORCELAIN, width=4),
    ),
    "input-tablet": Icon(
        GROUP, rect(10, 8, 44, 48, rx=6) + dot(32, 50, 2) + stroke("M40 40 54 54", width=4) + dot(54, 54, 2),
        rect(10, 8, 44, 48, rx=6, fill=VIOLET) + dot(32, 50, 2, PORCELAIN) + stroke("M40 40 54 54", color=INK, width=4) + dot(54, 54, 2, INK),
    ),
    "drive-harddisk": Icon(
        GROUP, rect(8, 14, 48, 36, rx=6) + ring(32, 32, 11) + dot(32, 32, 2),
        rect(8, 14, 48, 36, rx=6, fill=VIOLET) + ring(32, 32, 11, color=PORCELAIN, width=4) + dot(32, 32, 2, PORCELAIN),
    ),
    "network-wired": Icon(
        GROUP, rect(24, 14, 16, 24, rx=2) + strokes("M28 14v-4", "M32 14v-4", "M36 14v-4", width=3) + stroke("M32 38v18", width=5),
        rect(24, 14, 16, 24, rx=2, fill=BLUE) + strokes("M28 14v-4", "M32 14v-4", "M36 14v-4", color=INK, width=3) + stroke("M32 38v18", width=5),
    ),
    "network-wireless-signal-none": Icon(GROUP, _wifi_level(0), dot(32, 48, 3, BLUE)),
    "network-wireless-signal-weak": Icon(GROUP, _wifi_level(1), dot(32, 48, 3, BLUE) + stroke(WIFI_ARCS[0], color=BLUE, width=4)),
    "network-wireless-signal-ok": Icon(GROUP, _wifi_level(2), dot(32, 48, 3, BLUE) + stroke(WIFI_ARCS[0], color=BLUE, width=4) + stroke(WIFI_ARCS[1], color=BLUE, width=4)),
    "network-wireless-signal-good": Icon(GROUP, _wifi_level(3), dot(32, 48, 3, BLUE) + "".join(stroke(a, color=BLUE, width=4) for a in WIFI_ARCS[:3])),
    "network-wireless-signal-excellent": Icon(GROUP, _wifi_level(4), dot(32, 48, 3, BLUE) + "".join(stroke(a, color=BLUE, width=4) for a in WIFI_ARCS[:4])),
    "network-wireless-offline": Icon(
        GROUP, _wifi_level(3) + stroke("M12 12 52 52", width=6),
        dot(32, 48, 3, INK) + "".join(stroke(a, color=INK, width=4) for a in WIFI_ARCS[:3]) + stroke("M12 12 52 52", color=APRICOT, width=6),
    ),
    "network-wireless-error": Icon(
        GROUP, _wifi_level(2) + stroke("M50 14v10", width=5) + dot(50, 28, 2),
        dot(32, 48, 3, INK) + stroke(WIFI_ARCS[0], color=INK, width=4) + stroke(WIFI_ARCS[1], color=INK, width=4)
        + stroke("M50 14v10", color=APRICOT, width=5) + dot(50, 28, 2, APRICOT),
    ),
    # The radio is switched off: no bars are even attempted, just the base
    # dot crossed out -- distinct from "-disconnected" below, where the
    # radio is on but has not associated with a network yet.
    "network-wireless-off": Icon(
        GROUP, dot(32, 48, 3) + stroke("M12 12 52 52", width=6),
        dot(32, 48, 3, INK) + stroke("M12 12 52 52", color=APRICOT, width=6),
    ),
    # The radio is on and searching but has no association: a single dashed
    # arc (still trying, nothing confirmed) instead of a solid bar or a slash.
    "network-wireless-disconnected": Icon(
        GROUP,
        dot(32, 48, 3) + f'<path d="{WIFI_ARCS[0]}" fill="none" stroke="{INK}" stroke-width="4" stroke-linecap="round" stroke-dasharray="3 4"/>',
        dot(32, 48, 3, INK) + f'<path d="{WIFI_ARCS[0]}" fill="none" stroke="{BLUE}" stroke-width="4" stroke-linecap="round" stroke-dasharray="3 4"/>',
    ),
    "network-bluetooth-active": Icon(GROUP, stroke(BLUETOOTH_D), stroke(BLUETOOTH_D, color=BLUE, width=6)),
    "network-bluetooth-inactive": Icon(
        GROUP, stroke(BLUETOOTH_D) + stroke("M12 12 52 52", width=6),
        stroke(BLUETOOTH_D, color=INK, width=6) + stroke("M12 12 52 52", color=APRICOT, width=6),
    ),
}

ALIASES = {
    "bluetooth": ("network-bluetooth-active", None),
    "network-wireless": ("network-wireless-signal-excellent", None),
    # Runtime spelling used by the bluetooth applet; same "radio is on" glyph.
    "network-bluetooth-activated": ("network-bluetooth-active", None),
}
