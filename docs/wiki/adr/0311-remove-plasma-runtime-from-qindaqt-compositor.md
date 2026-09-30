# ADR-0311: Remove Plasma runtime dependencies from the QindaQt compositor

- **Status:** Accepted
- **Date:** 2026-09-29
- **Owners:** Compositor and Platform
- **Supersedes:** [ADR-0309](0309-build-qindaqt-without-plasma-activities.md)
- **Superseded by:** None

## Context

The exact QindaQt fork owns its decoration, tab switcher, outline, effect frames,
notifications, and output locator. Its production build still discovered Plasma
Activities, libplasma, Breeze, and Aurorae, and several installed QML surfaces
imported Plasma components or depended on Plasma SVG assets. This made the
production runtime depend on a desktop service and presentation library that
QindaQt does not ship. The binary boundary and its compiled defaults are
specified in [Compositor and session integration](../architecture/compositor-session.md);
the tab switcher also consumes the native KWin model described in
[Task switching](../adr/0089-present-task-switching-through-kwins-native-model.md).

## Decision

The production qindaqt-kwin fork does not discover Plasma Activities, libplasma,
Breeze, or Aurorae. Its Activities build and exported consumer capability are
fixed false; activity-neutral workspace and output APIs remain available.
Classic Plasma overview, window-view, desktop-change OSD, tile-editor, and
bundled private-effect presentation units are excluded from the production
build. QindaQt-owned outline, notification, and effect-frame surfaces use QtQuick
and QindaQt styling. The Plasma-backed output locator, bundled thumbnail-grid
switcher, and classic presentation effects are excluded; the required QindaQt
switcher remains the only production tabbox layout. The QindaQt
window decoration remains the compiled default and fallback. Enabling KWin's
decoration build switch does not re-enable classic decoration backends.

## Consequences

A production configure can disable discovery of every retired package and
build the compositor and QindaQt consumer without those packages. Existing
activity-neutral sentinels, workspace/output scope, window decoration,
switcher model, and QindaQt switcher selection behavior stay stable. A missing
QindaQt switcher package is reported through KWin's existing broken-resource path. The fork retains
its separate Wayland protocol code-generation and client dependencies; they are
not libplasma. Optional KCM sources and upstream-only implementation files may
remain in the source tree but are not built or installed in the production
configuration.

The fork and consumer must qualify the exact staged ABI together, inspect the
compositor's transitive runtime dependencies and installed QML imports, and run
the focused decoration/switcher and neighboring workspace tests. A future
reintroduction of a Plasma-backed presentation surface requires a new reviewed
decision and an explicit production dependency change.

## Revisit when

Reconsider only if QindaQt adopts a supported native replacement contract that
needs one of these retired libraries or explicitly decides to ship a Plasma
service as part of the compositor session.
