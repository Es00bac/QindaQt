# ADR-0228: Bundled wallpapers resolve beyond PNG

- **Status:** Accepted
- **Date:** 2026-09-21
- **Owners:** Appearance Settings route and shell runtime
- **Supersedes:** [ADR-0078](0078-own-wallpaper-surfaces-in-the-shell.md) (bundled-identity resolution point only)
- **Superseded by:** None

## Context

[ADR-0078](0078-own-wallpaper-surfaces-in-the-shell.md) gave the shell one
background window per output fed by the `appearance.wallpaper` Settings1 key,
with bundled values carried as `qindaqt:<name>` identities resolved beneath
standard generic-data roots at `qindaqt/wallpapers/<name>.png`. The Appearance
route's chooser discovers the same directory for its bundled grid.

Two ends of that contract were PNG-only in code: the route catalog globbed
`*.png`, and the shell resolver appended a literal `.png` to the identity. A
`.jpg` or `.webp` dropped into the directory was invisible in the chooser and
unresolvable by the shell — latent while only PNGs ship, but a silent trap for
anyone adding artwork. The chooser's file dialog already offered those
formats for custom paths, so the bundled-directory restriction was an
accident, not a policy.

## Decision

A bundled identity `qindaqt:<name>` resolves beneath each generic-data root at
`qindaqt/wallpapers/<name>.<ext>` for `<ext>` in `png`, `jpg`, `jpeg`, `webp`,
`bmp` — the same formats the chooser's file dialog accepts. Both resolution
ends try the formats in that fixed priority order: the Appearance route's
`discoverBundledWallpapers()` and the shell's `resolveWallpaperSource()` name
the same file for one identity, so what the chooser previews is what the
desktop paints. Earlier roots still win across roots; within one root and one
basename, the priority format wins. Everything else about ADR-0078 stands:
ownership, per-output surfaces, fallback color, `qindaqt:` name rules (no
empty names, no `/`), and absolute-path support for explicit user choices.

## Consequences

Non-PNG artwork placed beside the bundled wallpapers appears in the chooser
and resolves on the desktop with no further change. Duplicate-basename
directories are deterministic instead of depending on glob order. The two
implementations stay mirrored with a named cross-reference; a fifth format
must be added to both in the same change. Focused rows cover format
resolution on both ends (`qindaqt.appearance-values`,
`qindaqt.shell-preference-values`).

## Revisit when

The bundled identity scheme gains per-screen or per-theme variants, or the
two mirrored resolvers move into one shared module boundary.
