# ADR-0121: Real iconography in the Settings Customize route

- **Status:** Accepted
- **Date:** 2026-09-10
- **Owners:** Settings / Customize route, Shell iconography
- **Supersedes:** None
- **Superseded by:** None

## Context

The Customize route's WYSIWYG preview, applet palette, and inspector must
show the same glyphs the desktop panels show; a layout editor that labels
its applets with text rows and anonymous dots cannot answer "what am I
moving". The confined shell iconography (`src/shell/icons`,
[ADR-0072](0072-shell-iconography-confined-xdg-icon-themes.md)) already
provides exactly that: a public compiled `QindaQt.Shell.Icons` module over
an engine-installed `image://qindaqt-icon/` provider with a typed fallback,
and no D-Bus, compositor, or shell-surface dependency. Before this decision
the Settings engine installed no icon runtime, so any icon element would
have rendered letter placeholders, and the route's boundary contract said it
"never imports shell surfaces" without distinguishing the icon module.

## Decision

1. The Settings composition root installs `IconRuntime::install` once per
   engine, after QST-1 publication and before any route QML exists. Icon
   roots are the freedesktop data locations, plus the relocated
   `../share/icons` layout beside an installed executable, plus the source
   icon catalog only for the exact build executable — the same guard the
   developer QML import path already uses. The bundled themes all name the
   `QindaQt` theme, which is passed as the chain; a failed install is
   non-fatal and degrades glyphs to typed placeholders.
2. The Customize route imports `QindaQt.Shell.Icons` as a presentation-only
   dependency, like `QindaQt.Controls` and `QindaQt.Tokens`. The route
   keeps its existing boundary (no shell surfaces, compositor, or transport
   imports); the icon module is catalogued public surface, and the
   plugin-mapping the preview uses is a static projection inside the route.
3. Recolors are always opaque token colors. The bundled symbolic SVGs
   stroke the theme canvas color, so a translucent recolor target (the
   disabled-foreground overlay, for example) is "no recolor" to the
   provider and renders invisible glyphs on dark themes; disabled states
   dim by opacity instead.
4. Installed Settings packages stage the icon module beside the route
   through the existing `IconRuntimeInstall.cmake` closure under the
   `SettingsAppearanceRuntime` component, so a package needs no shell
   install to resolve the import.

## Consequences

- The Customize preview, palette, and inspector render real applet glyphs
  that follow the active theme, and every in-process host of the route's
  QML (the app plus the page, pointer, lifecycle, and Settings Center
  navigation test rows) links the icon module plugin and installs the
  runtime or deliberately accepts placeholder rendering.
- Settings gains a dependency on `QindaQt::ShellIcons` (public,
  Qt-only). It remains free of Plasma, KDE Frameworks, and D-Bus surface
  dependencies; no external packages are required.
- The settings app's icon theme chain is fixed at install time to the
  bundled theme name rather than following a live appearance theme switch;
  glyphs are theme-neutral shapes and a switch changes at most their
  recolor.
