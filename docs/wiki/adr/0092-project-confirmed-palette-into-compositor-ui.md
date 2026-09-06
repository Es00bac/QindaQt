# ADR-0092: Project the confirmed palette into QindaQt compositor UI

- Status: Accepted
- Date: 2026-09-06

## Context

KWin hosts the bundled task switcher and native context menus. Its ambient KDE
palette can differ from the theme confirmed through QindaQt Settings1, leaving
those QindaQt-owned surfaces bright while the session chrome is dark. Changing
KWin's application palette would also recolor unrelated third-party UI.

## Decision

The compositor appearance owner derives one native `QPalette` and one semantic
QML map from its retained validated theme. It applies the native palette only
to QindaQt-created menus. It exposes the map as a read-only, process-local QML
singleton for the bundled switcher; the compositor plugin owns the singleton
for its process lifetime and emits a change notification after each confirmed
theme update. The switcher's root loads the hard import through an isolated
bridge component. A missing module makes only that loader fail, and the root
supplies an empty map so every role falls back to the ambient Kirigami palette.

System color-scheme preference keeps an installed requested theme. Explicit
Light and Dark preferences remain the only inputs that select compatible theme
variants.

## Consequences

QindaQt menus and the bundled switcher share the confirmed session colors and
update without restarting KWin. Other KWin UI keeps its own palette. The QML
surface receives colors only; settings transport, theme selection, and mutable
appearance authority remain in the compositor appearance owner.

If the compositor plugin is unavailable, the QindaQt switcher still loads and
uses its ambient Kirigami roles. When the bridge is available, its live binding
delivers later confirmed changes without recreating the switcher.
