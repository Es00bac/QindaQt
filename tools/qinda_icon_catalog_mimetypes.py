#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Canonical file-type glyphs: one page silhouette, a colored band, a content glyph.

AGENT-NOTE: every type shares the page silhouette so a file list reads as
files; the type is told by BOTH the band color and the content glyph (never
color alone). Names are the freedesktop MIME icon names (`/` -> `-`);
common spellings are aliases, not separate drawings. Owner formats: Sloom
Studio projects are `application/x-sloom` and Paper layouts
`application/x-slppr` (flow/packaging/arch/sloom-studio.xml); QindaOffice
native types come from QindaOffice/data/mime/qindaoffice.xml.

AGENT-CONTRACT: QindaThemes copies this module verbatim and re-tones the band
and glyph colors per family; keep art to paths/rects/circles so every family
renderer (chamfer, outline, pixel grid, glass) can process it.
"""
import qinda_icon_type_marks as g
from qinda_icon_color import luminance, shade
from qinda_icon_shapes import INK, Icon, filled, rect, ring, stroke

GROUP = "mimetypes"

PAGE_D = "M13 5h25l13 13v41H13z"
FOLD_D = "M38 5v13h13z"
BAND_D = "M13 45h38v14H13z"
PAPER = "#fbf8f1"
PAGE_EDGE = "#cbc4b6"


def glyph_tone(band: str) -> str:
    return band if luminance(band) < 0.33 else shade(band, -0.58)


def page(band: str, glyph, color_glyph=None) -> Icon:
    art = (filled(PAGE_D, PAPER) + filled(BAND_D, band)
           + filled(FOLD_D, shade(band, 0.45)) + stroke(PAGE_D, 1.4, PAGE_EDGE, join="round")
           + (color_glyph(glyph_tone(band)) if color_glyph else glyph(glyph_tone(band))))
    symbolic = stroke(PAGE_D, width=4) + stroke("M38 5v13h13", width=3) + glyph(INK)
    return Icon(GROUP, symbolic, art)


def _python(_tone: str) -> str:
    return filled(g.PY_TOP_D, "#3572a5") + filled(g.PY_BOTTOM_D, "#f2c230")


def _terminal(_tone: str) -> str:
    return rect(16, 15, 32, 26, rx=3, fill="#2e3440") + g.shell("#8fe060")


def _sloom(_tone: str) -> str:
    return "".join(ring(x, y, 6.5, 2.8, c) for (x, y), c in zip(
        ((26.5, 22.5), (37.5, 22.5), (26.5, 33.5), (37.5, 33.5)), ("#ef4f63", "#f39a2e", "#3f7ff0", "#22b8c9")))


# name: (band color, glyph, aliases[, color-only glyph])
TYPES = {
    "text-x-generic": ("#7d8ea3", g.text_lines, ["text-plain", "application-octet-stream", "text-x-log"]),
    "text-markdown": ("#3b4252", g.markdown, ["text-x-markdown"]),
    "text-x-readme": ("#8a64c8", g.info, []),
    "text-x-script": ("#556b8a", g.script, ["text-x-perl", "application-x-perl", "text-x-ruby", "application-x-ruby",
                                            "text-x-php", "application-x-php", "text-x-lua", "text-x-sql",
                                            "application-sql", "text-x-makefile", "text-x-cmake"]),
    "text-x-python": ("#3572a5", g.python, ["text-x-python3", "application-x-python", "application-x-python-code"], _python),
    "text-x-csrc": ("#5a6b85", g.c_lang, ["text-x-c"]),
    "text-x-c++src": ("#d6456f", g.cpp, ["text-x-cpp", "text-x-c++"]),
    "text-x-chdr": ("#8a5cc0", g.header, ["text-x-c++hdr", "text-x-h"]),
    "application-javascript": ("#e8c93a", g.javascript, ["text-javascript", "application-x-javascript", "text-x-javascript"]),
    "text-x-typescript": ("#3178c6", g.typescript, ["application-x-typescript", "application-typescript"]),
    "text-rust": ("#c4562b", g.rust, ["text-x-rust"]),
    "text-x-go": ("#00a2cc", g.go_lang, ["text-go"]),
    "text-x-java": ("#b56a1e", g.java, ["text-x-java-source", "application-x-java"]),
    "application-x-shellscript": ("#2e3440", g.shell, ["text-x-sh", "application-x-sh", "text-x-shellscript",
                                                       "application-x-executable-script"], _terminal),
    "text-html": ("#e3572a", g.html, ["text-x-html", "application-xhtml+xml"]),
    "text-css": ("#5b44a8", g.css, ["text-x-css"]),
    "application-json": ("#9a7b1c", g.json, ["application-x-json", "application-schema+json", "application-ld+json"]),
    "application-xml": ("#2a6fb0", g.xml, ["text-xml", "application-x-xml"]),
    "application-yaml": ("#c42b3a", g.yaml, ["text-yaml", "application-x-yaml", "text-x-yaml"]),
    "application-toml": ("#9c4a2a", g.toml, ["text-x-toml", "application-x-toml"]),
    "text-csv": ("#2f9e5b", g.csv, ["text-x-csv", "text-tab-separated-values"]),
    "application-vnd.oasis.opendocument.text": ("#2a7fd4", g.paragraphs, [
        "application-vnd.oasis.opendocument.text-template", "application-rtf", "text-rtf", "x-office-document"]),
    "application-vnd.oasis.opendocument.spreadsheet": ("#21a453", g.sheet, [
        "application-vnd.oasis.opendocument.spreadsheet-template", "x-office-spreadsheet"]),
    "application-vnd.oasis.opendocument.presentation": ("#e8732c", g.slides, [
        "application-vnd.oasis.opendocument.presentation-template", "x-office-presentation"]),
    "application-vnd.openxmlformats-officedocument.wordprocessingml.document": ("#2b579a", g.word, [
        "application-msword", "application-vnd.ms-word", "application-vnd.openxmlformats-officedocument.wordprocessingml.template"]),
    "application-vnd.openxmlformats-officedocument.spreadsheetml.sheet": ("#1d7044", g.excel, [
        "application-vnd.ms-excel", "application-vnd.openxmlformats-officedocument.spreadsheetml.template"]),
    "application-vnd.openxmlformats-officedocument.presentationml.presentation": ("#c4441e", g.powerpoint, [
        "application-vnd.ms-powerpoint", "application-vnd.openxmlformats-officedocument.presentationml.template"]),
    "application-pdf": ("#d93025", g.pdf, ["application-x-pdf"]),
    "application-epub+zip": ("#7fb52d", g.epub, ["application-x-epub"]),
    "application-x-mobipocket-ebook": ("#f08c1f", g.mobi, ["application-vnd.amazon.mobi8-ebook", "application-x-mobi8-ebook",
                                                         "application-x-fictionbook+xml", "application-x-kindle"]),
    "application-vnd.comicbook+zip": ("#8b4fc7", g.comic, ["application-x-cbz", "application-x-cbr", "application-x-cb7",
                                                           "application-vnd.comicbook-rar"]),
    "image-x-generic": ("#3aa37a", g.picture, ["image-bmp", "image-tiff", "image-avif", "image-heic", "image-heif", "image-jxl"]),
    "image-png": ("#2a9d8f", g.png, ["image-apng"]),
    "image-jpeg": ("#4d9a3a", g.jpeg, ["image-jpg", "image-pjpeg"]),
    "image-gif": ("#d63384", g.gif, []),
    "image-webp": ("#1e8fc6", g.webp, []),
    "image-svg+xml": ("#f08a24", g.vector, ["image-svg+xml-compressed", "image-x-eps", "application-postscript",
                                            "application-illustrator"]),
    "image-x-dcraw": ("#4b4f58", g.aperture, ["image-x-raw", "image-x-adobe-dng", "image-x-canon-cr2", "image-x-canon-cr3",
                                              "image-x-nikon-nef", "image-x-sony-arw", "image-x-panasonic-rw2",
                                              "image-x-fuji-raf", "image-x-olympus-orf"]),
    "audio-x-generic": ("#b8568f", g.speaker, ["audio-aac", "audio-mp4", "audio-x-m4a", "audio-midi", "audio-x-midi"]),
    "audio-mpeg": ("#e94f8a", g.note, ["audio-mp3", "audio-x-mp3", "audio-x-mpeg"]),
    "audio-flac": ("#d99a1e", g.bars, ["audio-x-flac"]),
    "audio-ogg": ("#7b52c7", g.sine, ["audio-x-vorbis+ogg", "audio-x-opus+ogg", "audio-opus", "application-ogg", "audio-webm"]),
    "audio-x-wav": ("#2f7fd8", g.square_wave, ["audio-wav", "audio-vnd.wave", "audio-x-aiff", "audio-aiff"]),
    "video-x-generic": ("#cf4a4a", g.clapper, ["video-mpeg", "video-x-msvideo", "video-x-flv", "video-3gpp", "video-ogg"]),
    "video-mp4": ("#e2463d", g.screen_play, ["video-x-m4v", "video-quicktime"]),
    "video-x-matroska": ("#2b7a78", g.film, ["video-matroska", "video-x-matroska-3d"]),
    "video-webm": ("#3a61c4", g.circle_play, []),
    "application-zip": ("#c7902a", g.zipper, ["application-x-zip", "application-x-zip-compressed"]),
    "application-x-compressed-tar": ("#8a5a35", g.box_outline, [
        "application-x-tar", "application-x-archive", "application-gzip", "application-x-gzip", "application-x-xz",
        "application-x-xz-compressed-tar", "application-x-bzip", "application-x-bzip2", "application-x-bzip-compressed-tar",
        "application-x-zstd", "application-zstd", "application-x-zstd-compressed-tar", "application-x-lzma",
        "application-x-lz4", "application-x-cpio"]),
    "application-x-7z-compressed": ("#3a3f4b", g.seven, ["application-x-7z"]),
    "application-vnd.rar": ("#7a3f9e", g.stacked, ["application-x-rar", "application-x-rar-compressed"]),
    "application-x-cd-image": ("#7f8c9b", g.disc, ["application-x-iso", "application-x-iso9660-image",
                                                   "application-x-raw-disk-image", "application-x-qemu-disk"]),
    "application-x-appimage": ("#3f7fd6", g.cube, ["application-vnd.appimage"]),
    "application-vnd.flatpak.ref": ("#1ea5b8", g.layers, ["application-vnd.flatpak", "application-vnd.flatpak.repo",
                                                          "application-vnd.flatpak.bundle"]),
    "application-x-rpm": ("#cc2f2f", g.package, ["application-x-redhat-package-manager", "application-x-source-rpm"]),
    "application-vnd.debian.binary-package": ("#a3134b", g.swirl, ["application-x-deb", "application-x-debian-package"]),
    "font-ttf": ("#4b5b9c", g.font_tt, ["application-x-font-ttf", "font-sfnt", "application-x-font-truetype"]),
    "font-otf": ("#9c4b8e", g.font_aa, ["application-x-font-otf", "font-woff", "font-woff2", "application-font-woff",
                                        "application-x-font-type1"]),
    "application-x-bittorrent": ("#4a9e3f", g.magnet, ["application-x-magnet"]),
    "text-calendar": ("#e0463a", g.calendar, ["text-x-vcalendar", "application-ics", "text-x-ical"]),
    "text-vcard": ("#1d9a8a", g.vcard, ["text-x-vcard", "text-directory"]),
    "application-x-desktop": ("#6b7fa3", g.launcher, []),
    "application-pgp-keys": ("#c99a2e", g.key, ["application-pkix-cert", "application-x-pem-file", "application-pkcs12",
                                                "application-x-x509-ca-cert", "application-pgp-encrypted"]),
    "application-x-blender": ("#e87d0d", g.blend, ["model-stl", "model-obj", "model-gltf+json", "model-gltf-binary"]),
    "application-vnd.sqlite3": ("#0f80cc", g.cylinder, ["application-x-sqlite3", "application-x-sqlite"]),
    "application-x-sloom": ("#2a2440", g.rings4, [], _sloom),
    "application-x-slppr": ("#3656c4", g.layout, []),
    "application-x-qinda-notebook": ("#e2a72c", g.notebook, []),
    "application-x-qindabooks": ("#2f8f5a", g.coins, []),
    "application-x-qindadiagram": ("#e07b39", g.flowchart, []),
    "application-x-qindaplan": ("#3a7bd5", g.gantt, []),
    "application-x-qindabase": ("#7a5bd0", g.form, []),
}

CANON = {name: page(band, glyph, *extra) for name, (band, glyph, _aliases, *extra) in TYPES.items()}
ALIASES: dict[str, tuple[str, str | None]] = {
    alias: (name, None) for name, (_band, _glyph, aliases, *_extra) in TYPES.items() for alias in aliases
}
ALIASES["inode-directory"] = ("folder", "mimetypes")
