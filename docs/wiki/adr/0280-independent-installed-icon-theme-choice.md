# ADR-0280: Independently select an installed icon theme

- Status: Accepted
- Date: 2026-09-27

## Context

Color themes carried one fixed icon-theme id and the shell installed its icon
provider only at startup. Installed icon families could not be selected in
Settings, and changing an id without invalidating both locator and image caches
would leave old artwork visible.

## Decision

`appearance.iconTheme` is an additive Settings1 string with empty default meaning
**Follow theme**. The existing Appearance draft, Apply, Revert, ownership and
conflict rules govern it. Choosing a color theme does not erase this independent
preference. Settings discovers choices from installed XDG `index.theme` files;
the Themes module owns bounded read-only discovery and identifier resolution.
Indexes must remain within canonical injected icon roots; unsafe identifiers,
symlink escapes, hidden themes and oversized indexes are excluded.

A confirmed installed preference overrides the resolved color theme's icon id.
Empty or unavailable choices use that authored id, finally QindaQt for malformed
authored ids. Owner loss retains the last confirmed presentation; no speculative
write or automatic preference repair occurs. Qt's native appearance and QIcon
receive the resolved id. Shell and Settings use the public `IconRuntime::update`
seam: keep the engine-owned provider, replace its locator and bounded cache under
its request mutex, update the separate GUI-thread lookup and publish a revision
in QML image URLs. Existing delegates and task identities remain intact. A task
icon catalog change rebuilds cached row presentation once, never on row reads.

## Consequences

New installed families appear when Settings is opened; no hard-coded family list
is needed. Theme removal falls back on the next confirmed appearance update or
application launch, without overwriting the saved preference. This does not
recolor application-supplied bitmap icons or replace a tray item's supplied
pixmap. Focused tests cover confinement, unavailable/hostile fallback, confirmed
native changes and owner loss, and changed provider pixels with the same existing
QML image object and no duplicate invalidation for an unchanged selection.
