# XDG Settings portal appearance backend

QindaQt provides a production, read-only XDG desktop-portal backend for its
appearance policy. Sandboxed applications and toolkits continue to call the
standard `org.freedesktop.portal.Settings` frontend owned by
`xdg-desktop-portal`; QindaQt implements only the desktop-specific
`org.freedesktop.impl.portal.Settings` backend selected by that frontend.

The upstream [backend authoring contract](https://flatpak.github.io/xdg-desktop-portal/docs/writing-a-new-backend.html)
requires an activatable executable, a `.portal` declaration, and backend
selection configuration. The exact method and signal contract is pinned to the
[xdg-desktop-portal 1.20.4 Settings XML](https://github.com/flatpak/xdg-desktop-portal/blob/1.20.4/data/org.freedesktop.impl.portal.Settings.xml),
which matches the installed development dependency. The user-visible standard
keys and value meanings are defined by the upstream
[Settings portal documentation](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Settings.html).

## Responsibility and non-goals

`src/services/portal` owns one narrow result: exporting confirmed QindaQt
appearance truth through the standard Settings portal. It has three internal
layers:

| Layer | Owns | Does not own |
| --- | --- | --- |
| Appearance policy | Exact Settings1 field validation, theme selection, QST-1 derivation, and standard value projection | D-Bus, activation, persistence, or theme discovery |
| Appearance source | A four-key, exact-owner/epoch Settings1 subscription and atomic ready/unavailable truth | Settings files, Settings1 service implementation, or UI |
| Portal adapter/process | Standard D-Bus marshalling, filtering, change signals, service-name ownership, activation, and shutdown on bus loss | Appearance persistence or any non-Settings portal |

The backend does **not** implement a chooser, OpenURI, notifications, inhibit,
screencast, remote desktop, a consent dialog, or portal-frontend policy. Its
`.portal` file advertises only `org.freedesktop.impl.portal.Settings`. It does
not replace, embed, or supervise `xdg-desktop-portal`, and the repository test
boundary does not qualify a host portal installation.

## Standard endpoint

The resident process owns:

- bus name `org.freedesktop.impl.portal.desktop.qindaqt`;
- object `/org/freedesktop/portal/desktop`;
- interface `org.freedesktop.impl.portal.Settings`; and
- backend property `version = 1`.

It implements `ReadAll(as) -> a{sa{sv}}`, `Read(s,s) -> v`, and
`SettingChanged(s,s,v)`. The detailed signatures, filter behavior, limits, and
errors are fixed in the
[Settings backend v1 reference](../reference/portal-settings-backend-v1.md).

## Appearance projection

The projection accepts exactly four fields from a complete, Ready Settings1
snapshot. Partial or extra input is not appearance truth.

| Standard result | QindaQt source | Projection |
| --- | --- | --- |
| `color-scheme` (`u`) | `appearance.colorScheme` | `system` → 0, `dark` → 1, `light` → 2 |
| `accent-color` (`(ddd)`) | `appearance.theme`, QST-1, and `accessibility.reducedTransparency` | selected theme's derived QST accent, converted to opaque sRGB components in `[0,1]` |
| `contrast` (`u`) | `appearance.theme` and `accessibility.highContrast` | 1 when the accessibility input is true or the selected theme is `high-contrast`; otherwise 0 |

The portal deliberately does not read the legacy
`appearance.accentColor` schema field. QST-1's selected theme plus explicit
accessibility inputs are the first-party rendering truth; exporting a separate
unused color would make sandboxed applications disagree with QindaQt itself.
Reduced transparency participates only because QST derivation can turn a
nonopaque theme source into an opaque final accent. If the final QST accent is
invalid, non-finite, outside sRGB, or nonopaque, the complete projection is
unavailable.

Theme discovery reads at most 16 unique absolute directories and 128 JSON
theme documents of at most 128 KiB each. Earlier directories win a duplicate
theme ID. A malformed discovered document, empty catalog, duplicate projector
ID, unknown selected theme, or failed QST derivation fails the entire policy
closed rather than publishing fallback colors.

## Lineage, loss, and notification

The source composes the public
[Settings1 client](settings-service.md) with a four-key scope. That client owns
activation, exact unique-owner binding, epoch/revision validation, bounded
subscription, debounce, timeout, and stale-reply rejection. The portal source
publishes only while the client is Ready with a complete snapshot.

Owner loss, replacement, bus loss, a malformed replacement baseline, or any
projection failure withdraws the current value before a later lineage can be
accepted. During withdrawal:

- `ReadAll` returns an empty map;
- `Read` returns a D-Bus failure; and
- no stale policy is retained as readable truth.

The standard signal has no removal form, so loss itself emits no fabricated
value. The adapter retains only its last signal-comparison policy, not readable
truth. A replacement baseline emits `SettingChanged` only for standard values
that actually differ; the first accepted baseline emits one bounded signal per
exported key. The process exits instead of reconnecting when its constructing
session bus disconnects, preventing an old lineage or comparison baseline from
crossing to a new daemon.

## Activation and package boundary

The `QindaQtPortalP0` install component contains the resident executable,
public policy/source headers and libraries, five built-in QST themes, the
D-Bus activation descriptor, a hardened user systemd unit, `qindaqt.portal`,
and `qindaqt-portals.conf`. The selector binds only
`org.freedesktop.impl.portal.Settings` to the `qindaqt` backend; its `default=*`
entry leaves every other portal interface to another installed provider, as
defined by upstream [portal selection rules](https://flatpak.github.io/xdg-desktop-portal/docs/portals.conf.html).

The executable and its injected Settings1 source are thread-confined to the
constructing Qt event loop. Startup registers the object, acquires the exact
name, then starts the source. Any failure rolls the earlier steps back. Stop
withdraws the source, releases the name, and unregisters the object. The user
unit permits only `AF_UNIX`, removes capabilities, and applies the repository's
resident-service hardening profile.

## Verification and limits

The complete P0 selector is:

```sh
ctest --test-dir build/dev \
  -R '^qindaqt\.portal-' --output-on-failure --no-tests=error
```

It covers pure mapping and hostile theme/snapshot input, exact-owner
replacement and stale replies, standard D-Bus signatures and filters, service
name rollback, bounded change signals, private-daemon activation/restart,
staged runtime execution, exact installed metadata, source/package poison, and
read-only startup. The metadata gate compares the complete `.portal` and
selector contracts, so duplicate Settings entries or any extra standard family
(including installed 1.20.4 Background) fail both source and staged-installed
controls. All bus tests create disposable daemons after removing host
session-bus variables; no test calls or modifies the host portal.

This proves the backend boundary and direct standard D-Bus behavior. It does
not prove that a distribution installed or selected the package, that the host
portal frontend consumed it, or that a particular third-party toolkit reacts
to changes. Those are downstream integration checks, not authority to modify
the developer's active portal state.

The durable process/protocol choice is recorded in
[ADR-0054](../adr/0054-export-appearance-through-the-standard-settings-portal.md).
