# Shared native Qt appearance

QindaQt's `qindaqt` platform theme makes the selected Qinda theme, interface font,
monospace font and icon theme available through Qt's standard application APIs.
Ordinary Qt Widgets and Qt Quick Controls use the built-in Fusion style. The
source remains the existing QindaQt theme JSON and confirmed Settings1
preferences; no KDE configuration or alternative theme format is involved.
[ADR-0115](../adr/0115-share-appearance-through-qt-platform-theme.md) records the
additive platform boundary. Existing first-party skins, layouts and application
chrome are unchanged.

## Ownership and updates

`src/platform/qt_theme/native_palette.*` projects the public immutable QST value
into a complete active/inactive/disabled `QPalette` and interface/fixed `QFont`.
Window, base, text, highlight, tooltip, placeholder and disabled roles come from
those same semantic values. The projection is thread-neutral, owns its results,
and rejects invalid input without partial replacement.

The private QPA plugin is owned by QApplication on its GUI thread. It loads the
installed dark fallback before palette discovery, then constructs the public
Settings1 transport/client and AppAppearance observer on the first event-loop
turn. This avoids asking for style hints while QApplication itself is still
constructing. Subscription and failure policy remain in the existing
[AppAppearance boundary](../adr/0080-resolve-first-party-appearance-from-settings.md):
confirmed preferences replace one complete value; missing services, owner loss
and invalid values preserve the last valid appearance. No writes, file polling,
settings-store access or synchronous startup wait are added.

After palette/font/icon changes, the plugin issues Qt's ordinary theme-change
notification. Standard Widgets and Quick Controls can update live. An application
that explicitly sets a palette, font or style retains Qt's normal override
behavior; this adapter cannot force an unrelated custom UI to follow defaults.

## Session and platform boundary

The launcher defaults `QT_QPA_PLATFORMTHEME` to `qindaqt` and
`QT_QUICK_CONTROLS_STYLE` to `Fusion` only if each variable is absent. Explicit
user choices, including `QT_STYLE_OVERRIDE`, are preserved. The session's D-Bus
and systemd activation environment carries these values so launched applications
and activated services agree. Empty optional overrides clear stale user-manager
values inherited from another desktop.

Only the adapter links `Qt6::GuiPrivate`. Matching Qt development headers and a
rebuild for each Qt ABI change are required. The module installs below Qt's
`plugins/platformthemes` directory; palette projection itself uses only public
Qt and QindaQt values. The plugin asks Qt's platform integration directly for its
generic base theme and delegates tray/menu/dialog services to it. It does not
recursively discover another appearance plugin and does not implement those
platform protocols.

Existing [Settings portal](portal-service.md) scheme/accent/contrast exports
remain available to other toolkits. The QPA adapter supplements that standard
portal with the full palette/font contract required by Qt applications.

## Verification

Run `qindaqt.qt-platform-theme`, its `-explicit-style` and `-installed` variants,
`qindaqt.qt-platform-theme-services`, and `session.sessionenvironment`. The platform rows load
the built plugin into a real QApplication, constructs native Quick controls, and
uses a private D-Bus fixture with no activation directories or host settings. It
checks the shared Fusion palette, confirmed font, light/dark live transitions,
invalid-state retention, owner loss/replacement and no settings writes. The explicit-style variant retains a user-selected
Windows QStyle, and the installed variant repeats the live checks through a
relocated plugin staged without build-library lookup. Session
coverage proves defaults and explicit user override preservation. Package gates
must additionally verify installed plugin discovery with the matching Qt build.
The injected services row proves tray/menu/dialog delegation to the owned base
theme; native Wayland tray/menu interaction remains a combined session gate.

No test or package merge by itself proves that already running applications
have loaded a newly installed plugin. Session/application restart remains the
boundary for adopting new code.
