# QindaQt Settings portal backend version 1

This page fixes QindaQt's implementation of the standard
`org.freedesktop.impl.portal.Settings` backend interface. It is a compatibility
profile of the upstream
[xdg-desktop-portal 1.20.4 XML](https://github.com/flatpak/xdg-desktop-portal/blob/1.20.4/data/org.freedesktop.impl.portal.Settings.xml),
not a QindaQt-private protocol.

## Address and version

| Field | Value |
| --- | --- |
| Well-known backend name | `org.freedesktop.impl.portal.desktop.qindaqt` |
| Object path | `/org/freedesktop/portal/desktop` |
| Interface | `org.freedesktop.impl.portal.Settings` |
| Read-only property | `version` (`u`) = 1 |

Applications do not call this backend name directly in production. They use
the `org.freedesktop.portal.Settings` frontend, which selects and forwards to a
desktop backend.

## Frontend routing for the QindaQt desktop

`qindaqt-portals.conf` is an exact allowlist. `default=none` closes every
family absent from this table; listing a fallback is routing policy, not a
claim that the fallback package is installed or that QindaQt implements that
family.

| Frontend family / backend interface | Ordered selection | QindaQt authority |
| --- | --- | --- |
| Settings / `org.freedesktop.impl.portal.Settings` | `qindaqt` | This version-1 backend |
| Access | `qindaqt` | [Native foundation](../architecture/portal-foundation.md) |
| AppChooser | `qindaqt` | [Native chooser version 1](portal-choosers.md) |
| FileChooser | `qindaqt` | [Native chooser version 4](portal-choosers.md) |
| Email | `qindaqt` | [Native foundation](../architecture/portal-foundation.md) |
| Inhibit | `kde;gtk;lxqt` | None |
| Notification | `qindaqt` | [Native foundation](../architecture/portal-foundation.md) |
| Print | `kde;gtk;lxqt` | None |
| Screenshot | `kde;gtk;lxqt` | None |
| ScreenCast | `kde;gtk;lxqt` | None |
| RemoteDesktop | `kde;gtk;lxqt` | None |
| GlobalShortcuts | `kde` | None |
| Secret | `qindaqt` | Separate [native Secret module](../architecture/secret-portal.md) |
| InputCapture | `kde` | None |
| Clipboard | `kde` | None |
| Usb | `kde` | None |
| Account | `kde` | None |
| DynamicLauncher | `kde` | None |
| Wallpaper | `none` | Deliberately unavailable |
| Background | `none` | Deliberately unavailable |
| OpenURI | Frontend-owned; no backend selector | None |
| Any unlisted family | `default=none` | Deliberately unavailable |

The frontend filters the ordered names by the staged providers that advertise
the requested implementation interface. The first available match wins.
QindaQt's `.portal` advertises Settings, Secret, Access, Notification, Email,
FileChooser and AppChooser. Every other family retains its explicit fallback or
closed route. An unavailable native chooser fails closed without fallback.

Every family routed to `kde` depends on the KDE backend running under the
compatibility identity QindaQt's systemd drop-in supplies
([ADR-0088](../adr/0088-enable-kde-remote-desktop-for-qindaqt.md)). That backend
registers ScreenCast, Screenshot, RemoteDesktop, InputCapture and
GlobalShortcuts **only** with `XDG_CURRENT_DESKTOP=KDE`: measured on the same
binary and compositor, the KDE identity yields 19 impl interfaces and the
QindaQt identity 11, with none of the capture ones. The drop-in therefore also
overrides the unit to `Type=exec`
([ADR-0170](../adr/0170-survive-a-private-session-bus-for-dbus-units.md)),
because on a session running the private bus QindaQt bootstraps itself the
systemd user manager cannot observe a `Type=dbus` unit taking its name, kills
the working backend at `TimeoutStartSec`, and D-Bus activation respawns it
without the drop-in - leaving screen capture unavailable desktop-wide. The
`qindaqt.portal-kde-compat` row asserts both halves.

GlobalShortcuts lists only `kde` rather than the uniform `kde;gtk;lxqt` order
used for the other reviewed families: the installed `xdg-desktop-portal-gtk`
and `xdg-desktop-portal-lxqt` backends do not advertise
`org.freedesktop.impl.portal.GlobalShortcuts` in their `.portal` metadata, so
listing them would document an unsupported fallback rather than an inert one.
The boundary test compares the exact selector string, so a future edit that
widens this entry back to `kde;gtk;lxqt` fails closed until the listed
backends are re-verified. See
[ADR-0086](../adr/0086-route-globalshortcuts-only-to-a-verified-backend.md).

Secret lists only `qindaqt`, the separate native Secret backend. The installed
KWallet and GNOME declarations do not receive a fallback. Native per-application
identity, record persistence and opaque legacy compatibility belong to
[ADR-0310](../adr/0310-native-per-application-secret-portal.md) and
[ADR-0312](../adr/0312-preserve-exact-legacy-portal-secrets.md). The routing checker
rejects a dropped or rerouted Secret row under the closed default.

InputCapture, Clipboard, and Usb list only `kde` because it is the only
installed provider whose `.portal` metadata advertises those interfaces.
Account and DynamicLauncher also list only `kde` even though the installed
`gtk.portal` advertises them: the KDE backend is the primary provider in a
KWin session, and keeping one reviewed consent surface beats documenting a
second inert fallback. Wallpaper is closed like Background: the KDE backend
would change Plasma's wallpaper, which QindaQt does not use, and wallpaper
surfaces are owned by the shell
([ADR-0078](../adr/0078-own-wallpaper-surfaces-in-the-shell.md)). The
frontend exports a family's interface only when the selector resolves an
implementation for it, so `none` rows keep Wallpaper and Background
unexported entirely; the private routing proof asserts both stay unexported.

## Methods and signal

| Member | D-Bus signature | Behavior |
| --- | --- | --- |
| `ReadAll` | `as -> a{sa{sv}}` | Returns matching supported namespaces, or an empty map while truth is unavailable |
| `Read` | `s, s -> v` | Returns one supported value; unknown namespace/key or unavailable truth is an error |
| `SettingChanged` | `s, s, v` | Emitted for each exported value that differs from the last accepted signalled policy |

The only supported namespace is `org.freedesktop.appearance`. An empty
`ReadAll` filter list, an empty filter, or `*` matches it. An exact namespace
matches itself; a single trailing section wildcard such as
`org.freedesktop.*` matches by prefix. Other patterns do not match. At most 64
filters are accepted and each namespace/key/filter is at most 256 UTF-8 bytes,
contains no NUL, and contains valid Unicode scalar structure. A bound failure
returns `org.freedesktop.DBus.Error.InvalidArgs`.

## Exported values

| Namespace/key | Signature | Values |
| --- | --- | --- |
| `org.freedesktop.appearance color-scheme` | `u` | 0 no preference, 1 prefer dark, 2 prefer light |
| `org.freedesktop.appearance accent-color` | `(ddd)` | opaque sRGB red, green, blue, each finite and within `[0,1]` |
| `org.freedesktop.appearance contrast` | `u` | 0 no preference, 1 prefer higher contrast |

`ReadAll` returns exactly these three keys when authoritative truth exists.
QindaQt exports no private settings keys and no other namespace. The value
meanings follow the upstream
[Settings portal contract](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Settings.html).

## Failure and replacement semantics

Before the first complete Settings1 baseline, and immediately after exact
owner/epoch loss, `ReadAll` returns an empty map. `Read` returns
`org.freedesktop.DBus.Error.Failed` while truth is unavailable and
`org.freedesktop.DBus.Error.UnknownProperty` for an unsupported namespace or
key while truth is available.

The signal has no unset operation. QindaQt therefore emits no synthetic signal
on withdrawal. The next complete lineage is compared with the last signalled
policy, and only changed values are emitted. A late response or signal from a
retired Settings1 owner cannot restore or mutate current portal truth.

See [XDG Settings portal appearance backend](../architecture/portal-service.md)
for source projection, activation, packaging, and qualification.
