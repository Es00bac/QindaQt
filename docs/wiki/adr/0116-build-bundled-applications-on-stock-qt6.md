# ADR-0116: Build bundled applications on stock Qt 6

- **Status:** Accepted
- **Date:** 2026-09-09
- **Owners:** First-party applications
- **Supersedes:** the bundled-application scope of [ADR-0013](0013-own-qst1-semantic-tokens.md), [ADR-0027](0027-extract-a-narrow-first-party-application-shell.md), [ADR-0080](0080-resolve-first-party-appearance-from-settings.md) (application-surface clauses), and [ADR-0109](0109-use-pearl-and-smoked-plum-app-materials.md)
- **Superseded by:** None

## Context

QindaQt built a custom presentation stack for first-party surfaces: QST-1
semantic tokens ([design tokens](../architecture/design-tokens.md)), the
compiled `QindaQt.Controls 1.0` QML module, per-app token→palette adapters for
the Qt Widgets apps, and Pearl/Smoked Plum application materials. The product
owner reviewed the result and decided the custom token UI was mostly a mistake
for ordinary applications: it duplicates what Qt already does well, costs a
bespoke qualification harness (visual baselines, per-app palette projections),
and makes the bundled apps harder to change than their MATE/GNOME/KDE/XFCE
counterparts.

[ADR-0115](0115-share-appearance-through-qt-platform-theme.md) already landed
the generic mechanism this decision needs: the `qindaqt` Qt platform theme
pushes the confirmed palette, interface/monospace fonts, icon theme, and
light/dark/contrast hints to **any** standard Qt consumer, with live updates,
while `QT_QUICK_CONTROLS_STYLE=Fusion` gives QML stock controls the same
palette. Ordinary Qt 6 therefore covers the bundled apps without any per-app
styling layer.

System surfaces are a different product: the shell, panels, Settings,
Settings Center, and Welcome keep QST-1/`QindaQt.Controls` as their
presentation vocabulary; nothing in this decision weakens their contracts or
their qualification harness.

## Decision

The bundled applications — Calendar, File Manager, Terminal, and Text Editor —
are built on stock Qt 6:

- Qt Widgets applications (Terminal, Text Editor) carry no token→palette
  projection and no application QSS chrome. Their appearance comes from the
  Qt platform theme and the Fusion style. Content semantics that are not
  chrome — the terminal's per-scheme ANSI protocol palettes (ADR-0112) and the
  editor's syntax-highlight contrast guard — remain, derived from the active
  palette rather than from QST.
- Qt Quick applications (Calendar, File Manager) import stock
  `QtQuick.Controls` and layouts. They do not import `QindaQt.Tokens` or
  `QindaQt.Controls`, and they contain no palette hex literals; the platform
  theme supplies the palette.
- The non-visual application seams are unchanged: the AppShell action
  catalog/coordinator, the fail-closed first-party global-menu export,
  Settings1 persistence, and atomic-save/mutation boundaries remain the owning
  contracts. This decision replaces presentation only.
- QST-1, `QindaQt.Controls`, the theme catalog, and their qualification gates
  remain normative for shell, panel, Settings, Settings Center, and Welcome
  surfaces.

## Consequences

- New bundled-application UI is written against stock Qt 6 APIs; the
  `coding-practices.md` QST rule and the `module-boundaries.md` application
  rows are re-scoped accordingly.
- The `QindaQt.Controls` static gate and visual baselines no longer apply to
  bundled-application QML; application visual qualification uses the existing
  native/offscreen capture rows instead.
- The deferred native-chrome experiment (`f6115476`) is superseded in intent:
  its direction is now policy, executed fresh per application.
- Application wiki pages state appearance comes from the platform theme; no
  page may document a per-application token or palette authority for these
  apps.
- Shell and Settings changes are out of scope for this decision and keep the
  prior ADRs in full.

## Revisit when

Reconsider if the platform theme demonstrably cannot express a required
application appearance through standard Qt APIs, if a bundled app is adopted
as a system surface, or if Qt's stock controls gain a measurable accessibility
or Wayland defect that the custom layer verifiably avoids.
