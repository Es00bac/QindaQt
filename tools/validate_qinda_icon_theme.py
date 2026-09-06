#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Validate QindaQt's generated scalable icon catalog without Qt tooling.

AGENT-CONTRACT: this is the enforcement point for the "no fallback glyph"
rule -- it fails if two distinct canonical (non-alias) names render identical
geometry, since that is exactly what a group-level or index-based fallback
would produce. Aliases are expected to match their target byte-for-byte.
"""
import json
import re
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path

from qinda_icon_catalog import ALIASES, CANON, GROUP_CONTEXT, GROUPS
from qinda_icon_shapes import AMBER, INK, JADE, PORCELAIN, render

ROOT = Path(__file__).resolve().parents[1] / "data/icons/QindaQt"
REPO_ROOT = Path(__file__).resolve().parents[1]
COVERAGE_CMAKE = REPO_ROOT / "tests/shell/verify_shell_icon_coverage.cmake"

# Every literal name `require_icon(...)` pins in the shell's existing icon
# coverage contract (tests/shell/verify_shell_icon_coverage.cmake), with any
# "-symbolic" suffix stripped: the generator always writes both forms for a
# base name, so requiring the base name is sufficient. A rename in that
# cmake file must be coordinated here, not made unilaterally in either
# direction -- this must track the *complete* contract, not a hand-picked
# subset of it.
def _required_names_from_coverage_cmake() -> set[str]:
    text = COVERAGE_CMAKE.read_text(encoding="utf-8")
    foreach_names: set[str] = set()
    for block in re.findall(r"foreach\(icon_name IN ITEMS\s*(.*?)\)", text, re.S):
        foreach_names.update(block.split())
    direct = set(re.findall(r'require_icon\("[^"]+"\s*,?\s*\n?\s*"([^"]+)"\)', text))
    names = {n.replace("-symbolic", "") for n in (foreach_names | direct)}
    names.discard("${icon_name}")
    if len(names) < 40:
        raise SystemExit(
            f"only parsed {len(names)} required name(s) out of "
            f"{COVERAGE_CMAKE}; the extraction regex likely drifted from "
            "the file's actual shape -- fix the parser, don't shrink the contract"
        )
    return names


REQUIRED_NAMES = _required_names_from_coverage_cmake()


def _expected_paths():
    for name, icon in CANON.items():
        yield name, icon.group
    for alias, (_, group) in ALIASES.items():
        yield alias, group


def main() -> None:
    index = (ROOT / "index.theme").read_text(encoding="utf-8")
    if "Name=QindaQt" not in index or "Inherits=hicolor" not in index:
        raise SystemExit("index.theme lacks the public QindaQt/hicolor contract")
    for group in GROUPS:
        if f"[scalable/{group}]" not in index or f"Context={GROUP_CONTEXT[group]}" not in index:
            raise SystemExit(f"index.theme is missing a Context= entry for scalable/{group}")

    missing = REQUIRED_NAMES - (set(CANON) | set(ALIASES))
    if missing:
        raise SystemExit(f"required built-in icon name(s) dropped from the catalog: {sorted(missing)}")

    expected = []
    for name, group in _expected_paths():
        expected.append(ROOT / "scalable" / group / f"{name}.svg")
        expected.append(ROOT / "scalable" / group / f"{name}-symbolic.svg")
    on_disk = [p for group in GROUPS for p in (ROOT / "scalable" / group).glob("*.svg")] if (ROOT / "scalable").is_dir() else []
    missing_files = [str(p) for p in expected if not p.is_file()]
    if missing_files:
        raise SystemExit("missing icon files:\n" + "\n".join(missing_files))
    stray_files = sorted(set(str(p) for p in on_disk) - set(str(p) for p in expected))
    if stray_files:
        raise SystemExit("catalog does not account for on-disk file(s):\n" + "\n".join(stray_files))

    for path in expected:
        root = ET.parse(path).getroot()
        if not root.tag.endswith("svg") or root.attrib.get("viewBox") != "0 0 64 64":
            raise SystemExit(f"invalid scalable SVG: {path}")
        _check_no_stray_text(path, root)

    _check_semantic_uniqueness()
    _check_identity_palette()
    _check_wifi_alpha_masks()
    print(f"validated {len(expected)} QindaQt SVG assets: {len(CANON)} canonical icons, "
          f"{len(ALIASES)} aliases, {len(GROUPS)} semantic groups")


def _check_no_stray_text(path: Path, root: ET.Element) -> None:
    """Catch a fragment built by string-concatenating raw path data.

    A helper call missing its `<path .../>` wrapper (the exact defect that
    made every `network-wireless-signal-*-symbolic` icon render as a bare
    dot: `"".join(WIFI_ARCS[:bars])` instead of
    `"".join(stroke(a) for a in WIFI_ARCS[:bars])`) leaves the raw `d=`
    string sitting as text content between elements instead of inside a
    tag, which ElementTree exposes as non-whitespace `.text`/`.tail`.
    """
    for element in root.iter():
        for chunk, where in ((element.text, "text"), (element.tail, "tail")):
            if chunk and chunk.strip():
                raise SystemExit(
                    f"{path}: stray non-whitespace {where} content on <{element.tag}>: "
                    f"{chunk.strip()!r} -- a drawing helper call is likely unwrapped raw path data"
                )


WIFI_LEVELS = (
    "network-wireless-signal-none", "network-wireless-signal-weak",
    "network-wireless-signal-ok", "network-wireless-signal-good",
    "network-wireless-signal-excellent",
)


def _check_wifi_alpha_masks() -> None:
    """Render each wifi level and compare painted-pixel alpha, not source text.

    `_check_semantic_uniqueness` compares generated SVG strings, which is
    exactly what missed the reported defect: every level's *source* differed
    (different `d=` data was concatenated), but the unwrapped strings never
    became paintable elements, so every rendered symbolic icon was in fact
    an identical bare dot. This renders the real pixels with rsvg-convert
    and asserts they differ and that ink coverage increases with the level,
    catching a "source looks distinct, pixels are not" regression.
    """
    from PIL import Image

    sizes = []
    masks = []
    for name in WIFI_LEVELS:
        svg_path = ROOT / "scalable" / "devices" / f"{name}-symbolic.svg"
        png_path = svg_path.with_suffix(".validate-check.png")
        result = subprocess.run(
            ["rsvg-convert", "-w", "64", "-h", "64", "-o", str(png_path), str(svg_path)],
            capture_output=True, text=True,
        )
        if result.returncode != 0:
            raise SystemExit(f"rsvg-convert failed rendering {svg_path}: {result.stderr}")
        try:
            with Image.open(png_path) as image:
                alpha = image.convert("RGBA").getchannel("A")
                mask = alpha.tobytes()
                opaque_pixels = sum(1 for byte in mask if byte > 0)
        finally:
            png_path.unlink(missing_ok=True)
        masks.append(mask)
        sizes.append(opaque_pixels)

    for i, name_a in enumerate(WIFI_LEVELS):
        for name_b, mask_b in zip(WIFI_LEVELS[i + 1:], masks[i + 1:]):
            if masks[i] == mask_b:
                raise SystemExit(
                    f"{name_a} and {name_b} render pixel-identical symbolic artwork "
                    "(their SVG source differs, but nothing painted the difference -- "
                    "check for a drawing helper call missing its element wrapper)"
                )

    if sizes != sorted(sizes):
        raise SystemExit(
            f"wifi signal levels should paint strictly more ink as signal improves, "
            f"got opaque-pixel counts {list(zip(WIFI_LEVELS, sizes))}"
        )


def _check_semantic_uniqueness() -> None:
    """Two distinct canonical names must not draw the same artwork.

    A shared drawing convention (e.g. the settings-for-subsystem gear badge)
    is fine; the *complete* fragment for two unrelated names matching exactly
    means one of them is an undeclared copy-paste fallback rather than its
    own glyph.
    """
    seen: dict[tuple[str, str], str] = {}
    for name, icon in CANON.items():
        key = (render(icon, symbolic=False), render(icon, symbolic=True))
        if key in seen:
            raise SystemExit(
                f"canonical icons {seen[key]!r} and {name!r} render identical artwork; "
                "give one its own semantic glyph or declare it as an alias instead"
            )
        seen[key] = name


def _check_identity_palette() -> None:
    """Pin the icon palette to the packaged QindaPunk theme roles.

    The first-party dock shipped with retired Mineral Light jade discs after
    the themes moved to Nightfall/Porcelain because nothing tied
    qinda_icon_shapes.py to data/themes. INK, PORCELAIN, and AMBER must equal
    the dark canvas, light surface, and dark accent roles, and JADE (a status
    color) must not appear in any first-party `apps` color artwork.
    """
    dark = json.loads((REPO_ROOT / "data/themes/qinda-dark.json").read_text(encoding="utf-8"))["colors"]
    light = json.loads((REPO_ROOT / "data/themes/qinda-light.json").read_text(encoding="utf-8"))["colors"]
    for label, actual, expected in (
        ("INK", INK, dark["canvas"]), ("AMBER", AMBER, dark["accent"]), ("PORCELAIN", PORCELAIN, light["surface"]),
    ):
        if actual.lower() != expected.lower():
            raise SystemExit(
                f"qinda_icon_shapes.{label} is {actual} but the packaged theme role is {expected}; "
                "update the icon palette and regenerate, or supersede the identity deliberately"
            )
    jade_apps = sorted(name for name, icon in CANON.items() if icon.group == "apps" and JADE.lower() in icon.color.lower())
    if jade_apps:
        raise SystemExit(f"first-party apps artwork must not use JADE as a brand color: {jade_apps}")


if __name__ == "__main__":
    main()
