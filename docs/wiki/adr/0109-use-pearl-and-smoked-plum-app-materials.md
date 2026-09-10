# ADR-0109: Use Pearl and Smoked Plum application materials

- Status: Superseded by [ADR-0116](0116-build-bundled-applications-on-stock-qt6.md) for bundled applications; materials remain available to system surfaces
- Date: 2026-09-08

## Context

The user requested a distinctive, visual, less text-heavy first-party desktop,
with ordinary app windows composing through QindaQt's containers. The prior
mandatory blue/amber QindaPunk identity made unrelated application surfaces
look alike and did not establish readable terminal ANSI colors.

## Decision

Supersede the blue-body/amber-accent mandate in the visual identity with Pearl,
Smoked Plum and Velvet themes. Preserve theme identifiers and semantic QST-1
roles. Explicit custom themes remain authoritative. The former palette stays
in Git history; wallpaper choices and the mascot remain user-controlled.

Layer local translucent highlights over opaque semantic chrome to suggest
glass; do not introduce a compositor capture/blur dependency or place terminal
and editor text over wallpaper. Accessibility preferences remove material
layers and preserve opaque contrast. Original generated empty-state artwork
has recorded provenance; functional SVG icons remain deterministic.

Controls owns a narrow public app icon value lookup and QML Icon presentation.
Qt's selected icon theme is consulted with an embedded QindaQt catalog fallback;
applications select iconTheme through public Qt composition. No application
imports shell icon/runtime internals. A valid QML color requests alpha-preserving
tint and unknown icon names return empty rather than an unrelated symbol.

AppAppearance also projects confirmed font/accessibility inputs without owning
Settings1 transport or persistence. Explicit palette overrides do not suppress
readability preferences. Existing consumers may continue to subscribe only to
theme/scheme; absent optional preferences retain defaults or confirmed values.

## Consequences

App UI can use icons without depending on a shell process or installed ambient
artwork. Shared Controls consumers ship the catalog in their runtime. Theme,
icon, material, keyboard and real-app capture tests must be run together.
Public interfaces retain QST revision 1 and add only compatible Controls and
AppAppearance APIs. App-specific window ownership and preview/ANSI policies
are documented in their respective ADRs.

See [Visual identity](../shell/visual-identity.md), [Icon theme](../shell/icon-theme.md),
and [Design tokens](../architecture/design-tokens.md).
