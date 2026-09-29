# SPDX-License-Identifier: GPL-3.0-or-later
"""Add owner-provided wallpapers to data/wallpapers, and check the bundle.

`tools/import-wallpapers LIST.json` imports the images a JSON list names:
[{"source": "~/Pictures/Wallpapers/x.png", "title": "Neon Harbor",
  "description": "...", "credit": "Jarrod C, AI-assisted"}]. Each must be a
landscape image at least MINIMUM_WIDTH pixels wide; duplicates (by content
hash, against the list, the bundle, and the recorded sources) are skipped; an
existing bundled name is never replaced. `--check` validates the shipped
bundle and needs no image library. See ADR-0286 and docs/wiki/shell/wallpapers.md.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_DIRECTORY = ROOT / "data" / "wallpapers"
ARTWORK = "ARTWORK.md"
# AGENT-CONTRACT: the bundled format and native size of the shipped artwork
# (ADR-0078, data/wallpapers/ARTWORK.md). The shell and the Appearance catalog
# resolve these extensions in this order (ADR-0228); keep them in step.
TARGET_SIZE = (1672, 941)
MINIMUM_WIDTH = 1536
FORMATS = ("png", "jpg", "jpeg", "webp", "bmp")
# The Appearance chooser derives its label from the file name
# ("neon-harbor" -> "Neon harbor"), so the name is always the title's slug.
NAME = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
DEFAULT_CREDIT = "Jarrod C, AI-assisted"
FIELDS = {"source", "title", "description", "credit"}
COLLECTION_HEADING = "## Owner collection"
COLLECTION_INTRO = (
    "Wallpapers the project owner generated and chose for QindaQt, added with\n"
    "`tools/import-wallpapers` (ADR-0286). Each section names the image's title and\n"
    "credit and records the SHA-256 of the exact source file that was imported.\n"
)
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
SOURCE_HASH = re.compile(r"source SHA-256 ([0-9a-f]{64})")


class WallpaperImportError(Exception):
    """A list entry or the bundle breaks one of the import rules."""


@dataclass(frozen=True)
class Entry:
    source: Path
    title: str
    credit: str
    description: str

    @property
    def name(self) -> str:
        return slug(self.title)


@dataclass(frozen=True)
class Planned:
    entry: Entry
    digest: str
    size: tuple[int, int]


def slug(title: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", title.lower()).strip("-")


def png_header(path: Path) -> bytes | None:
    with path.open("rb") as handle:
        header = handle.read(29)
    if len(header) < 29 or header[:8] != PNG_SIGNATURE or header[12:16] != b"IHDR":
        return None
    return header


def image_size(path: Path) -> tuple[int, int]:
    header = png_header(path)
    if header is not None:
        width, height = struct.unpack(">II", header[16:24])
        return width, height
    try:
        from PIL import Image
    except ImportError as error:
        raise WallpaperImportError(f"{path}: not a PNG, and Pillow is unavailable to read it") from error
    with Image.open(path) as image:
        return image.size


def is_bundled_png(path: Path, size: tuple[int, int]) -> bool:
    """True when the file already is an 8-bit RGB, non-interlaced PNG at the bundled size."""
    header = png_header(path)
    return header is not None and size == TARGET_SIZE and header[24] == 8 and header[25] == 2 \
        and header[28] == 0


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def bundled_images(directory: Path) -> list[Path]:
    return sorted(path for path in directory.iterdir()
                  if path.is_file() and path.suffix.lower().lstrip(".") in FORMATS)


def load_entries(list_path: Path) -> list[Entry]:
    try:
        data = json.loads(list_path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise WallpaperImportError(f"{list_path}: {error}") from error
    if not isinstance(data, list) or not data:
        raise WallpaperImportError(f"{list_path}: expected a non-empty JSON array")
    entries = []
    for index, item in enumerate(data):
        if not isinstance(item, dict) or not set(item) <= FIELDS:
            raise WallpaperImportError(f"entry {index}: expected an object with only {sorted(FIELDS)}")
        title = " ".join(str(item.get("title", "")).split())
        if not 0 < len(title) <= 64 or not NAME.match(slug(title)):
            raise WallpaperImportError(f"entry {index}: a title of 1-64 characters is required")
        entries.append(Entry(source=Path(str(item.get("source", ""))).expanduser(),
                             title=title,
                             credit=" ".join(str(item.get("credit") or DEFAULT_CREDIT).split()),
                             description=" ".join(str(item.get("description", "")).split())))
    return entries


def plan(entries: list[Entry], directory: Path) -> tuple[list[Planned], list[tuple[Entry, str]]]:
    artwork = directory / ARTWORK
    known = set(SOURCE_HASH.findall(artwork.read_text(encoding="utf-8"))) if artwork.is_file() else set()
    known |= {sha256(path) for path in bundled_images(directory)}
    planned: list[Planned] = []
    skipped: list[tuple[Entry, str]] = []
    for entry in entries:
        if not entry.source.is_file():
            raise WallpaperImportError(f"{entry.source}: no such file")
        width, height = image_size(entry.source)
        if width <= height or width < MINIMUM_WIDTH:
            raise WallpaperImportError(
                f"{entry.source}: {width}x{height} is not a landscape wallpaper at least "
                f"{MINIMUM_WIDTH} px wide")
        digest = sha256(entry.source)
        if digest in known:
            skipped.append((entry, "same image already bundled or listed"))
            continue
        for extension in FORMATS:
            existing = directory / f"{entry.name}.{extension}"
            if existing.exists():
                raise WallpaperImportError(
                    f"{existing.name} already exists; bundled wallpapers are never replaced")
        if any(item.entry.name == entry.name for item in planned):
            raise WallpaperImportError(f"two entries would both be named {entry.name}")
        known.add(digest)
        planned.append(Planned(entry, digest, (width, height)))
    return planned, skipped


def convert(source: Path, target: Path) -> None:
    try:
        from PIL import Image, ImageOps
    except ImportError as error:
        raise WallpaperImportError(f"{source}: converting needs Pillow") from error
    with Image.open(source) as image:
        fitted = ImageOps.fit(image.convert("RGB"), TARGET_SIZE, method=Image.Resampling.LANCZOS)
        fitted.save(target, format="PNG", optimize=True)


def credit_section(item: Planned) -> str:
    entry = item.entry
    description = f" {entry.description}" if entry.description else ""
    return (f"### {entry.name}\n\n"
            f"Title: {entry.title}. Credit: {entry.credit}.{description} Bundled as a "
            f"{TARGET_SIZE[0]} × {TARGET_SIZE[1]} PNG; source SHA-256 {item.digest}.\n")


def write(planned: list[Planned], directory: Path) -> None:
    for item in planned:
        target = directory / f"{item.entry.name}.png"
        # A source already in the bundled format is copied byte for byte, so
        # the recorded source hash is also the shipped file's hash.
        if is_bundled_png(item.entry.source, item.size):
            shutil.copyfile(item.entry.source, target)
        else:
            convert(item.entry.source, target)
    if not planned:
        return
    artwork = directory / ARTWORK
    text = artwork.read_text(encoding="utf-8") if artwork.is_file() else "# QindaQt wallpapers\n"
    if COLLECTION_HEADING not in text:
        text = text.rstrip("\n") + f"\n\n{COLLECTION_HEADING}\n\n{COLLECTION_INTRO}"
    for item in planned:
        text = text.rstrip("\n") + "\n\n" + credit_section(item)
    artwork.write_text(text, encoding="utf-8")


def check(directory: Path) -> tuple[list[Path], list[str]]:
    """Every bundled image is landscape, wide enough, slug-named and credited."""
    artwork = directory / ARTWORK
    sections = set(re.findall(r"^### ([a-z0-9-]+)[ \t]*$", artwork.read_text(encoding="utf-8"),
                              re.MULTILINE)) if artwork.is_file() else set()
    images = bundled_images(directory)
    problems = [] if images else [f"{directory}: no bundled wallpapers"]
    for image in images:
        if not NAME.match(image.stem):
            problems.append(f"{image.name}: the name must be lowercase words joined by hyphens")
        if image.stem not in sections:
            problems.append(f"{image.name}: no '### {image.stem}' credit section in {ARTWORK}")
        try:
            width, height = image_size(image)
        except (OSError, WallpaperImportError) as error:
            problems.append(f"{image.name}: {error}")
            continue
        if width <= height or width < MINIMUM_WIDTH:
            problems.append(f"{image.name}: {width}x{height} is not landscape and at least "
                            f"{MINIMUM_WIDTH} px wide")
    return images, problems


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("list", nargs="?", type=Path, help="JSON list of wallpapers to import")
    parser.add_argument("--wallpapers", type=Path, default=DEFAULT_DIRECTORY,
                        help="bundled wallpaper directory (default: data/wallpapers)")
    parser.add_argument("--dry-run", action="store_true", help="report the plan, write nothing")
    parser.add_argument("--check", action="store_true", help="validate the bundle and exit")
    args = parser.parse_args(argv)
    try:
        if args.check:
            images, problems = check(args.wallpapers)
            for problem in problems:
                print(f"ERROR: {problem}", file=sys.stderr)
            if not problems:
                print(f"{len(images)} bundled wallpapers are landscape, named and credited")
            return 1 if problems else 0
        if args.list is None:
            parser.error("a wallpaper list is required unless --check is given")
        planned, skipped = plan(load_entries(args.list), args.wallpapers)
        for entry, reason in skipped:
            print(f"skip   {entry.source}: {reason}")
        for item in planned:
            print(f"import {item.entry.source} -> {item.entry.name}.png ({item.entry.title})")
        if not args.dry_run:
            write(planned, args.wallpapers)
        return 0
    except WallpaperImportError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1
