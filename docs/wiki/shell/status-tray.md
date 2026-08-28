# Status notifier tray

The status tray presents StatusNotifier items contributed by third-party
session services. Its `src/shell/status_notifier` module is a pure, Qt
Core-only source foundation: validated values, an exact-owner keyed registry,
validated request intents, and a deterministic presentation projection. It
contains no D-Bus code, owns no bus name, and never performs an item action;
the architectural decision is in
[ADR-0026](../adr/0026-status-notifier-exact-owner-foundation.md). The tray
applet itself still resolves as `implementation-unavailable` in the applet
runtime until its presentation slice lands.

## Ownership identity

- A tray item's owner is the source's bus **unique name** (`:1.42`); a
  well-known name is rejected as an owner because its ownership can change
  hands while items remain registered.
- The registry keys each item by `(uniqueName, objectPath)` and a
  registry-issued owner generation allocated when the transport reports the
  name's arrival.
- Replacing the item at a live owner's exact key is the supported update path.
- One user-visible item identity (`Id`) may be claimed by only one live owner;
  a duplicate claim from a second owner is rejected without disturbing the
  first.

## Generation fencing

When the transport reports an owner's departure, the registry removes that
owner's items and retains the last allocated generation purely for fencing. Any
keyed event — registration, removal, or request intent — stamped with a
non-current generation or a departed owner is rejected as stale. A reply that
races a disconnect, and a source that restarts under the same unique-name
string, therefore cannot resurrect removed items. Transport implementations
must report owner departure for every name they observe; without that report
the registry cannot fence stale replies.

## Bounded payloads

Every payload crosses `status_notifier_limits.h` before presentation: identity
and title byte caps, icon pixmap dimension/count/byte budgets with exact
`width * height * 4` ARGB32 byte accounting, tooltip text bounds, and a flat
DBusMenu-style menu of at most 128 entries whose parent chains stay within a
depth of 4 with forward references rejected. Text values reject embedded NUL
and control characters; byte budgets count UTF-8 bytes. Malformed input fails
closed: a malformed replacement of a live item is rejected, marks the registry
degraded, and keeps the last-known-good descriptor presented until the
degradation is acknowledged.

## Request intents

Activation, context-menu, and secondary-activation requests are value objects
validated against exact live ownership. The registry answers whether the named
owner currently presents the targeted item; it never executes anything. A later
presenter maps an accepted intent onto that owner's own D-Bus object.

## Presentation and accessibility

`projectPresentation` is a pure projection from registry state: same state, same
result, stable owner-name item ordering. States are:

| State | Meaning |
| --- | --- |
| `Loading` | Watcher live; the initial item population has not been observed yet |
| `Ready` | Watcher live and at least one item is registered |
| `Empty` | Watcher live, initial population observed, no items |
| `Degraded` | Watcher unavailable, or the registry is degraded; last-known-good items stay visible with a diagnostic |

Each item carries accessible name (title, falling back to identity),
description (tooltip title, then description), a status string, and keyboard
identities: Activate bound to Enter/Space, context menu to Shift+F10 or the
Menu key, and secondary activation recorded truthfully as pointer-only until a
designed keyboard route exists.

## Verification

The module's hostile coverage is selected with:

```sh
ctest --test-dir build/dev \
  -R '^qindaqt\.status-notifier-(values|registry|presentation)$' \
  --output-on-failure
```

The values tests cover owner-name/path/generation syntax, pixmap dimension,
byte-count and budget rules, icon/tooltip bounds and control-character
rejection, flat-menu depth/parent/cycle/budget rules, and unknown category or
status rejection. The registry tests cover exact-owner keying, replacement and
removal, well-known-name spoofing, duplicate identity across live owners, stale
replies after owner loss, restart generation advance, generation-fenced
removal, capacity overflow, and malformed-replacement degradation with
last-known-good retention. The presentation tests cover every state
transition, stable ordering, accessibility identities, and a complete lifecycle
driven through the injected fake transport. The only allowed transport
implementations are test fakes until the exact-owner D-Bus adapter milestone.

This evidence is source/static and unit-level. It does not claim live session
bus behavior, watcher activation, a rendered panel tray, or assistive-technology
bridge behavior; those belong to later milestones and their own gates.
