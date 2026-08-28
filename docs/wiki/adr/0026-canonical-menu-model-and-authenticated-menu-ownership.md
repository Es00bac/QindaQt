# ADR-0026: Canonical menu model with authenticated active-window ownership

- **Status:** Proposed
- **Date:** 2026-08-28
- **Owners:** Shell / global menu
- **Supersedes:** None
- **Superseded by:** None

## Context

The panel's global-menu applet must show the focused application's menu without
turning the shell into an application framework or trusting every process on
the session bus. The de facto standard, `com.canonical.AppMenu.Registrar`,
lets any bus client register a menu for any window id it names; the registrar
does not authenticate that the registering peer owns the window or even that
the window exists. Reproducing that trust model would let a hostile client
replace or spoof another application's menu and redirect its shortcuts.
First-party menus also need to survive toolkit diversity eventually, while
today only native Qt menus exist (the Text Editor's ordinary `QMenuBar`/
`QAction` tree per [ADR-0022](0022-keep-text-documents-local-and-atomic.md)).
The owning behavior and milestone boundary are in
[Global application menu](../shell/global-menu.md).

## Decision

The shell owns a bounded, toolkit-neutral canonical menu/action model and a
proof-bound ownership policy built on one authoritative lineage:

- Every menu crossing a boundary is a canonical `MenuTree` value with fixed
  hostile-input ceilings, well-formed-Unicode text (no embedded NULs, no
  isolated surrogates), toolkit-neutral mnemonic representation, globally
  unique stable ids, and owner/epoch/revision lineage mirroring Display1,
  Audio1, and Settings1. Validation rejects malformed input — including
  unknown item kinds — as a whole, never as a traversable prefix.
- The ownership selector is the single lineage authority: it mints the epoch
  and advances the revision, the exporter stamps exactly that lineage into
  accepted trees through an injected composition seam, and invocation
  requests carry the revision. Export, publication, and invocation therefore
  share one source of truth, and an ordinary authenticate → adopt → export →
  invoke flow is coherent over the public API alone.
- Focus observation carries a monotonic generation; authentication re-reads
  focus after the credential lookup and fails on any change, returns a proof
  carrying exactly the verified facts, and only that proof is acceptable for
  adoption. A focus-generation change invalidates the current adoption
  before any export or invocation can run.
- The exporter validates every pulled snapshot and fails closed by retaining
  the last accepted tree on invalid, incomplete, or unauthorized pulls.
  Sources report traversal completeness explicitly; overflow, cycles, or
  mid-traversal defects reject the snapshot whole instead of publishing a
  bounded prefix. G0 keeps snapshot-only truth; any future delta contract
  requires its own design with an apply-to-next-tree proof.
- Toolkit adaptation is confined to adapters; the Qt Widgets `QMenuBar`
  adapter is the only component permitted to link `Qt6::Widgets` inside the
  global-menu module, so the QtQuick shell stays Widgets-free.
- Presentation is honest about what exists: submenu entries render visibly
  but non-activating until a popup milestone, hidden items are omitted,
  and keyboard plus accessible-press activation are first-class.

## Consequences

- A hostile or buggy client cannot publish a menu for a window it does not
  own, a focus race cannot authenticate a stale window, and a stale UI
  request cannot trigger an action of a provider or revision that has since
  lost authority.
- Compatibility with `com.canonical.AppMenu.Registrar` is deliberately not
  provided in this milestone; toolkits or apps needing it require a future
  ADR covering the trust gap.
- Applications that want stable ids must publish persistent action object
  names; positional fallback ids degrade gracefully but are only stable
  while the sibling structure is unchanged.
- The model's fixed ceilings reject pathologically large menus wholesale;
  a legitimate application exceeding them must be redesigned or the limits
  widened through the same governance as other wire-limit changes. Adapters
  enforce the same ceilings during traversal and report incompleteness
  rather than truncating.
- Focused hostile tests must cover validation bounds and unknown kinds,
  focus-change rejection and proof-bound adoption, lineage composition with
  same-epoch stale revisions, fail-closed and incomplete export, adapter
  overflow/cycle behavior, and facade/QML activation gating before any
  transport milestone.

## Revisit when

A real cross-toolkit or legacy application must export menus, a per-window
registration cache is product-required, a payload-bearing delta contract is
needed by a transport, or a compositor-authenticated window inventory becomes
available to replace the injected G0 seams with a resident transport.
