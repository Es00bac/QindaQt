# Status notifier tray

The status tray presents StatusNotifier items contributed by third-party
session services. Its `src/shell/status_notifier` module is a pure, Qt
Core-only source foundation — validated values, an exact-owner keyed registry,
validated request intents, and a deterministic presentation projection — plus
three production D-Bus transports that feed it: a
`org.kde.StatusNotifierWatcher` service, an asynchronous item reader with its
registry-feeding monitor, and an icon-theme/pixmap renderer. The architectural
decision is in
[ADR-0032](../adr/0032-status-notifier-exact-owner-foundation.md). The tray
applet itself still resolves as `implementation-unavailable` in the applet
runtime until its presentation slice lands.

## Ownership identity

- A tray item's owner is the source's bus **unique name** (e.g. `:1.42`): a
  colon followed by two or more nonempty dot-separated ASCII elements whose
  characters are letters, digits, underscore, or hyphen. Elements may begin
  with any of those characters, and the complete name is capped at 255 bytes.
  A well-known name is rejected as an owner because its ownership can change
  hands while items remain registered.
- Object paths accept the root path (`/`) or slash-separated valid elements
  up to 255 bytes.
- The registry keys each item by `(uniqueName, objectPath)` and a
  registry-issued owner generation allocated when the transport reports the
  name's arrival.
- Replacing the item at a live owner's exact key is the supported update path.
- One user-visible item identity (`Id`) may be claimed by only one live owner;
  a duplicate claim from a second owner is rejected without disturbing the
  first.

## Generation fencing, monotonic epochs, and bounded owner history

Generations come from one globally monotonic counter, so a generation value is
never reissued after owner loss, slot reuse, or counter-wrap refusal. Only live
owners occupy tracking slots, and the table is capped at
`kMaxTrackedOwners`; a new live owner beyond capacity fails closed instead of
growing shell memory or evicting a live owner. Every keyed event —
registration, removal, mass removal, and owner loss — must carry the owner's
current generation and the active watcher epoch, so a reply that races a
disconnect, restart, or watcher transition is rejected as stale instead of
resurrecting removed items.

Two lifecycle transitions keep presented keys safe:

- **Owner rebaseline.** A duplicate begin for a still-live name drops that
  owner's items and issues a fresh generation; no stale, unactionable key
  survives and freed identities can be claimed immediately.
- **Watcher epochs.** A watcher (re)connection begins a new monotonic watcher
  epoch, which resets the population bit; presentation returns to fail-closed
  Loading until the replacement watcher's population is observed and marked
  complete. A replacement population is staged beside the published last-known-
  good set, validates identity uniqueness and the item bound against its own
  post-prune target, then replaces both membership and the reverse identity
  index atomically on matching completion. This permits valid path/owner
  identity handover and 64-for-64 replacement without transient duplicate
  claims. A contradictory or over-capacity target cannot complete and leaves
  the last-known-good set intact. If a replacement watcher interrupts the
  never-completed first population, that provisional set has never become LKG:
  its item, identity, capacity, and degradation state are discarded before the
  replacement target admits events. Watcher *loss* is handled at the
  presentation layer (below) because a watcher departure says nothing about
  its sources' liveness.

Both monotonic counters refuse wrap. Owner-generation exhaustion leaves the
current owner and last-known-good items intact but refuses new generations.
Watcher-epoch exhaustion invalidates the active epoch and resets population
truth to Loading, so neither a replacement watcher nor traffic stamped by the
superseded watcher can mutate the registry with a reused value.

## Bounded payloads

Every payload crosses `status_notifier_limits.h` before presentation: identity
and title byte caps, icon pixmap dimension/count/byte budgets with exact
`width * height * 4` ARGB32 byte accounting and aggregate pixmap count checks
performed in place without list concatenations, tooltip text bounds, and a
flat DBusMenu-style menu of at most 128 entries whose parent chains stay within
a depth of 4. Parents must exist, be declared earlier, and be submenus; forward
references and children beneath ordinary items or separators are rejected.
Text values reject embedded NUL and C0, DEL, and C1 control characters, count
byte budgets in UTF-8 bytes, and must contain non-whitespace content whenever
they are present, so a source cannot publish blank presentation text.
`validateItemDescriptor` is the single admission gate: every descriptor
member — menu included — is validated there before the registry can make any
part of the descriptor visible. Malformed input fails closed: a malformed
replacement of a live item is rejected, marks the registry degraded, and keeps
the last-known-good descriptor presented until the degradation is acknowledged.
During watcher reconciliation the rejected payload still counts as observing
that exact live key; admission failure cannot erase its last-known-good item.

## Request intents and revalidation

Activation, context-menu, and secondary-activation requests are validated
against exact live ownership and returned as a typed `RequestIntent` bound to
the target owner key (including its current generation), the item identity
snapshot at acceptance time, and the request kind. The intent has an explicit
lifetime: it is valid only while that generation remains current and the key's
identity is unchanged, so an executor must call `revalidateIntent` immediately
before performing anything. The registry never executes an intent; the item-client monitor maps a
validated, revalidated intent onto the owner's own D-Bus object, and anything
that fails validation or revalidation is refused before any call is sent.

## Transport seam

Transport adapters reach the registry only through `StatusNotifierEventSink`,
a narrow event interface covering owner arrivals and departures, item
registration/removal, and watcher population epochs. Through that seam a
transport cannot observe items, evaluate requests, or acknowledge degradation.
Both `StatusNotifierRegistry` and `StatusNotifierEventSink` delete copy and move
authority to preserve singular ownership. The sink contract is explicit: the
sink is not owned and must outlive the attachment, attachment requires a
non-null sink and refuses re-attachment, detach is idempotent, and all sink
calls stay on the attaching thread.

The production transports implement that seam on an injected session-bus
connection (private buses in tests; the host session bus in the shell). They
own no global state and never touch hardware, the network, or the filesystem
for writes.

### Watcher service (`src/shell/status_notifier/watcher`)

`StatusNotifierWatcherService` serves `org.kde.StatusNotifierWatcher` at
`/StatusNotifierWatcher` on the injected connection:
`RegisterStatusNotifierItem`, `RegisterStatusNotifierHost`, the
`RegisteredStatusNotifierItems` / `IsStatusNotifierHostRegistered` /
`ProtocolVersion` properties, and the four protocol signals
(`StatusNotifierItemRegistered`, `StatusNotifierItemUnregistered`,
`StatusNotifierHostRegistered`, `StatusNotifierHostUnregistered`). Ownership
rules follow ADR-0032 exactly:

- Every item is keyed to the caller's bus **unique name**. A bare object-path
  argument registers against the caller; a service-name argument is resolved
  through the bus daemon and lands on the resolved owner's
  `/StatusNotifierItem` path. A well-known name can never hold an item.
- Registered items and hosts are retired only on a bus-daemon-authenticated
  `NameOwnerChanged` unique-name loss tuple. A peer-emitted signal with the
  same path, interface, member, and payload cannot retire a live owner. A
  genuine disconnect emits the matching protocol signal first, including
  `StatusNotifierHostUnregistered` for host retirement.
- `start()` never claims a name another connection owns. A foreign owner is
  not an error: the service fails closed into `NameOwnedElsewhere`, stays
  introspectable, refuses registrations, and reports a truthful degraded
  reason so the shell can present Degraded state instead of a silently broken
  watcher. `start()`/`stop()` are idempotent.

### Item client and monitor (`src/shell/status_notifier/item_client`)

`StatusNotifierItemClient` is an asynchronous reader for one
`org.kde.StatusNotifierItem` object. It fetches the full property set
(Category, Id, Title, Status, WindowId, IconName, IconPixmap, OverlayIconName,
AttentionIconName, AttentionPixmap, AttentionMovieName, ToolTip, ItemIsMenu,
Menu), decodes hostile input defensively, and validates through the foundation
admission gate. Pixmap structs are decoded by manual wire iteration rather
than registered-type demarshalling, so a hostile payload can never crash the
decoder inside libdbus. Unknown properties are ignored.
Presentation-bearing recognized properties with unexpected types fail the
descriptor closed. The recorded-only optional facts `WindowId`,
`OverlayIconName`, `ItemIsMenu`, and
`Menu` are safe-dropped on a wrong type because they cannot reach the registry
or renderer. Missing optional properties decode to defaults. Every emitted
result is tagged with the owner
generation captured at construction and fenced through an injected predicate,
so a reply racing owner loss or a watcher rebaseline is dropped instead of
resurrecting a removed item. The typed fetch status distinguishes a bounded
live-owner timeout from an immediate D-Bus transport error. New* signals
coalesce into at most one in-flight refetch. The intent calls (`activate`,
`secondaryActivate`, `contextMenu`, `scroll`) are fire-and-forget; coordinate
methods use the protocol's signed `(int, int)` signature. Callers must evaluate
and revalidate a `RequestIntent` through the registry before dispatching, and
a non-horizontal/non-vertical scroll orientation is refused without sending.
Wire-side details the value model has no slot for — `windowId`,
`overlayIconName`, `itemIsMenu`, and the DBusMenu exporter path — are recorded
in `ItemWireDetails` for later composition: the `Menu` path is **recorded but
not rendered** here; the Global Menu G1 lane delivers the shared dbusmenu
adapter and a later lane composes it into the tray.

`StatusNotifierItemMonitor` drives the registry through the event sink. It
watches the watcher name, opens a fresh epoch and re-populates whenever a
(replacement) watcher acquires it, issues one generation per owner per epoch
and shares it across that owner's object paths (including the valid root path
`/`), retires owners only on a bus-daemon-authenticated `NameOwnerChanged`
unique-name loss tuple, and subscribes to the
watcher's item registered/unregistered signals as a second retire path (the
registry refuses the duplicate as stale, so ordering is not a contract).
Population completion is fenced by the current epoch: a late reply from a dead
epoch can never mark the replacement population complete. Fetch timeouts
degrade to an empty admission while still counting the key as observed, so
completion cannot wedge on a single silent item.

### Icon renderer (`src/shell/status_notifier/icon`)

`StatusNotifierIconLocator` performs deterministic icon-theme lookup over
caller-injected theme roots (index parsing, exact-size probing, fixed
extension order) and `StatusNotifierIconRenderer` turns the result — or the
wire IconPixmap ARGB32 payload — into a bounded `QImage`. Pixmap decoding is
dimension- and byte-budget checked before any image is allocated. Every index
and icon candidate is canonicalized and must remain beneath its injected root,
so declared `../` paths and symlink escapes cannot cause outside reads. Theme
metadata is dimension-checked before decode, decoded dimensions are checked
again, and fallback requests are clamped to the shared 512-pixel ceiling. A
missing or undecodable icon falls back to that deterministic placeholder. The
module performs no network access and no filesystem writes.

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

The module's full hostile coverage is selected with:

```sh
ctest --test-dir build/dev \
  -R '^qindaqt\.status-notifier-' \
  --output-on-failure
```

Seven CTest rows run in Debug and Release. The `values`, `registry`, and
`presentation` rows cover the foundation (see below). The transport rows run
against a **private session bus** (`dbus-daemon --session` spawned by the test
fixture, never the host bus):

- `qindaqt.status-notifier-watcher`: fake items and hosts registering by bare
  object path and by service name, unique-name keying, owner-loss retirement
  of items and hosts (including the host-unregistered wire signal), protocol
  properties and signals, idempotent degraded startup, and refusal to claim a
  name another watcher owns (`NameOwnedElsewhere` with a truthful degraded
  reason).
- `qindaqt.status-notifier-item-client`: descriptor fetches over the private
  bus, New*-signal refetch coalescing, hostile payloads (oversized pixmaps,
  malformed wire shapes, wrong-typed string facts, bad tooltips, and
  unknown/missing properties), bounded strings, a live owner that withholds its
  reply through the configured typed timeout, the distinct immediate-error
  outcome, late-reply generation fencing, and signed activation intents
  recorded by a strict fake item.
- `qindaqt.status-notifier-monitor`: end-to-end registry population from a
  live watcher, simultaneous paths sharing one owner generation, last-path
  retirement followed by a new path retaining that still-live owner's
  generation, root-path population, item retirement and bounded owner-slot
  release on owner disconnect,
  rejection of a peer-forged owner-loss signal while the real owner remains
  connected, watcher-restart rebaseline into a fresh epoch (the fake item
  re-registers with the replacement watcher, as real items do), truthful Degraded
  presentation with last-known-good retention, and validated intent dispatch
  (stale generations and invalid orientations refused).
- `qindaqt.status-notifier-icon`: theme lookup over injected theme roots,
  canonical containment against hostile index directories, ARGB32 and theme
  image decode bounds, and a bounded deterministic fallback for missing or
  undecodable icons.

The values tests cover canonical unique owner-name/path/generation syntax, root
object path, in-place pixmap dimension, byte-count and aggregate budget rules,
icon/tooltip bounds, control-character rejection including C1, blank-text
rejection, flat-menu depth/parent-kind/budget rules, and the composed
descriptor gate catching hostile menus, plus source-policy precision for the
production adapter constants. The registry tests cover type traits
(non-copyable, non-movable), exact-owner keying through the narrow sink
interface, replacement and removal, well-known-name spoofing, duplicate
identity across live owners, stale replies after owner loss, generation-fenced
loss events, restart generation advance, live-owner rebaseline, globally unique
generations, bounded owner history with fail-closed exhaustion, capacity
overflow, watcher-epoch rebaseline with stale completion/arrival/registration/
removal/loss fencing, empty/partial/full membership reconciliation, atomic
same-owner and cross-owner identity handover, conflicting handover rollback in
both event orders, 64-for-64 post-prune replacement in both event orders, and
interrupted-first-population identity/capacity replacement with stale-epoch and
Loading assertions, plus both
generation- and epoch-counter exhaustion,
malformed-replacement degradation with last-known-good retention, typed
accepted intents bound to owner/generation/identity, and `revalidateIntent`
defenses against identity replacement, removal, rebase, and owner loss. The
presentation tests cover every state transition including watcher loss and
reconnect rebaseline, stable ordering, accessibility identities with injected
localized texts, and a scripted lifecycle driven through the injected fake
transport. The fake cases separately prove null-first refusal, different-sink
reattach refusal, state-clearing detach, and destructor-triggered detach.

This evidence is source, unit, and private-bus level with fake items and hosts.
It does not claim host session bus behavior, dbusmenu rendering (deferred to
the Global Menu G1 lane and a later composition lane), a rendered panel tray,
or assistive-technology bridge behavior; those belong to later milestones and
their own gates.
