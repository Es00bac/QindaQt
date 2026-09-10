# ADR-0120: Panel translucency from elevation tokens with client-side blur

- **Status:** Accepted
- **Date:** 2026-09-10
- **Owners:** Shell / panel surfaces, Design tokens
- **Supersedes:** the "no local blur effect is created" clause of the
  production panel surfaces contract (previously the only option)
- **Superseded by:** None

## Context

Panels painted opaque backgrounds; the dock used 0.88 material opacity with
no blur. Users expect modern translucency: the wallpaper should read through
the dock and top bar with a blur, while remaining fully legible. Two forces
constrain the implementation. QST already derives per-surface `elevation`
levels (`backgroundBlur`, `blurRadius` 12/20/30) from the theme's
`blurEnabled` flag and the accessibility projection, so the *intent* exists
but nothing consumed it. And blur of what is *behind* a Wayland surface can
only be produced by the compositor: KWin's blur effect honors the
`org_kde_kwin_blur` protocol, which a client can request directly — no KWin
patch and no privileged path required.

## Decision

1. **One published truth.** A panel surface is translucent when, and only
   when: the theme enables blur, the accessibility projection does not
   reduce transparency or request high contrast, and the per-panel
   `transparency` quick setting (ADR-0118) is on. `PanelContent` publishes
   this as `materialTranslucent`; the material's token color keeps its role
   and renders at reduced opacity when translucent, fully opaque otherwise.
   The accessibility projection always wins over the quick setting.
2. **Blur by protocol.** The shell requests blur-behind through the vendored
   `org_kde_kwin_blur` protocol (client-side `QWaylandClientExtension`), with
   a region equal to the painted material bounds intersected with the window
   rect — the same bounds that drive the input mask. The region is pushed on
   every published bounds or translucency change and removed when
   translucency is off. A build without the Wayland scanner or QPA private
   headers compiles the helper into a stored-state no-op, and non-Wayland
   platforms are runtime no-ops, so presentation never needs platform
   branches.
3. **No palette literals.** Translucent material colors, borders, and radii
   come from the published QST roles exactly as before; only the opacity
   step is new, and it is a presentation constant, not a theme role.

## Consequences

- Panels and docks render translucent with compositor blur when the theme
  and user settings allow it, and flatten to exactly the previous opaque
  rendering under reduced transparency, high contrast, or the quick toggle.
- The blur region and the input mask consume the same bounds, so the two can
  never disagree about what the panel covers.
- KWin's stock blur effect must be enabled for the blur to render; with the
  effect disabled the translucent material still renders (unblurred).
- The repo gains a small vendored protocol XML (LGPL-2.1, KWin-supported)
  and a Qt Wayland client-codegen dependency for one module.
