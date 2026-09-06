# ADR-0089: Present task switching through KWin's native model

- **Status:** Accepted
- **Date:** 2026-09-06
- **Owners:** Session and compositor presentation
- **Supersedes:** None
- **Superseded by:** None

## Context

KWin owns Alt-Tab focus order and activation in a QindaQt session. Hybrid
container identity already projects exactly one active-page representative by
setting KWin's native `skipSwitcher` state, but a session with no explicit
`TabBox/LayoutName` uses KWin's `thumbnail_grid` presentation. The result has
correct container semantics with a visibly foreign task-switching interface.

Building a second shell switcher would duplicate focus order, shortcut
lifetime, multi-output placement, and activation authority. Replacing KWin's
model would also create another container projection beside `skipSwitcher`.
KWin already supports installed `KWin/WindowSwitcher` packages over its public
process-registered `TabBoxSwitcher` model.

The package executes in KWin's QML engine. QST-1 publication is intentionally
owned by each first-party shell or application engine, so its singleton has no
published generation in this host engine. A platform-hosted extension cannot
pretend that an uninitialized QST map is confirmed appearance truth.

## Decision

QindaQt ships a `qindaqt` `KWin/WindowSwitcher` package. It consumes only the
native caption, icon, minimized state, current index, and activation operation.
KWin keeps shortcut, ordering, focus, and activation authority. Hybrid's native
skip flags remain the sole container-to-switcher identity projection; the
package neither queries topology nor invents a second group model.

The session selects this package only when `TabBox/LayoutName` is absent.
Explicit user layouts, including an intentionally restored KWin layout, survive
all later launches.

Within KWin's engine, the package uses Kirigami's host semantic palette and
font roles and draws its own frameless structure. This is a narrow exception to
the QST-1 presentation rule for this KWin-hosted package; it adds no theme
identifier, palette literal, persistence, or shell dependency.

## Consequences

- Alt-Tab presents one clear selected row per KWin switcher entry. A container
  uses its single native representative's caption and icon. Ordinary rows add
  no generic type subtitle; only a minimized entry states that extra status.
- Presentation can change without changing window ordering or activation.
- The package follows the semantic palette selected in KWin's process rather
  than importing unconfirmed shell-engine QST state.
- Package metadata, public-model usage, accessibility bindings, and QML are
  validated without launching or mutating the active desktop.
- A nested visual test remains the final evidence for appearance, multi-output
  placement, and actual Alt-Tab activation.

## Revisit when

Reconsider this boundary if KWin removes `KWin/WindowSwitcher`, publishes a
stable themed switcher API with container roles, or QindaQt gains a supported
cross-engine QST publication surface for compositor-hosted QML.
