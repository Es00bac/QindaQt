# ADR-0115: Share appearance through the native Qt platform theme

- Status: Accepted
- Date: 2026-09-08
- Supersedes: None; extends ADR-0013, ADR-0080 and ADR-0109 to standard Qt consumers

## Context

The user requires the current QindaQt appearance to be available to ordinary Qt
applications while preserving the working desktop and first-party presentation. QindaQt applications previously painted Controls skins
and widget stylesheets from QST while unrelated Qt applications used a different
platform palette. Exporting only the portal's color-scheme, accent and contrast
preferences cannot provide a complete Qt palette or font. KDE configuration and
Plasma appearance services are not the requested authority.

## Decision

Keep existing QindaQt theme JSON, confirmed Settings1 preferences, AppAppearance
resolution and QST semantic values as the desktop's appearance source. Publish
those values through a `qindaqt` Qt `QPlatformTheme` plugin that supplies standard
`QPalette`, system/fixed `QFont`, icon theme and light/dark/contrast hints. It adds
no theme format or appearance persistence and never writes KDE or Qt application
configuration.

Use Qt's built-in Fusion widget and Quick Controls styles as defaults for
ordinary Qt consumers. This adapter does not paint controls, remove existing
QindaQt control skins or change application chrome. Existing first-party QST
consumers continue using the same confirmed preferences. Semantic content such
as terminal ANSI text, container identity and shell surfaces is unchanged.
Explicit application palette/font/style choices retain Qt's normal authority;
a desktop cannot force an application to honor system defaults.

The session selects `QT_QPA_PLATFORMTHEME=qindaqt` and
`QT_QUICK_CONTROLS_STYLE=Fusion` only when the user has not supplied those
variables. The activation environment propagates the selections to D-Bus and
systemd-launched consumers, including clearing stale optional widget-style
values from an earlier desktop. No KDE appearance environment or configuration
is selected.

`src/platform/qt_theme` confines the version-sensitive Qt QPA API. Its pure
palette projection depends on public ThemeSpec/QST and Qt Gui. Its plugin alone
links matching `Qt6::GuiPrivate` and reuses the public Settings1 client. Qt owns
widget rendering and the underlying generic platform theme remains responsible
for native platform services such as tray/menu integration. The plugin does not
implement a QStyle, compositor, tray protocol or chooser backend.

The plugin initially publishes the installed `qinda-dark` fallback, then starts
its asynchronous Settings1 subscription after QApplication construction. It
publishes complete confirmed changes through Qt's ordinary theme-change event;
invalid/unavailable state retains the last valid palette, and the adapter never
blocks application startup or reads Settings1 storage. Existing Qinda theme,
font and accessibility preferences remain unchanged and reach all conforming Qt
applications when the confirmed snapshot arrives.

## Consequences

- Qt applications can consume the selected QindaQt themes through their normal
  platform palette/font API; there is no separate KDE scheme migration.
- Custom Qinda JSON themes remain usable without conversion. Applications that
  opt out with explicit palettes or their own theme engine remain responsible
  for their rendering.
- Rebuild and reinstall the QPA plugin for each Qt ABI update. Distribution
  dependencies must track the Qt Gui subslot and include matching private
  development headers. Qt rejects incompatible plugin metadata where supported;
  package qualification remains mandatory.
- Tests load the real plugin into an ordinary Qt Widgets/Quick process on a
  private activation-free bus and verify startup, live palette/font changes,
  owner replacement, invalid-state retention and zero writes.
- This change does not remove appropriately scoped existing platform backends
  such as KWin or PowerDevil, nor introduce Plasma appearance services.

See [Qt platform theme](../architecture/qt-platform-theme.md) and
[QST-1 semantic tokens](../architecture/design-tokens.md).
