# Bundled-apps shipping ebuild prepared (kimi, follow-up)

Commit `79c8b4cf` adds
`packaging/gentoo/gui-wm/qindaqt-desktop/qindaqt-desktop-0.1.0_pre20260910.ebuild`,
pinning `0e44c65b` (the ADR-0116 bundled-apps close-out: Calendar, File
Manager, Text Editor, Terminal on stock Qt 6). It mirrors the r3 ebuild
exactly apart from the commit pin; `files/` needs no patches and the
`kcalendarcore` dependency added by the calendar lane is retained.

The Manifest is intentionally untouched — the release lane regenerates it
during the operator archive/digest flow. The running session still carries
the pre-program apps until that emerge lands. HANDOFF (same commit) now
names the exact ebuild to ship.
