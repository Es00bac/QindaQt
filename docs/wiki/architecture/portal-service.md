# XDG desktop portal backends

QindaQt provides a production, read-only XDG desktop-portal backend for its
appearance policy. Sandboxed applications and toolkits continue to call the
standard `org.freedesktop.portal.Settings` frontend owned by
`xdg-desktop-portal`; QindaQt implements the desktop-specific
`org.freedesktop.impl.portal.Settings` backend selected by that frontend.

The upstream [backend authoring contract](https://flatpak.github.io/xdg-desktop-portal/docs/writing-a-new-backend.html)
requires an activatable executable, a `.portal` declaration, and backend
selection configuration. The exact method and signal contract is pinned to the
[xdg-desktop-portal 1.20.4 Settings XML](https://github.com/flatpak/xdg-desktop-portal/blob/1.20.4/data/org.freedesktop.impl.portal.Settings.xml),
which matches the installed development dependency. The user-visible standard
keys and value meanings are defined by the upstream
[Settings portal documentation](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Settings.html).

## Responsibility and non-goals

The appearance libraries in `src/services/portal` own one narrow result: exporting confirmed QindaQt
appearance truth through the standard Settings portal. It has three internal
layers:

| Layer | Owns | Does not own |
| --- | --- | --- |
| Appearance policy | Exact Settings1 field validation, theme selection, QST-1 derivation, and standard value projection | D-Bus, activation, persistence, or theme discovery |
| Appearance source | A four-key, exact-owner/epoch Settings1 subscription and atomic ready/unavailable truth | Settings files, Settings1 service implementation, or UI |
| Appearance adapter | Standard Settings marshalling, filtering and change signals | Persistence or Secret policy |
| Resident composition | Service ownership, activation, bus-loss shutdown, separately borrowed Secret adaptor | Secret storage, app identity or prompt policy |

The appearance module does **not** implement a chooser, OpenURI, notifications, inhibit, screencast, remote desktop, secret storage or a consent dialog. The resident additionally composes the separate [native Secret module](secret-portal.md) and [native portal foundation](portal-foundation.md); its ordinary `.portal` declares thirteen families, with five sharing/input families owned by the separate protected broker as described below. The module owns
QindaQt's frontend selection file and keeps an explicit routing decision for
every portal family ([ADR-0133](../adr/0133-route-every-portal-family.md)),
but it does not replace, embed, or supervise `xdg-desktop-portal`; a routing
row names another backend's authority, and each routed family remains its owning module/backend's responsibility.

## Standard endpoint

The [native chooser components](../reference/portal-choosers.md) keeps
FileChooser/AppChooser wire, request lifetime and separate ordinary GUI outside
the appearance module. Their actual private frontend/native-input qualification
passed all advertised methods before their selector rows moved to QindaQt.
Unavailable native choosers fail closed without compatibility fallback.

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
| `accent-color` (`(ddd)`) | `appearance.theme`, `appearance.colorScheme`, QST-1, and `accessibility.reducedTransparency` | active theme's derived QST accent, converted to opaque sRGB components in `[0,1]` |
| `contrast` (`u`) | `appearance.theme`, `appearance.colorScheme`, and `accessibility.highContrast` | 1 when the accessibility input is true or the active theme is `high-contrast`; otherwise 0 |

The portal deliberately does not read the legacy
`appearance.accentColor` schema field. QST-1's active theme plus explicit
accessibility inputs are the first-party rendering truth; exporting a separate
unused color would make sandboxed applications disagree with QindaQt itself.
Reduced transparency participates only because QST derivation can turn a
nonopaque theme source into an opaque final accent. If the final QST accent is
invalid, non-finite, outside sRGB, or nonopaque, the complete projection is
unavailable.

The *active* theme is `appearance.theme` (the chosen base selection) resolved
through the same `QindaQt::AppAppearance::resolveAppearanceTheme` call the
Settings preview, shell, and first-party applications use
([ADR-0284](../adr/0284-themes-name-their-light-and-dark-twins.md)): with a
dark twin active, `accent-color` and `contrast` reflect the twin, not the
chosen base theme's own values. The portal has no platform-level scheme
signal of its own, so resolution always runs with an unknown platform scheme;
an explicit `dark`/`light` `appearance.colorScheme` still resolves its twin
fully, and `system` keeps the plain selection. Installation is checked
against the exact `appearance.theme` id before resolution runs, so an unknown
selection still fails the projection outright rather than silently
substituting a fallback theme's accent.

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
public policy/source headers and libraries, six built-in QST themes, the
D-Bus activation descriptor, a hardened user systemd unit, `qindaqt.portal`,
and `qindaqt-portals.conf`. The native selector assigns thirteen ordinary
families to `qindaqt`, and five sharing/input families to `qindaqt.capture`:
Screenshot, ScreenCast, RemoteDesktop, InputCapture and Clipboard move together.
Wallpaper and Background remain closed, and `default=none` prevents implicit
fallback. OpenURI belongs to the standard frontend and has no backend selector.
The exact table is in the [Settings backend v1 reference](../reference/portal-settings-backend-v1.md).
[ADR-0342](../adr/0342-route-native-portals-to-their-owning-process.md)
supersedes the old KDE routing/drop-in choice; source selection still requires
coherent native, package and installed qualification.

The ordinary resident composition exports none of the five sharing/input
adaptors and no longer constructs an ordinary ProcessCapture. Its older capture
constructor parameter remains source/link compatible and is ignored. The
protected broker publishes `qindaqt.capture.portal` only when the fixed public
capture contract is available; no standalone activation or ambient capture
permission is added. The old KDE backend identity drop-in is no longer installed.

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
desktop, native FileChooser failure without a selected attachment and zero KDE
chooser calls, and injected KDE GlobalShortcuts fallback routing (after
confirming the installed KDE backend's own `.portal` metadata still advertises
`GlobalShortcuts`), and the closed Background escape. Two more P1 rows stage
fake `kde` and `gnome-keyring` backends behind the real frontend and prove
that Screenshot, ScreenCast, RemoteDesktop and InputCapture
requests reach the routed fake backend while Secret resolves only to the native resident, that Wallpaper and Background
stay unexported, and that removing the Secret routing row withdraws the Secret
interface (negative control). The Qt row runs an offscreen Qt 6 process with
`QT_QPA_PLATFORMTHEME=xdgdesktopportal`, observes Dark then a live Light
`QStyleHints::colorScheme()` change, and applies a palette derived from that
hint. It does not claim that Qt replaces an application's explicit palette.

The metadata gate compares the complete `.portal` and selector contracts, so
duplicate entries, QindaQt ownership of a family beyond Settings/Secret/Access/Notification/Email/FileChooser/AppChooser, a rerouted or
dropped Secret/Wallpaper row, or a reopened Background/default route fail both
source and staged-installed controls. The
staged-package row repeats every frontend row against the installed artifacts.
All rows remove host session-bus variables, reserve the auxiliary portal names
with injected test fakes, and use build-local runtime/config/data directories;
they neither contact nor modify the host portal or host D-Bus services.

This proves package selection, non-Settings routing to declared backends, and
Qt reaction on the private bus. It does not qualify an installed desktop, a
host session bus, GTK/GSettings or Flatpak sandbox reaction, a real chooser
UI, or any other portal implementation. The independent [native chooser
gate](../reference/portal-choosers.md) supplies actual private native-dialog
evidence; physical and sandbox journeys remain manager gates. Real-backend reachability is
evidenced by the recorded headless smoke
(`tests/services/portal/proof/private-portal-proof.sh`): it starts a virtual
KWin, a private PipeWire stack, the real KDE portal backend, and the real
frontend, and its run stops at the real screenshot consent dialog, which needs
a human click; the runtime traps it hit are noted in
[gabbee interop evidence](../development/gabbee-interop-evidence.md).

The durable process/protocol choice is recorded in
[ADR-0054](../adr/0054-export-appearance-through-the-standard-settings-portal.md).
The fail-closed fallback routing policy is recorded in
[ADR-0059](../adr/0059-route-unimplemented-portal-families-explicitly.md).

The secret-bearing resident disables cores/dumpability before requests. Its activation lifetime fixture retains PIDFDs for private-daemon observed owners, so exit/restart verification and cleanup cannot target a recycled PID and do not require readable `/proc/PID/exe`. No production dumpability exception is introduced for tests.

The [native capture module](../reference/portal-capture.md) composes separate Screenshot/ScreenCast policy, request/session lifetime and native helper ports. The current selector routes Screenshot, ScreenCast, RemoteDesktop, InputCapture and Clipboard to the protected native broker. Five actual native GPU/PipeWire/consent/input/clipboard journeys pass with zero failures or skips in the frozen release source; the historical monitor-only slice below is not the current routing boundary. Final installed activation and physical fresh-login adoption remain release gates. The shared public pixel transport is owned by [CompositorCapture](compositor-capture.md), with no Screenshot app-private includes.

## GlobalShortcuts migration candidate

A separate native GlobalShortcuts v1 adaptor/helper owns
CreateSession, BindShortcuts, ListShortcuts and activation/deactivation. It
adapts existing session admission and uses the compositor's native Shortcuts1
service; metadata routing is held pending tests and real compositor
qualification. Its transient bindings disappear on actual lifetime loss. See
[ADR-0334](../adr/0334-native-shortcut-authority-and-consumers.md) and
[portal foundation](portal-foundation.md).
