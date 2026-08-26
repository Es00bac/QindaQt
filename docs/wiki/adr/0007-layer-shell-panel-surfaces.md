# ADR-0007: Use LayerShellQt behind a QindaQt panel-surface boundary

- **Status:** Accepted
- **Date:** 2026-08-26

## Context

QindaQt's pure `shell_layout` solver already expands validated profile panels
onto named outputs and produces exact compositor-logical rectangles and work
areas. A production shell must turn those values into Wayland surfaces with
stable output placement, stacking, and exclusive zones. Ordinary Qt windows do
not expose the layer-shell roles needed by docks and panels.

KWin 6.6.5 implements `zwlr_layer_shell_v1`, and the release-matched
LayerShellQt 6.6.5 library exposes that protocol through public Qt APIs. Using
it avoids a QindaQt-specific protocol client while keeping Plasma shell code
out of the runtime. The layer-shell protocol has background, bottom, top, and
overlay layers, but QindaQt profiles retain the user-facing `below`, `normal`,
`above`, and `overlay` vocabulary.

## Decision

QindaQt will use `LayerShellQt::Interface` for production panel and dock
windows. The dependency is required only when the production shell is enabled;
pure profile, layout, customization, and preview builds remain independent of
it.

A dedicated `src/shell_surface` module owns the conversion from a successful
`PanelLayoutResult` to backend-neutral surface configurations, failure-aware
reconciliation, Qt output inventory, and the private LayerShellQt adapter. The
runtime shell supplies a window factory and panel presentation. QML may render
injected panel and theme values, but it may not choose outputs, geometry,
anchors, margins, layer, exclusive edge, exclusive zone, or publication order.

One surface is identified by the pair `(panelId, outputId)`. All geometry stays
in Qt/KWin logical coordinates; output scale remains metadata and is never
multiplied into the solved rectangle.

Profile layers map as follows for the first protocol version:

| Profile layer | Layer-shell layer | Work-area behavior |
| --- | --- | --- |
| `below` | bottom | never reserves |
| `normal` | top | reserves when selected as the edge carrier |
| `above` | top | reserves when selected as the edge carrier |
| `overlay` | overlay | never reserves |

Layer shell has no distinct role between QindaQt `normal` and `above`. Keeping
both on the top layer preserves their existing work-area contract without
inventing compositor-private stacking. A future protocol or explicit KWin
extension may supersede this mapping if users need a distinction beyond theme
and profile policy.

For each output edge, only the deepest reserving surface publishes a positive
exclusive zone equal to its thickness. Layer shell includes that surface's
anchored-edge margin, so margin plus zone represents the solver's complete
reserved depth without double-counting the stack. Other
surfaces use the protocol's non-reserving value rather than asking the
compositor to independently reposition them. Fill surfaces use the protocol's
opposite-edge anchoring convention; partial surfaces use explicit edge,
alignment anchors, desired size, and margins derived from the solver rectangle.

The controller validates and prepares a complete replacement set before
publication and keeps the prior set alive until preparation succeeds. When
surface identity and every static role value are unchanged, the published set
may instead apply margins, exclusive zones, and mapped state in place. This
preserves QML and applet instances across automatic-hide transitions. The
backend prevalidates the complete candidate and rolls back synchronous Qt state
if a transition fails. Structural changes continue to use replacement.

The Wayland protocol does not provide an atomic transaction spanning several
layer surfaces, so either path is failure atomic in QindaQt state but not a
claim of single-frame compositor atomicity. A close event marks a retained role
as compositor-dismissed; ordinary `hide()` does not. Exact screen identity,
logical geometry, and scale are also liveness requirements.

## Consequences

- QindaQt reuses a small public KDE library without depending on Plasma shell.
- The production panel runtime is Wayland-only and fails clearly on another Qt
  platform; the deterministic offscreen/X11 preview remains separate.
- `QScreen::name()` is the initial runtime output identifier because KWin and
  Qt expose it consistently in the pinned stack. Stable persistence aliases,
  connector replacement, and display-policy migration belong to Platform
  services and require later qualification.
- Layer and exclusive-zone correctness requires nested compositor evidence;
  screenshots alone cannot prove either property.
- Window-aware policy and transport remain outside this adapter. Applet
  execution, global menu, notification presentation, reveal animation, and
  live settings publication remain separate shell slices.

## Alternatives considered

- **Implement `zwlr_layer_shell_v1` directly.** Rejected because it duplicates
  mature protocol and Qt-window integration already maintained by KDE.
- **Use ordinary frameless Qt windows.** Rejected because output anchoring,
  work-area exclusion, and well-defined shell stacking would be heuristic.
- **Import Plasma panel implementation.** Rejected because it would couple the
  shell to Plasma runtime and undermine QindaQt's modularity and memory target.
- **Map every profile layer to a unique protocol layer.** Rejected because
  background is not a truthful substitute for an ordinary below-window panel,
  and the profile's `normal`/`above` distinction has no direct layer-shell
  equivalent.
