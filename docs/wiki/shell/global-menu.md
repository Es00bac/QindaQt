# Global application menu

The global menu applet presents the focused window's application menu in the
panel. QindaQt builds it on a bounded, toolkit-neutral canonical menu/action
model with an authenticated ownership policy, not on the desktop-agnostic
`com.canonical.AppMenu.Registrar` trust model. The durable choices are in
[ADR-0026](../adr/0026-canonical-menu-model-and-authenticated-menu-ownership.md).

## Milestone boundary

The G0 slice delivers the source/static foundation only: pure model, policy,
exporter, Qt Widgets adapter, and applet facade, each with focused hostile
tests. There is no D-Bus transport, no registrar, no shell-runtime
instantiation, and no applet-registry wiring yet; the `global-menu` manifest
still resolves as `implementation-unavailable` (see
[Applet runtime](applet-runtime.md)). No one should read this page as a live
feature claim. Wiring the transport, registry entry, and panel instance is the
next milestone and requires compiler- and session-verified evidence.

## Canonical model

`QindaQt::Shell::GlobalMenu::Protocol` (module `QindaQt::GlobalMenuProtocol`)
owns the only menu representation that crosses any boundary. `MenuItem` is a
bounded value with kind `Action`, `Separator`, or `Submenu`; `MenuTree` adds
the owner/epoch/revision lineage used by Display1, Audio1, and Settings1.

- Text, ids, shortcut text, and radio-group names have fixed UTF-8 byte
  ceilings (menu_limits.h); a tree that exceeds any bound is invalid as a
  whole.
- `text` never contains a toolkit mnemonic character. The mnemonic position is
  `mnemonicIndex`, a UTF-16 offset into `text`, or -1. Toolkit escaping
  ('&', '_', ...) exists only inside adapters.
- Ids are unique across a tree and stable across snapshots; they are the join
  key for deltas and the only trusted invocation key.
- Separators carry no content, actions have no children, `checked` requires
  `checkable`, and a radio group admits at most one checked member per parent.
- `validateMenuTree` rejects hostile input as a whole rather than admitting a
  partial tree, matching every other QindaQt wire model.
- `computeMenuTreeDelta` produces deterministic Removed → Inserted → Updated
  operations keyed by id, so a consumer that applies them in order never sees
  a dangling parent.

## Authenticated ownership

`QindaQt::Shell::GlobalMenu::Ownership` (module `QindaQt::GlobalMenuOwnership`)
decides which provider may publish and which invocation may proceed:

- A registration is accepted only when an injected, compositor-authenticated
  active-window source names that exact window and a bus-daemon credential
  lookup confirms the registering peer's real OS process equals both the
  claimed process and the active window's process. Both facts come from
  injected seams; G0 ships no real adapter and trusts no caller claim.
- Only the currently active window can register in G0. Unlike
  `com.canonical.AppMenu.Registrar`, a provider cannot name an arbitrary
  window id; a per-window registration cache is a later, separately reviewed
  milestone.
- `ActiveProviderSelector` assigns the lineage: the epoch changes only when
  the owning window identity changes; the revision advances on every
  adoption within one epoch.
- `InvocationGuard` rejects a request whose (windowId, epoch) — or whose
  presented tree — no longer matches the current lineage as `stale-owner`
  before any action lookup, then rejects unknown ids, non-actions, and
  disabled/invisible items.

## Export lifecycle

`QindaQt::Shell::GlobalMenu::Exporter` (module `QindaQt::GlobalMenuExporter`)
pulls snapshots through the toolkit-neutral `MenuSource` interface. The
exporter — never the source — assigns epoch/revision lineage, validates every
snapshot against the canonical bounds, and fails closed: a rejected pull keeps
the last accepted tree, so a transiently malformed source can never regress a
previously good menu. Identical content under one owner is `Unchanged` and
does not advance the revision.

## Qt Widgets adapter

`QMenuBarMenuSource` (module `QindaQt::GlobalMenuQtWidgetsAdapter`) walks a
real `QMenuBar`/`QMenu`/`QAction` tree into the canonical model. This is the
exact shape the integrated Text Editor exposes per
[ADR-0022](../adr/0022-keep-text-documents-local-and-atomic.md): persistent
`QAction` object names become ids, '&' mnemonics split into display text plus
offset, exclusive `QActionGroup` membership becomes a radio group, and
`QKeySequence` text is carried as bounded shortcut text. It is the only
target in `global_menu` that links `Qt6::Widgets`; the QtQuick shell never
gains that dependency. Apps that want delta-stable ids across menu edits must
set persistent object names; the positional fallback is only stable while
sibling structure does not change.

## Applet facade and presentation

`GlobalMenuAppletAccess` mirrors
`NotificationCenterAppletAccess`: shell composition publishes authoritative
state, and QML only reads the top-level projection and requests an activation.
Every activation request is re-checked against the published tree inside the
facade, and `publishTree` is fail-closed — invalid input publishes the
unavailable state instead of any part of its content. `GlobalMenuApplet.qml`
renders the same unavailable placeholder as an unprovisioned notification
center when nothing is published, exposes enabled top-level entries with
accessible names and roles, and forwards clicks as id-based activation
requests only. G0 wires no live publisher anywhere in the shell, so
`available` stays false in production.

## Non-goals

- No general application framework, foreign-application injection, or
  arbitrary command execution: the model carries menu values, never launch
  payloads, and invocation authorization never executes anything itself.
- No private KWin/KDE ABI: compositor integration arrives later through an
  authenticated public seam, mirroring the session-lock observer pattern.
- No per-window registration, D-Bus transport, legacy-protocol
  compatibility, or submenu-expanding panel UI in this milestone.

## Verification

Focused gates: `qindaqt.global-menu-protocol`,
`qindaqt.global-menu-ownership`, `qindaqt.global-menu-exporter`,
`qindaqt.global-menu-qt-widgets-adapter` (offscreen),
`qindaqt.global-menu-applet-access`, and
`qindaqt.global-menu-applet-qml-offscreen`. Live export, focus handoff, and
installed-session qualification remain unbuilt and unclaimed until the
transport milestone passes the nested-session matrix in the
[testing harness](../development/testing-harness.md).
