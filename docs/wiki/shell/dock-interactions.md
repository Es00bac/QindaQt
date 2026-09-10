# Dock interactions

The dock (a centered bottom panel whose resolved applets request `dockMode`)
supports the interaction set described here. This page owns the presentation
contracts for dock growth, overflow scrolling, magnification, drag-and-drop
reordering, hover previews, translucency, and the panel right-click
configuration menu. The task-list data contracts stay in
[Task list source model](task-list.md); surface geometry stays in
[Production panel surfaces](panel-surfaces.md).

## Growth and overflow

Both stock dock panels (the QindaQt smart shelf and the macOS-style dock)
solve at full edge length (`length: 1.0`). The painted shelf still hugs its
content: it grows as tasks, pins, and launcher tiles are added, fills the
window at full output width, and never truncates. Beyond full width the
center-zone viewport scrolls — by wheel, flick, or keyboard reveal — behind
a thin non-interactive token-styled indicator that exists only while its own
axis overflows (the [panel hit targets](panel-surfaces.md#panel-hit-targets)
contract). In dock mode the strip projects every task row up to the
compositor fact ceiling; the taskbar strip keeps its 64-row cap and exact
"+N more" overflow truth.

## Magnification (macOS-style zoom)

Pointer proximity swells dock tiles with a Gaussian falloff (peak 1.5 at the
hovered tile, roughly two neighbors participating). Magnification transforms
tile visuals only — delegate sizes, layout bounds, and hit targets never
move, which keeps the strip's layout contract and the zone viewport exact.
The tile reserves its full envelope before the icon swells, bottom-anchored.
The effect is disabled by `reducedMotion` or by the per-panel `dockZoom`
quick setting. The same seam backs the pins strip.

## Drag-and-drop reorder

Dock tiles reorder by drag (threshold-gated, so clicks still activate) or by
the tile context menu's "Move left/right" actions. Both paths commit through
the controller's fenced reorder intent: the drop is applied only against the
displayed generation, and the committed order is persisted through Settings1
(`panels.configuration`, [ADR-0118](../adr/0118-user-task-order-overlay-and-panel-quick-settings.md)).
The persisted overlay takes precedence over the canonical order for display
and traversal; windows that close keep their stored position and return to
it when they reopen. Reorder is presentation preference only — no
compositor operation is dispatched, and nothing is pending while dragging.

## Hover previews

Hovering a tile opens one preview card per strip above the tile: the
window's live capture (for containers, the primary active-page member),
refreshed at most every 500 ms during a static hover, closed on leave or
activation. Previews require the authenticated compositor preview channel
([ADR-0119](../adr/0119-authenticated-window-preview-channel.md)); when the
channel is absent or a capture fails, the card degrades to its title-only
form and the classic tooltip remains the fallback whenever the preview seam
is not wired. Previews are observation: `windows.read` gating and the ready
phase apply, and a stale capture can never decorate a newer generation.

## Translucency and blur

Panel materials render translucent with compositor blur when the theme
enables blur, the accessibility projection allows it, and the per-panel
`transparency` quick setting is on
([ADR-0120](../adr/0120-panel-translucency-and-blur.md)). The accessibility
projection always wins: reduced transparency and high contrast flatten every
panel to exactly the historical opaque rendering. The blur-behind request
consumes the same painted bounds as the input mask.

## Panel right-click configuration

Right-clicking a panel's own surface (behind every applet chip) opens the
panel configuration menu: "Customize Panel…" (opens the Settings app's
Customize route) plus live quick settings — transparency for every panel,
and magnification plus tile size (56/60/64) for docks. Every quick setting
persists through Settings1 and applies immediately. Deep layout edits
(alignment, autohide, applet arrangement) remain Customize-editor work until
the live profile-binding slice lands; the menu routes there.
