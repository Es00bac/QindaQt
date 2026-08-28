# Global application menu

The global menu applet presents the focused window's application menu in the
panel. QindaQt builds it on a bounded, toolkit-neutral canonical menu/action
model with a proof-bound authenticated ownership policy, not on the
desktop-agnostic `com.canonical.AppMenu.Registrar` trust model. The durable
choices are in
[ADR-0033](../adr/0033-canonical-menu-model-and-authenticated-menu-ownership.md).

## Milestone boundary

The G0 slice delivers the source/static foundation only: pure model, policy,
exporter, Qt Widgets adapter, and applet facade, each with focused hostile
tests. There is no D-Bus transport, no registrar, no shell-runtime
instantiation, and no applet-registry wiring yet; the `global-menu` manifest
still resolves as `implementation-unavailable` (see
[Applet runtime](applet-runtime.md)), and the QML component is not yet part
of an installed QML module — the test imports the source tree directly. No
one should read this page as a live feature claim. Wiring the transport,
registry entry, installed packaging, and panel instance is the next
milestone and requires compiler- and session-verified evidence.

## Canonical model

`QindaQt::Shell::GlobalMenu::Protocol` (module `QindaQt::GlobalMenuProtocol`)
owns the only menu representation that crosses any boundary. `MenuItem` is a
bounded value with kind `Action`, `Separator`, or `Submenu`; `MenuTree` adds
the owner/epoch/revision lineage used by Display1, Audio1, and Settings1.

- Text, ids, shortcut text, and radio-group names have fixed UTF-8 byte
  ceilings (menu_limits.h); a tree that exceeds any bound is invalid as a
  whole.
- Text must be well-formed Unicode: embedded NULs and isolated (unpaired)
  UTF-16 surrogate code units are rejected, because they are not
  representable scalar values.
- `text` never contains a toolkit mnemonic character. The mnemonic position is
  `mnemonicIndex`, a UTF-16 offset into `text`, or -1. Toolkit escaping
  ('&', '_', ...) exists only inside adapters.
- Ids are unique across a tree and stable across snapshots; they are the
  only trusted invocation key.
- Separators carry no content, actions have no children, `checked` requires
  `checkable`, and a radio group admits at most one checked member per
  parent. A kind outside the three known values rejects the node — its
  children are never traversed.
- `validateMenuTree` rejects hostile input as a whole rather than admitting a
  partial tree, matching every other QindaQt wire model.
- G0 keeps snapshot-only truth: a full-tree delta contract (payload-bearing
  operations with safe application order) is deferred to the transport
  milestone, where it will be designed and proven with an
  apply-to-next-tree test before any consumer exists.

## One authoritative lineage

The ownership selector is the single lineage authority. It mints the
`epoch` (only when the owning window identity changes) and advances the
`revision` (on every adoption within one epoch); the exporter stamps
exactly that epoch/revision into each accepted tree through an injected
`ExportLineageSource` seam, and shell composition backs that seam with the
selector. Owner, epoch, revision, and the invocation guard's expectations
therefore share one source of truth, and an ordinary public-API flow —
authenticate, adopt the returned proof, export, invoke — is coherent by
construction. The exporter never mints lineage itself; a pull whose owner
has no current authority is rejected without publishing. Publication
enforces the lineage/content binding at the accepted-publication authority:
within one epoch, changed content is accepted only when the revision
strictly advances, and regressed revisions or null epochs are rejected
outright as `RejectedStaleLineage` with the last accepted tree retained, so
a replay that re-pushes changed content under a consumer-observed revision
cannot become truth.

## Proof-bound authentication

`QindaQt::Shell::GlobalMenu::Ownership` (module `QindaQt::GlobalMenuOwnership`)
decides which provider may become authoritative:

- The active-window seam reports compositor-authenticated observations
  carrying a monotonic `focusGeneration` that changes on every focus move.
- Authentication reads focus, performs the bus-daemon credential lookup,
  then re-reads focus: both observations must agree on window and focus
  generation, or the attempt fails with `focus-changed`. This closes the
  sample-lookup race where focus moves mid-check.
- The registering peer must present a syntactically valid D-Bus unique name
  (the `:1.42` shape: leading colon, dot-separated `[A-Za-z0-9_]` elements,
  bounded length). Well-known names are refused at the ownership boundary
  even when the credential seam resolves them, because a well-known name can
  be re-owned later and silently change who a proof names.
- An accepted authentication returns an `AuthenticatedProvider` proof — an
  opaque, non-aggregate capability whose constructor only
  `ProviderAuthenticator` can call. It carries exactly the verified window
  identity, unique name, and focus generation. `ActiveProviderSelector::
  adopt` accepts only this proof, so the verified identity and the adopted
  identity cannot diverge and no caller can mint accepted ownership.
- `applyFocusGeneration` is the fail-closed invalidation seam: a generation
  other than the adopted proof's drops the adoption. Shell composition
  calls it on every observed focus change before any export or invocation.
- Only the currently active window can register in G0. Unlike
  `com.canonical.AppMenu.Registrar`, a provider cannot name an arbitrary
  window id; a per-window registration cache is a later, separately
  reviewed milestone.
- `InvocationGuard` requires the request's (windowId, epoch, revision), the
  presented tree's lineage, and the selector's current lineage to agree
  exactly; any mismatch is `stale-owner` before any action lookup. It then
  rejects unknown ids, non-actions, and disabled/invisible items.

## Export lifecycle

`QindaQt::Shell::GlobalMenu::Exporter` (module `QindaQt::GlobalMenuExporter`)
pulls `MenuSnapshot` values through the toolkit-neutral `MenuSource`
interface. A snapshot carries an explicit completeness verdict: when a
source detects overflow (depth, siblings, or total items), a submenu cycle,
or the loss of its observed widget, it marks the snapshot incomplete with a
stable defect code (`too-deep`, `too-many-children`, `too-many-items`,
`submenu-cycle`, `source-destroyed`) and the exporter rejects it whole — a
bounded prefix or an authoritative empty menu is never published. A
destroyed source is a lifetime defect, not an empty application menu. Valid
snapshots are validated against the canonical bounds, stamped with the
authoritative lineage, and stored; a rejected or incomplete pull keeps the
last accepted tree, so a transiently malformed source can never regress a
previously good menu. Content that is identical under a re-advanced lineage
reports `Unchanged` but is re-stamped, so the published tree never drifts
stale against the selector.

## Qt Widgets adapter

`QMenuBarMenuSource` (module `QindaQt::GlobalMenuQtWidgetsAdapter`) walks a
real `QMenuBar`/`QMenu`/`QAction` tree into the canonical model. This is the
exact shape the integrated Text Editor exposes per
[ADR-0022](../adr/0022-keep-text-documents-local-and-atomic.md): persistent
`QAction` object names become ids, '&' mnemonics split into display text plus
offset, exclusive `QActionGroup` membership becomes a radio group, and
`QKeySequence` text is carried as bounded shortcut text. Visibility is
carried verbatim so presentation can omit hidden entries honestly. It is the
only target in `global_menu` that links `Qt6::Widgets`; the QtQuick shell
never gains that dependency. Apps that want stable ids across menu edits
must set persistent object names; the positional fallback is only stable
while sibling structure does not change. The observed widget tree must
outlive the source, snapshots must run on the Qt GUI thread, and menus must
not be mutated during a snapshot; violations degrade to an incomplete
snapshot, never to wrong complete truth.

## Applet facade and presentation

`GlobalMenuAppletAccess` mirrors
`NotificationCenterAppletAccess`: shell composition publishes authoritative
state, and QML only reads the top-level projection and requests an
activation. The projection is honest by construction: entries carry their
`kind` ("action" or "submenu") and their checked state, hidden items and
separators are omitted, and `activate()` opens only enabled visible
actions — top-level submenus render visibly but non-activating until the
popup milestone, with their accessible name saying so. `publishTree` is
fail-closed: invalid input publishes the unavailable state instead of any
part of its content. The facade is GUI-thread-confined, and its G1 consumer
must capture the observed window/epoch/revision at request time and run
`InvocationGuard` on that captured lineage before executing anything;
looking up a "current" tree by id at execution time would recreate the
request/content race the guard exists to close.
`GlobalMenuApplet.qml` renders entries as focusable, checkable
`AbstractButton` delegates that bind `checkable`/`checked` into the button
and the accessible state, share one named activation path between pointer
click, keyboard (Space/Return), and assistive-technology press, and lay
entries out in a `Row` or a real `Column` for vertical panels. Overflow is
bounded and geometry-aware: a clamped `maximumVisibleEntries` cap combined
with the assigned width (horizontal) or height (vertical) determines the
presented entries, and the muted "+N" indicator — exposed to assistive
technology as "N more menu entries" — is part of the implicit geometry in
both orientations so a constrained panel can never clip the affordance
away. G0 wires no live publisher anywhere in the shell, so `available`
stays false in production.

## Non-goals

- No general application framework, foreign-application injection, or
  arbitrary command execution: the model carries menu values, never launch
  payloads, and invocation authorization never executes anything itself.
- No private KWin/KDE ABI: compositor integration arrives later through an
  authenticated public seam, mirroring the session-lock observer pattern.
- No per-window registration, D-Bus transport, legacy-protocol
  compatibility, submenu popups, or payload-bearing menu deltas in this
  milestone.

## Verification

Focused gates: `qindaqt.global-menu-protocol`,
`qindaqt.global-menu-ownership`, `qindaqt.global-menu-exporter`,
`qindaqt.global-menu-qt-widgets-adapter` (offscreen),
`qindaqt.global-menu-applet-access`,
`qindaqt.global-menu-composition` (authenticate → adopt → export → invoke
over the public seams), and `qindaqt.global-menu-applet-qml-offscreen`
(keyboard, vertical, overflow, and submenu-honesty cases). Live export,
focus handoff, and installed-session qualification remain unbuilt and
unclaimed until the transport milestone passes the nested-session matrix in
the [testing harness](../development/testing-harness.md).
