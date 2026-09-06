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
| Access | `kde;gtk;lxqt` | None |
| AppChooser | `kde;gtk;lxqt` | None |
| FileChooser | `kde;gtk;lxqt` | None |
| Email | `kde;gtk;lxqt` | None |
| Inhibit | `kde;gtk;lxqt` | None |
| Notification | `kde;gtk;lxqt` | None |
| Print | `kde;gtk;lxqt` | None |
| Screenshot | `kde;gtk;lxqt` | None |
| ScreenCast | `kde;gtk;lxqt` | None |
| RemoteDesktop | `kde;gtk;lxqt` | None |
| GlobalShortcuts | `kde` | None |
| Background | `none` | Deliberately unavailable |
| OpenURI | Frontend-owned; no backend selector | None |
| Any unlisted family | `default=none` | Deliberately unavailable |

The frontend filters the ordered names by the staged providers that advertise
the requested implementation interface. The first available match wins.
QindaQt's `.portal` declaration continues to advertise only Settings, so no
fallback family can resolve to the QindaQt process.

GlobalShortcuts lists only `kde` rather than the uniform `kde;gtk;lxqt` order
used for the other reviewed families: the installed `xdg-desktop-portal-gtk`
and `xdg-desktop-portal-lxqt` backends do not advertise
`org.freedesktop.impl.portal.GlobalShortcuts` in their `.portal` metadata, so
listing them would document an unsupported fallback rather than an inert one.
The boundary test compares the exact selector string, so a future edit that
widens this entry back to `kde;gtk;lxqt` fails closed until the listed
backends are re-verified. See
[ADR-0086](../adr/0086-route-globalshortcuts-only-to-a-verified-backend.md).

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
