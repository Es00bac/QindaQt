# ADR-0114: Consume exported tray menus through the shared dbusmenu client

- Status: Accepted
- Date: 2026-09-08
- Deciders: QindaQt maintainers
- Related: [ADR-0032](0032-status-notifier-exact-owner-foundation.md),
  [Status notifier tray](../shell/status-tray.md),
  [Global application menu](../shell/global-menu.md)

## Context

The StatusNotifier item client already observes `Menu` and `ItemIsMenu`, but
previously the tray presented generic actions instead of the application's
exported menu. The Global Menu module owns a bounded, exact-owner asynchronous
`com.canonical.dbusmenu` client. A second wire implementation in the tray would
duplicate validation and permit its ownership and action rules to diverge.

## Decision

The item-client transport privately links the public `GlobalMenuDbusMenu`
target. Each admitted item slot owns one narrow menu collaborator. It binds
only the item's bus unique name and validated menu object path, and uses the
existing decoder's complete hierarchical snapshot: at most 1,024 total nodes,
128 children per node and six levels, with its existing text and wire bounds.
The registry's legacy synthetic menu values and their smaller bounds remain
unchanged. No process boundary or persisted format changes.

The shared client accepts `AboutToShow(0)` for the root, retains positive-only
`Event` IDs, and exposes whether its retained snapshot is current. Layout or
property invalidation, a pending root/submenu preparation, and failed reads
make actions unavailable until an accepted complete read/preparation restores
currentness. Existing consumers may retain the last accepted display snapshot.
No wire layout or retry authority moves into the tray presentation.

The monitor validates and immediately revalidates every operation through the
registry's existing exact `OwnerKey`/identity intent gate. The menu collaborator
also requires the exact locally issued monotonic menu revision and a visible,
enabled leaf action beneath visible, enabled ancestors. Revision numbers are
never reused across path replacement or item-slot recreation. An invocation
consumes its captured revision and sends one `Event(clicked)` without retry.
Submenu preparation instead requires an enabled, visible submenu, including an
empty lazy submenu. Root preparation occurs when the menu opens.

`NewMenu` and relevant `PropertiesChanged` events invalidate the old binding
before refetching item facts. Invalid descriptors, changed identities/menu
paths, item retirement, and owner-generation changes cannot retain an
actionable old menu. New hints racing an item read coalesce into a serial-fenced
follow-up read; superseded replies are not published as current menu facts.

The injected applet source seam exposes bounded presentation values and
`openMenu`, `aboutToShowMenu`, `invokeMenu`, `scroll`, `itemIsMenu`, and
`hasExportedMenu`. Menu revision is a decimal string across QML, preserving
64-bit precision. A separate `menuChanged` notification updates menus without
rebuilding the item delegates. Application menu icons are currently absent
because the canonical shared menu value does not carry icon payloads.

When no exported menu is advertised, including Qt's `/NO_DBUSMENU` sentinel,
opening the menu retains the existing validated `ContextMenu` dispatch. An
advertised endpoint that fails to load reports failure instead of pretending
that the application exported no menu. Its next explicit open can retry.

The shared client's required UUID is opaque, process-local decoder lineage for
this binding. It is never a compositor/window identity and grants no global
menu ownership. The registry's exact owner remains the sole tray authority.

## Consequences

The transport reuses the public Global Menu protocol/decoder/client without
linking its focused-window selection, registrar, or applet. The tray controller
and QML receive no D-Bus handles or registry authority. Core registry admission,
watcher behavior, legacy intents and icon rendering retain their boundaries.

Focused private-bus tests cover root and submenu preparation, hierarchy,
checkmarks, one-shot invocation, stale and disabled/hidden action rejection,
endpoint replacement, removal, fallback and scroll. The shared client tests
cover root ID zero and currentness transitions. UI, installed-package and live
third-party qualification belong to the primary tray integration evidence.
