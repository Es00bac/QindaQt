# ADR-0283: True-color places, file types and Qinda app marks from one shared catalog

- **Status:** Accepted
- **Date:** 2026-09-28
- **Owners:** Shell iconography (QindaQt icon theme) with the QindaThemes icon families
- **Supersedes:** None (extends the icon palette of [ADR-0109](0109-use-pearl-and-smoked-plum-app-materials.md))
- **Superseded by:** None

## Context

The owner asked for icon themes with real color and personality, a complete
set of special folders and file types, and a distinct mark for every Qinda
application. The QindaQt theme drew every folder in one apricot, every file
type on the same pale page with a tiny mark, and aliased Qinda apps (Files,
Text Editor, QQ_Term) onto generic glyphs. The six QindaThemes families were
one pastel drawing set pushed through filters, with the catalog modules copied
between repositories and nothing keeping the copies in step.

## Decision

- `tools/qinda_icon_*.py` in container-wm is the single source of truth for
  the semantic catalog. QindaThemes keeps verbatim copies and verifies them
  with its `tools/sync_icon_sources.py --check`; it never edits them.
- Places, file types and Qinda application marks are authored in **true
  colors**. Every special folder shares one folder grammar and is told apart
  by its own body color *and* an emblem; every file type shares one page and
  is told apart by a colored band *and* a content glyph. Nothing is
  distinguished by color alone.
- Each Qinda application is keyed by the `Icon=` name its desktop entry
  declares and has its own silhouette and hue (Files, Text Editor and QQ_Term
  no longer alias generic glyphs). QindaQt Settings keeps `preferences-system`.
- The `preferences-*` settings glyphs carry one muted hue per subsystem.
  Pearl, smoked plum and apricot remain the identity constants pinned to the
  packaged themes; jade stays a status color and never an app color.
- `tools/qinda_icon_required.py` is the required-name floor every Qinda icon
  theme must resolve: the desktop-icon names, the places, the per-format MIME
  names and the Qinda app ids. The validator and QindaThemes' tests both read
  it; removing a name is a contract change.
- Multi-color first-party marks derive their `-symbolic` cut by outlining
  every painted shape (`qinda_icon_outline.py`), so a layered color drawing can
  never produce a painted-over fake hole under the runtime's `source-in` tint.

## Consequences

- QindaQt resolves every place and file type a file manager or the desktop
  icon surface asks for, and every Qinda launcher shows its own mark, with or
  without QindaThemes installed. The embedded Controls resource catalog grows
  accordingly (about 1000 small SVGs, compressed by rcc).
- The QindaThemes families re-tone the same true colors into their own
  palettes, so both repositories agree on names and meaning.
- A third-party application still falls through QindaQt to `hicolor`; QindaQt
  does not draw other projects' application icons.
- `tools/generate_qinda_icon_theme.py --check` and the validator run as the
  `qindaqt.qindaqt-icon-artwork-current` and `qindaqt.qindaqt-icon-artwork` tests.

## Revisit when

The owner rejects a folder or file-type color scheme, the desktop-icon
surface needs names outside the floor, or QindaThemes stops consuming the
shared catalog.
