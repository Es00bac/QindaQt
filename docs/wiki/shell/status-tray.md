# Status notifier tray

The status tray presents StatusNotifier items contributed by third-party
session services. Its `src/shell/status_notifier` module is a pure, Qt
Core-only source foundation: validated values, an exact-owner keyed registry,
validated request intents, and a deterministic presentation projection. It
contains no D-Bus code, owns no bus name, and never performs an item action;
the architectural decision is in
[ADR-0032](../adr/0032-status-notifier-exact-owner-foundation.md). The tray
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

## Generation fencing and bounded owner history

Generations come from one globally monotonic counter, so a generation value is
never reissued — not after owner loss, history eviction, or counter-wrap
refusal. Only live owners occupy tracking slots, and the table is capped at
`kMaxTrackedOwners`; a new live owner beyond capacity fails closed instead of
growing shell memory or evicting a live owner. Every keyed event —
registration, removal, mass removal, and owner loss — must carry the owner's
current generation, so a reply that races a disconnect or restart is rejected
as stale instead of resurrecting removed items.

Two lifecycle transitions keep presented keys safe:

- **Owner rebaseline.** A duplicate begin for a still-live name drops that
  owner's items and issues a fresh generation; no stale, unactionable key
  survives and freed identities can be claimed immediately.
- **Watcher epochs.** A watcher (re)connection calls the epoch boundary, which
  resets the population bit; presentation returns to fail-closed Loading until
  the replacement watcher's population is observed. Watcher *loss* is handled
  at the presentation layer (below) because a watcher departure says nothing
  about its sources' liveness.

## Bounded payloads

Every payload crosses `status_notifier_limits.h` before presentation: identity
and title byte caps, icon pixmap dimension/count/byte budgets with exact
`width * height * 4` ARGB32 byte accounting, tooltip text bounds, and a flat
DBusMenu-style menu of at most 128 entries whose parent chains stay within a
depth of 4. Parents must exist, be declared earlier, and be submenus; forward
references and children beneath ordinary items or separators are rejected.
Text values reject embedded NUL and C0, DEL, and C1 control characters, count
byte budgets in UTF-8 bytes, and must contain non-whitespace content whenever
they are present, so a source cannot publish blank presentation text.
`validateItemDescriptor` is the single admission gate: every descriptor
member — menu included — is validated there before the registry can make any
part of the descriptor visible. Malformed input fails closed: a malformed
replacement of a live item is rejected, marks the registry degraded, and keeps
the last-known-good descriptor presented until the degradation is acknowledged.

## Request intents

Activation, context-menu, and secondary-activation requests are validated
against exact live ownership and returned as a typed `RequestIntent` bound to
the target owner key (including its current generation), the item identity
snapshot at acceptance time, and the request kind. The intent has an explicit
lifetime: it is valid only while that generation remains current, so an
executor must revalidate immediately before performing anything. The registry
never executes an intent; mapping one onto the owner's own D-Bus object
belongs to a later presenter milestone.

## Transport seam

Transport adapters reach the registry only through `StatusNotifierEventSink`,
a narrow event interface covering owner arrivals and departures, item
registration/removal, and watcher population epochs. Through that seam a
transport cannot observe items, evaluate requests, or acknowledge degradation.
The sink contract is explicit: the sink is not owned and must outlive the
attachment, attachment requires a non-null sink and refuses re-attachment,
detach is idempotent, and all sink calls stay on the attaching thread. This
module contains no D-Bus code; the only allowed implementations today are test
fakes.

## Presentation and accessibility

`projectPresentation` is a pure projection from registry state, input, and an
injected `PresentationTexts` record: same inputs, same result, stable
owner-name item ordering. All human-readable strings come from
`PresentationTexts`, which is the localization boundary for assistive text;
the defaults are deterministic fallbacks only, and the fallback accessible
name is the locale-independent item identity. States are:

| State | Meaning |
| --- | --- |
| `Loading` | Watcher live; the current watcher epoch's item population has not been observed yet (initial start and every reconnect) |
| `Ready` | Watcher live, population observed, at least one item |
| `Empty` | Watcher live, population observed, no items |
| `Degraded` | Watcher unavailable — last-known-good items stay visible and actionable — or the registry is degraded, with a diagnostic naming the cause |

Each item carries an accessible name (title, falling back to identity),
description (tooltip title, then description), a localized status string, and
keyboard identities: Activate and context menu carry the keyboard routes from
`PresentationTexts`, and secondary activation is recorded truthfully as
pointer-only until a designed keyboard route exists.

## Verification

The module's hostile coverage is selected with:

```sh
ctest --test-dir build/dev \
  -R '^qindaqt\.status-notifier-(values|registry|presentation)$' \
  --output-on-failure
```

The values tests cover owner-name/path/generation syntax, pixmap dimension,
byte-count and budget rules, icon/tooltip bounds, control-character rejection
including C1, blank-text rejection, flat-menu depth/parent-kind/budget rules,
and the composed descriptor gate catching hostile menus. The registry tests
cover exact-owner keying through the narrow sink interface, replacement and
removal, well-known-name spoofing, duplicate identity across live owners,
stale replies after owner loss, generation-fenced loss events, restart
generation advance, live-owner rebaseline, globally unique generations,
bounded owner history with fail-closed exhaustion, capacity overflow,
watcher-epoch rebaseline, malformed-replacement degradation with
last-known-good retention, and typed accepted intents bound to
owner/generation/identity. The presentation tests cover every state
transition including watcher loss and reconnect rebaseline, stable ordering,
accessibility identities with injected localized texts, and a complete
lifecycle driven through the injected fake transport. The only allowed
transport implementations are test fakes until the exact-owner D-Bus adapter
milestone.

This evidence is source and unit level with a fake transport. It does not
claim live session bus behavior, watcher activation, a rendered panel tray, or
assistive-technology bridge behavior; those belong to later milestones and
their own gates.
