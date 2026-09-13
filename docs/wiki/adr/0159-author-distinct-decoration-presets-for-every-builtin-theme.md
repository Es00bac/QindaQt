# ADR-0159: Author distinct decoration presets for every built-in theme

- **Status:** Accepted
- **Date:** 2026-09-13
- **Owners:** Themes, Decorations, Settings Appearance
- **Supersedes:** ADR-0129's unauthored-theme fallback as the built-in default
- **Superseded by:** None

## Context

The theme schema allowed decoration blocks, but four of the six built-in themes
omitted them. Those themes therefore resolved to the same compatibility
arrangement, making Appearance look like it offered one window-manager theme.
The theme-card `Flow` also contributed zero height to its parent layout, so
cards beyond the first could be clipped even though the model contained them.

## Decision

Every built-in theme authors its window-decoration block. The catalog must
retain multiple genuinely distinct arrangement families across button side,
button style, tab direction, hover behavior, and authored colors; a test
requires at least four families among the six current themes. The Appearance
theme-card flow publishes its laid-out children height to the surrounding
layout, so every installed theme remains visible and keyboard reachable.

The schema's unauthored compatibility fallback remains valid for external and
older theme packages. It is no longer the normal state of a built-in theme.
Explicit Appearance overrides still apply on top of the selected preset.

## Consequences

- Selecting a built-in theme changes real KDecoration and container chrome,
  not just preview colors.
- The six shipped cards remain visible in a scrollable Appearance page.
- Theme tests reject a built-in catalog that collapses back to one decoration
  family or omits an authored decoration.

## Revisit when

Decoration presets become independently installable packages with their own
identity and selection key instead of remaining part of theme data.
