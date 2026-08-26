# ADR-0005: Render Hybrid chrome as a member-anchored scene item

- **Status:** Accepted
- **Date:** 2026-08-26
- **Owners:** Compositor and Hybrid chrome
- **Supersedes:** The internal QWidget/QWindow surface portion of
  [ADR-0004](0004-process-local-hybrid-topology.md)
- **Superseded by:** None

## Context

ADR-0004 selected a paint-only internal Qt window for the shared frame around a
group. Qualification against the pinned KWin 6.6.5 implementation exposed two
non-local constraints that invalidate that surface:

- KWin's Qt platform abstraction implements top-level `raise()` as a no-op and
  classifies ordinary internal windows in its overlay layer. The surface cannot
  occupy the same stack slot as the grouped clients, so unrelated windows
  cannot reliably cover it.
- KWin internal-window hit testing does not honor
  `Qt::WindowTransparentForInput`. An `outputOnly` backing `QWindow` can avoid
  input capture, but it does not repair the stacking model and adds a second
  native lifetime to every group.

The pure chrome render plan and compositor-side pointer router already avoid a
need for a native input surface. KWin's scene graph can paint an image item as a
child of a client `WindowItem`, which naturally inherits the client's stacking,
visibility, opacity, effects, and output transform.

## Decision

Production shared chrome is one compositor `ImageItem` per live container. Its
ARGB image is rendered from the immutable `HybridChrome` plan, clears the full
outer frame first, and subtracts every complete member frame so application
content and native member decorations remain unobscured. The item is parented
to the topmost active-page member's `WindowItem` and positioned relative to
that anchor.

The compositor compacts each active-page group into one stable stacking block,
keeps associated transient subtrees above the complete member block, and
reanchors chrome whenever membership, page, activation, output, or stack state
changes. Failure to resolve or attach the current anchor hides the item and
rejects synchronization; a visible but unanchored item is never accepted.

No production QWidget, QWindow, input mask, `outputOnly` property, focusable
surface, or independent KWin layer is created for shared chrome. Ordinary
chrome interaction remains coordinate-based policy in KWin's global input
filter. Native member titles and client regions remain owned by KDecoration and
the application. `ChromeWidget` remains available only for settings previews
and toolkit-focused tests.

## Consequences

- Shared chrome occupies the group's real stack slot: a later unrelated window
  covers it, while the group's dialogs remain above all grouped members.
- Chrome lifetime is coupled to a currently valid member `WindowItem`; anchor
  close, page changes, minimization, and plugin unload must reanchor or destroy
  it synchronously with publication.
- There is no native chrome window to inventory, focus, mask, raise, or exclude
  from managed clients. Pointer and accessibility semantics must therefore be
  exposed by compositor-owned adapters rather than a widget hierarchy.
- Tests must sample transparent member holes, assert a real scene attachment,
  inspect group/transient/unrelated stacking, and prove that anchor failure
  hides chrome atomically. A widget-only paint test is insufficient evidence.
- The scene adapter is pinned to QindaQt's exact KWin ABI. The pure render plan,
  renderer, hit tester, semantic commands, and preview widget remain independent
  Qt modules.

## Revisit when

Reconsider this decision if KWin gains an upstream multi-client decoration or
group-scene primitive with equivalent stacking, effects, accessibility, input,
and lifetime behavior. Do not return to a native overlay without live evidence
that unrelated client stacking and transparent input both work on the pinned
compositor.
