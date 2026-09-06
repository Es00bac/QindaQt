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
screencast, remote desktop, or a consent dialog. Its `.portal` file advertises
only `org.freedesktop.impl.portal.Settings`. The module owns QindaQt's frontend
selection file, but it does not replace, embed, or supervise
`xdg-desktop-portal`; non-Settings calls remain another backend's authority.

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
and `qindaqt-portals.conf`. The selector binds
`org.freedesktop.impl.portal.Settings` only to `qindaqt`, explicitly orders
`kde;gtk;lxqt` for the supported non-Settings fallback families, orders
GlobalShortcuts to `kde` alone because it is the only installed provider whose
`.portal` metadata advertises that interface, disables Background, and uses
`default=none` so an unreviewed family cannot silently escape the table. OpenURI is implemented by the frontend itself and therefore
has no backend selector. The exact table is part of the
[Settings backend v1 reference](../reference/portal-settings-backend-v1.md),
following upstream [portal selection rules](https://flatpak.github.io/xdg-desktop-portal/docs/portals.conf.html).

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
staged runtime execution, exact installed metadata/singletons, source/package
poison, and read-only startup. Two P1 rows run the real installed
`xdg-desktop-portal` on a private `dbus-run-session` bus with only staged portal
metadata. They prove QindaQt selection for the `qindaqt` desktop, exact
frontend `ReadAll`/`Read` values and live forwarding, rejection under another
desktop, injected KDE FileChooser and GlobalShortcuts fallback routing (after
confirming the installed KDE backend's own `.portal` metadata still advertises
`GlobalShortcuts`), and the closed Background escape. The Qt row runs an offscreen Qt 6 process with
`QT_QPA_PLATFORMTHEME=xdgdesktopportal`, observes Dark then a live Light
`QStyleHints::colorScheme()` change, and applies a palette derived from that
hint. It does not claim that Qt replaces an application's explicit palette.

The metadata gate compares the complete `.portal` and selector contracts, so
duplicate entries, QindaQt ownership of a non-Settings family, or a reopened
Background/default route fail both source and staged-installed controls. The
staged-package row repeats both frontend rows against the installed artifacts.
All rows remove host session-bus variables, reserve the auxiliary portal names
with injected test fakes, and use build-local runtime/config/data directories;
they neither contact nor modify the host portal or host D-Bus services.

This proves package selection and Qt reaction on the private bus. It does not
qualify an installed desktop, a host session bus, GTK/GSettings or Flatpak
sandbox reaction, a real chooser UI, or any non-Settings portal implementation.

The durable process/protocol choice is recorded in
[ADR-0054](../adr/0054-export-appearance-through-the-standard-settings-portal.md).
The fail-closed fallback routing policy is recorded in
[ADR-0059](../adr/0059-route-unimplemented-portal-families-explicitly.md).
