#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Names every Qinda icon theme must resolve: QindaQt and all QindaThemes families.

AGENT-CONTRACT: one floor shared by tools/validate_qinda_icon_theme.py here
and QindaThemes' tests (this module is synced there verbatim). The desktop
icon surface resolves REQUIRED_DESKTOP; file managers resolve the places and
per-format MIME names; launchers resolve the Qinda desktop `Icon=` ids.
Removing a name here is a contract change (ADR-0283), not a cleanup.
"""
REQUIRED_DESKTOP = {
    "user-home", "folder-documents", "folder-download", "folder-pictures", "folder-videos",
    "folder-music", "user-trash", "user-trash-full", "computer", "network-workgroup",
}
REQUIRED_PLACES = REQUIRED_DESKTOP | {
    "folder", "user-desktop", "folder-books", "folder-ebooks", "folder-templates", "folder-publicshare",
    "folder-projects", "folder-games", "folder-development", "folder-remote", "folder-cloud",
}
REQUIRED_TYPES = {
    "text-plain", "text-markdown", "text-x-python", "text-x-csrc", "text-x-c++src", "text-x-chdr",
    "application-javascript", "text-x-typescript", "text-rust", "text-x-go", "text-x-java",
    "application-x-shellscript", "text-html", "text-css", "application-json", "application-xml",
    "application-yaml", "application-toml", "application-vnd.oasis.opendocument.text",
    "application-vnd.oasis.opendocument.spreadsheet", "application-vnd.oasis.opendocument.presentation",
    "application-vnd.openxmlformats-officedocument.wordprocessingml.document",
    "application-vnd.openxmlformats-officedocument.spreadsheetml.sheet",
    "application-vnd.openxmlformats-officedocument.presentationml.presentation", "application-pdf",
    "application-epub+zip", "application-x-mobipocket-ebook", "application-x-cbz", "image-png", "image-jpeg",
    "image-gif", "image-webp", "image-svg+xml", "image-x-dcraw", "audio-mpeg", "audio-flac", "audio-ogg",
    "audio-x-wav", "video-mp4", "video-x-matroska", "video-webm", "application-zip", "application-x-tar",
    "application-x-compressed-tar", "application-x-xz", "application-x-7z-compressed", "application-vnd.rar",
    "application-x-cd-image", "application-x-appimage", "application-vnd.flatpak.ref", "application-x-rpm",
    "application-x-deb", "font-ttf", "font-otf", "application-x-executable", "application-x-bittorrent",
    "text-calendar", "text-vcard", "application-x-sloom", "application-x-slppr", "application-x-qinda-notebook",
    "application-x-qindabooks", "application-x-qindadiagram", "application-x-qindaplan", "application-x-qindabase",
}
REQUIRED_FIRST_PARTY = {
    "org.qindaqt.QindaStudio", "sloom-studio", "org.qindaqt.QindaOffice", "org.qindaqt.QindaWrite",
    "org.qindaqt.QindaCalc", "org.qindaqt.QindaShow", "org.qindaqt.QindaBase", "org.qindaqt.QindaPlan",
    "org.qindaqt.QindaDiagram", "org.qindaqt.QindaMail", "org.qindaqt.QindaNote", "org.qindaqt.QindaBooks",
    "org.qindaqt.QindaDeck", "qqmpv", "com.github.es00bac.venusprolinux", "qqterm", "org.qindaqt.QindaLutris",
    "qindafox", "org.qindaqt.FileManager", "org.qindaqt.TextEditor", "org.qindaqt.SystemMonitor",
    "org.qindaqt.Calendar", "qindaqt-viewer", "qindaqt-voice", "gabbee", "org.qindaqt.Settings",
}

REQUIRED_ALL = REQUIRED_PLACES | REQUIRED_TYPES | REQUIRED_FIRST_PARTY
