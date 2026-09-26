#!/usr/bin/env python3
"""Validate static routes, references, accessible images, and film assets."""

from __future__ import annotations

import sys
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit


SITE = Path(__file__).resolve().parents[1]


class Page(HTMLParser):
    def __init__(self, path: Path):
        super().__init__(convert_charrefs=True)
        self.path = path
        self.ids: set[str] = set()
        self.refs: list[tuple[str, str]] = []
        self.errors: list[str] = []
        self.videos = 0
        self.tracks = 0

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        attributes = dict(attrs)
        item_id = attributes.get("id")
        if item_id:
            if item_id in self.ids:
                self.errors.append(f"duplicate id {item_id}")
            self.ids.add(item_id)
        if tag == "img" and "alt" not in attributes:
            self.errors.append(f"image without alt: {attributes.get('src')}")
        if tag == "button" and "type" not in attributes:
            self.errors.append("button without type")
        if tag == "video":
            self.videos += 1
        if tag == "track" and attributes.get("kind") == "captions":
            self.tracks += 1
        for name in ("href", "src"):
            value = attributes.get(name)
            if value:
                self.refs.append((name, value))
        for name in ("data-image", "data-wallpaper"):
            value = attributes.get(name)
            if value:
                self.refs.append((name, f"assets/images/{value}"))


def resolve(page: Page, raw: str) -> tuple[Path, str] | None:
    url = urlsplit(raw)
    if url.scheme or url.netloc or raw.startswith("mailto:"):
        return None
    if raw.startswith("#"):
        return page.path, unquote(url.fragment)
    target = (page.path.parent / unquote(url.path)).resolve()
    if target.is_dir():
        target /= "index.html"
    return target, unquote(url.fragment)


def main() -> int:
    pages = {}
    errors = []
    for path in SITE.rglob("*.html"):
        page = Page(path)
        page.feed(path.read_text(encoding="utf-8"))
        pages[path.resolve()] = page
        errors.extend(f"{path.relative_to(SITE)}: {error}" for error in page.errors)
        if page.videos != page.tracks:
            errors.append(f"{path.relative_to(SITE)}: {page.videos} videos but {page.tracks} caption tracks")
    for page in pages.values():
        for kind, raw in page.refs:
            destination = resolve(page, raw)
            if destination is None:
                continue
            target, fragment = destination
            if not target.is_file():
                errors.append(f"{page.path.relative_to(SITE)}: missing {kind} target {raw}")
            elif fragment and target in pages and fragment not in pages[target].ids:
                errors.append(f"{page.path.relative_to(SITE)}: missing anchor {raw}")
    for error in errors:
        print(error, file=sys.stderr)
    if errors:
        return 1
    print(f"Validated {len(pages)} pages, local assets, anchors, image alt text, and captions.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
