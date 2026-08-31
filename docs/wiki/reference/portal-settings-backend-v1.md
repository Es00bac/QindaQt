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
