# ADR-0160: Select installed KWin window decorations explicitly

- **Status:** Accepted
- **Date:** 2026-09-13
- **Owners:** Settings Appearance, Platform integration
- **Supersedes:** None
- **Superseded by:** None

## Context

Appearance previously presented QindaQt theme and button controls as though
they described every application window. That was untrue when KWin was using a
different native KDecoration plugin or an Aurorae theme: the route always
painted a QindaQt preview, offered no installed-decoration catalog, and did not
change KWin's actual decoration selection. Theme-schema decoration presets and
the Settings1 window-button preferences remain useful, but only while the
QindaQt KDecoration plugin is active.

KWin already owns this separate session preference in
`[org.kde.kdecoration2]` in `kwinrc`. Aurorae packages are data directories in
the standard XDG data roots, and native KDecoration plugins are Qt plugins.
Importing either implementation into Settings would cross their rendering and
ownership boundaries.

## Decision

The Appearance **Windows** destination inventories installed native QindaQt
and Breeze decorations plus valid Aurorae theme directories from the standard
XDG data roots. Each choice has a stable identity distinct from the QindaQt
appearance-theme identity. An explicit **Use decoration** action writes only
KWin's `library` and, for Aurorae, `theme` keys, preserves every unrelated key,
and completes only after a bounded `org.kde.KWin.reconfigure` D-Bus call
returns without an error.

Settings does not invent a foreign decoration preview. When QindaQt is the
selected decoration, the shared QindaQt painter and its Settings1 controls
remain visible. For any other selected decoration, the route explains that
KWin owns the rendering, hides the inapplicable QindaQt preview and controls,
and uses the Settings window's real post-apply frame as the live preview.
Container chrome remains independently previewed and configured through
Settings1 because it is rendered by the QindaQt compositor rather than by the
selected KDecoration plugin.

## Consequences

- Installed Aurorae themes become directly selectable without pretending they
  are QindaQt appearance themes.
- The current external decoration is represented even when its package is no
  longer discoverable, but an unavailable entry cannot be applied.
- A saved configuration whose live reload fails is reported as saved but not
  applied; Settings does not claim success from merely queueing a D-Bus
  message.
- Discovery and persistence stay behind a small platform adapter. QML receives
  validated rows and explicit select/apply methods, not filesystem or D-Bus
  access.
- QindaQt theme decoration presets described by
  [ADR-0159](0159-author-distinct-decoration-presets-for-every-builtin-theme.md)
  apply only when the QindaQt KDecoration plugin is active.

## Revisit when

KWin publishes a stable public decoration catalog and preview API that covers
both native and Aurorae decorations, or QindaQt gains a separately packaged
decoration format with an equivalent platform-owned selection boundary.
