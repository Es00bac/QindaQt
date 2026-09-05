# ADR-0071: Shell iconography over confined XDG icon themes

- **Status:** Accepted
- **Date:** 2026-09-04
- **Owners:** Shell iconography (`src/shell/icons`)
- **Supersedes:** None
- **Superseded by:** None

## Context

Shell surfaces presented text labels and one-letter placeholders because no
shell-wide icon resolution existed; only the status-notifier tray had a
confined locator/renderer
([status tray](../shell/status-tray.md)), and only for its own wire protocol.
Applets, the task list, and future shell surfaces all need application and
standard icons. The alternatives were: (a) per-applet ad-hoc lookups, which
duplicate confinement rules and drift; (b) adopting a framework icon engine
(KIconThemes), which would import Plasma-style ambient state (global theme
selection, platform integration) the shell deliberately avoids; (c) one
shell-owned module generalizing the tray's proven confinement rules.

## Decision

One shell iconography module (`src/shell/icons`) owns shell-wide icon
resolution, behind constructor-injected roots and fail-closed rules:

- XDG Icon Theme Specification lookup (`IconThemeLocator`) over an injected,
  ordered root list with parsed `index.theme` (Directories, Size, Scale,
  Fixed/Scalable/Threshold, MinSize/MaxSize, Inherits with cycle guard and
  bounded depth, hicolor always last), the specification's closest-size rule
  with scale awareness, deterministic ordering, `-symbolic` preference on
  request, and SVG/PNG/XPM formats. Every candidate is canonicalized and
  confined beneath its injected root; indexes and caches are bounded.
- Desktop-entry icon resolution (`DesktopEntryIconResolver`) over injected
  application roots, reusing the launcher's public pure desktop-entry parser
  rather than adding a second parser. Hidden entries and non-name `Icon=`
  values are refused.
- Icon delivery to QML exclusively through an engine-installed
  `QQuickImageProvider` (`image://qindaqt-icon/`), installed by the explicit
  `IconRuntime::install` composition seam with an injected theme chain
  (production default `breeze`, then `hicolor`). Unresolved names return a
  deterministic neutral placeholder image, never a null image or a warning.
- A compiled `QindaQt.Shell.Icons` module provides the `Icon` element with
  typed token-colored fallback (`fallbackText` glyph), accessible naming,
  and no hard-coded colors.

The status-notifier icon module remains untouched; its narrower wire-pixmap
contract stays local to the tray.

## Consequences

- The shell gains one auditable confinement boundary for icon filesystem
  reach; consumers cannot read icon files outside injected roots.
- Qt Svg becomes a dependency of this module only (QSvgRenderer); the pure
  launcher model and status-notifier modules do not gain it.
- The theme name is composition input, not module policy: the wiring lane
  adds an optional `iconTheme` key to the shell theme JSON and calls
  `IconRuntime::install` from the shell composition root.
- No applet, shell runtime, or QML surface is modified by this decision
  itself; wiring is a separate lane documented in
  [Shell iconography](../shell/iconography.md).
- Focused coverage runs under `^qindaqt\.shell-icons-` with hostile fixture
  trees, offscreen provider pixel assertions, and a fatal-warnings compiled
  module row.

## Revisit when

- A consumer needs resolution contexts the spec subset does not model
  (emblems, `KDE`-style icon effects, or runtime theme switching), or
- the tray's local locator converges with this module and the duplication
  can be retired without weakening the tray's wire contract.
