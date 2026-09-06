# ADR-0078: Own wallpaper surfaces in the shell

- Status: Accepted
- Date: 2026-09-05

## Context

Settings1 already persists `appearance.wallpaper` and `appearance.wallpaperMode`, but no production component applied them. A desktop identity needs one background on every live output, including after hotplug, without giving Settings UI or the compositor a second preference authority.

## Decision

The production shell owns one noninteractive layer-shell Background window per `QScreen`. A focused controller reconciles that set and consumes only confirmed values from the shell's existing scoped Settings1 client. `scaled`, `centered`, and `tiled` map to crop, pad, and tile rendering. Invalid, unreadable, or unavailable sources render the mineral ink fallback color.

Bundled values use `qindaqt:<name>` identities and resolve only beneath standard generic-data roots at `qindaqt/wallpapers/<name>.png`; absolute paths remain supported for explicit user choices. The default layer selects `qindaqt:jade-fold`. User-layer values continue to outrank it, including the empty string, so startup never rewrites a saved choice.

## Consequences

Output and Settings changes update backgrounds without restarting the shell. The controller owns no persistence and reserves no work area or input. Packaging must install wallpapers in both the shell and Appearance Settings runtime components.

This supersedes ADR-0074's deferral of wallpaper application; its Settings ownership and fail-closed snapshot rules remain unchanged.
