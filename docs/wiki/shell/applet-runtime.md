# Applet runtime

Production panels must distinguish a profile request, a valid package
description, authorized runtime code, and a working user-facing applet. QindaQt
does not treat any one of those as proof of the others.

## Resolution pipeline

The production shell resolves each applet instance before constructing panel
QML:

1. find an exact ID in the atomically validated manifest catalog;
2. validate the profile zone and panel orientation against that manifest;
3. select an allowed host from the declared entry-point kind and package trust;
4. require the entry point in the build's audited implementation registry; and
5. evaluate every requested capability against the installed policy.

Only an instance that passes all five gates receives `ready: true`. Granted
capabilities contain only affirmative policy decisions and are sorted for
deterministic consumers. A denied optional capability does not invent authority
or necessarily suppress safe presentation; service adapters must consult the
grant set before exposing privileged operations.

The first-party path deliberately identifies packages as audited built-ins.
Third-party package identity, consent, sandbox creation, process lifecycle, and
surface embedding belong to the separate out-of-process hosting milestone.
Requests that select a sandbox host therefore fail closed as
`sandbox-unavailable` today.

## Runtime descriptor

The original profile instance is retained and receives a nested `runtime` map:

| Field | Meaning |
| --- | --- |
| `status` | Stable typed resolution result |
| `ready` | True only for `ready` status |
| `displayName` | Validated manifest presentation name when available |
| `entryPoint` | Validated entry point after manifest lookup |
| `hostMode` | Host-selection result; rejected failures never imply execution |
| `grantedCapabilities` | Least-authority policy grants for this instance |
| `diagnostic` | Non-empty reason for an unresolved instance |

Statuses are `ready`, `missing-manifest`, `placement-rejected`,
`host-rejected`, `sandbox-unavailable`, `implementation-unavailable`, and
`policy-rejected`. Production QML visibly marks unresolved instances instead of
allowing their static profile label to masquerade as live behavior.

## Current built-ins

Shell composition publishes QST-1 and installs the selected freedesktop icon
runtime before this inventory is instantiated. Every hosted entry has an
explicit icon-first declaration guarded by `qindaqt.shell-icon-coverage`.
Buttons retain their accessible name, role, enabled state, and keyboard path
when visible labels are removed; unresolved icon names fail closed to the
typed placeholder. Clock and provider-owned global-menu labels are the two
intentional textual panel surfaces.

The manifest catalog describes clock, notification center, audio, Bluetooth,
network, power, launcher, task list, global menu, status tray, clipboard, status
notifier, start menu, desktop icons, and the thirteen desktop-control
packages. The compiled first-party
registry contains thirty audited entry points. The production QML
dispatcher renders the twenty-nine panel-hosted entry points, and the desktop
surface hosts the desktop-icons entry (see its bullet below):

- `qindaqt.applets.clock` renders local time, follows the locale by default,
  supports 12/24-hour overrides and optional seconds/date, and works on
  horizontal or vertical panels; and
- `qindaqt.applets.notification-center` renders a dedicated open/close button
  on either orientation. Its manifest has an empty capability set. When the
  authenticated presentation runtime is provisioned, QML receives only a
  shell-owned facade that can request a center toggle and observe whether the
  center is open plus read-only Do Not Disturb state. The applet shows a moon
  indicator and includes that state in its accessible label, but cannot change
  interruption policy, read notification records, or invoke notification
  operations; and
- `qindaqt.applets.power` renders exact-owner Power1 battery truth, bounded
  profile choices, and keyboard-brightness controls on either orientation. Its
  shell-private controller consumes only the public `PowerClient`; `power.read`
  gates observation and `power.control` gates serialized mutation. Owner loss
  or replacement clears prior truth and ends a pending request without replay.
  PB-1 currently supplies honest unavailable truth until its platform
  collaborators land, so the production applet remains visibly unavailable
  rather than inventing host state; and
- `qindaqt.applets.audio` renders bounded Audio1 device and application-stream
  truth and only capability-admitted volume and mute controls. Its
  shell-private controller consumes only the public `AudioClient`;
  `audio.read` gates observation and `audio.control` gates serialized
  mutation. Owner loss or replacement clears prior truth and pending work
  without replay. Until the platform WirePlumber adapter is the activated
  backend, the resident service reports honest unavailable/degraded truth,
  which the applet presents as-is.
- `qindaqt.applets.bluetooth` renders bounded adapter/device truth and only
  capability-admitted adapter power, one caller-scoped discovery lease, and
  paired-device connect/disconnect. Its shell-private controller consumes the
  public `BluetoothClient`, fails closed on owner/epoch/revision loss, releases
  discovery on close/teardown, and exposes no address, pairing, trust, Agent1,
  BlueZ, or audio authority. Bluetooth B0 currently reports the deterministic
  empty backend, so production truth remains unavailable until the platform
  adapter lands; and
- `qindaqt.applets.network` renders the current wired, Wi-Fi, or mobile
  connection from public Network1 truth, a Wi-Fi switch, visible networks, and
  Rescan. Its shell-private controller consumes only the public
  `NetworkClient`; `network.read` gates observation and `network.control`
  gates the typed intents. An action reports success only when a newer
  same-owner snapshot confirms it; owner loss clears every row and ends a
  request as uncertain without replay. It has no credential surface
  ([Network applet](network-applet.md),
  [ADR-0258](../adr/0258-network-panel-applet-over-public-network1.md)); and
- `qindaqt.applets.launcher` renders the compiled
  `QindaQt.Shell.Launcher` module over its shell-private composition of
  injected-root scanning, public Settings1 persistence, and seam-based bounded
  execution. The audited `applications.launch` grant gates activation, owner
  loss clears persistence truth, and a null controller leaves the preview
  visibly disabled. See [Launcher](launcher.md) and
  [ADR-0062](../adr/0062-bound-launcher-execution-behind-injected-seams.md); and
- `qindaqt.applets.global-menu` renders the compiled
  `QindaQt.Shell.GlobalMenu` module on horizontal or vertical panels over the
  shell-owned AppMenu registrar and dbusmenu coordinator; its manifest admits
  the start, center, and fill zones, and stock profiles place it only on top
  panels. `global-menu.read` gates observation; the explicitly
  denied `windows.activate` capability grants no arbitrary window control.
  The composition borrows the shell's one exact-owner window-actions client
  for authenticated active-window identity, and owner loss clears recursive
  menu truth and closes activation authority; and
- `qindaqt.applets.clipboard` is a registered built-in with a manifest, policy
  grants (`clipboard.read`, `clipboard.write`), a compiled
  `QindaQt.Shell.ClipboardApplet` module, and a shell-private controller over
  the injected `ClipboardClientInterface` seam presenting public Clipboard1
  truth only after exact Settings1 user consent. The production dispatcher
  hosts its keyboard-capable history popup and every stock family places one
  utility slot adjacent to notification center; the Bliss profile is the
  deliberate exception and ships no clipboard slot
  ([ADR-0124](../adr/0124-add-qindaqt-bliss-luna-option-set.md)). Owner loss
  clears stale truth,
  and Clipboard1-v1 pin requests fail closed. See
  [Clipboard applet](clipboard-applet.md).
- `qindaqt.applets.task-list` is a registered built-in with a manifest, policy
  decisions (`windows.read`, `windows.activate`, `windows.manage` granted to
  the audited package by the trust default and explicitly denied to
  third-party packages), a compiled `QindaQt.Shell.TaskList` module, and a
  shell-private controller over the injected T0 source and T1
  authority/operation seams. Both production and preview dispatchers host the
  compiled module on horizontal and vertical panels. Production borrows the
  shell's sole exact-owner `CompositorShell1` client for generation-fenced
  activate, minimize/unminimize, close, and raise requests; owner loss clears
  prior task truth and no uncertain request is replayed. See
  [Task list source model](task-list.md).
- `qindaqt.applets.status-notifier` is a registered built-in with a manifest,
  policy grants (`status-items.read`, `status-items.activate`), a compiled
  `QindaQt.Shell.StatusNotifier` module, and a shell-private controller over
  the injected `StatusNotifierSourceInterface` seam presenting the bounded,
  exact-owner StatusNotifier registry. The shell composition owns the S1
  watcher service and monitor adapter on its session bus and routes the
  degradation acknowledgement through the seam; the production dispatcher
  hosts the strip on both orientations with keyboard-capable `Popup.Window`
  context menus, and every stock family places one tray slot beside its
  notification-center utility slot. See
  [Status notifier tray](status-tray.md).
- `qindaqt.applets.start-menu` renders the compiled
  `QindaQt.Shell.StartMenu` module: a green Luna start button, sized to its
  icon and bold italic label, opening a
  two-column start panel popup with launcher sections left and places plus
  system actions right. Its `applications.launch` grant rides the same
  audited launcher seams, and the stock-profile invariant resolves exactly
  one menu slot per profile that is either a launcher or this applet.
  `settings.variant` selects which Windows start menu the instance
  reproduces — `luna` (the default, and Bliss) or `modern`, the Windows 11
  centred card; see [Layout profiles](layout-profiles.md#tile-shapes-and-start-menu-variants)
  and [ADR-0224](../adr/0224-name-the-presentations-a-layout-asks-for.md).
  The program list is **lazy**: the projection's sections are flattened into
  one array whose index is the flat program index, and a `ListView` with
  native sections draws it, so opening the panel builds a viewport of rows
  rather than the whole catalogue. `qindaqt.start-menu-qml` pins the budget
  (16 of 336 rows over a 364 px viewport; a nested `Repeater` built all 336,
  each resolving an icon through the theme, which is what made the panel feel
  sluggish). The panel is placed by
  [`PanelPopup`](panel-popup-placement.md). See
  [ADR-0124](../adr/0124-add-qindaqt-bliss-luna-option-set.md); and
- `qindaqt.applets.desktop-icons` hosts on the dedicated per-output desktop
  surface from `QindaQt.Shell.DesktopSurface` instead of a panel: it presents
  the user's real Desktop-directory files and folders as desktop icons,
  listed through the least-authority `DesktopContentsController` seam over File
  Manager's public `FileBoundary`. Regular files dispatch to the configured
  handler through `launchLocalFile`; folder tiles open QindaQt File Manager
  through the identity-fenced `openLocalFolder` boundary rather than a shell reach-through (see
  [module boundaries](../architecture/module-boundaries.md)), with
  configurable left/right default placement and icon size. The desktop is ONE
  desktop across every output
  ([ADR-0167](../adr/0167-one-desktop-across-every-output.md)): a single
  shell-owned layout store keeps one global-coordinate placement per
  filesystem identity, so rename retains placement and no entry is ever drawn
  twice. Each per-output surface draws only the icons whose centre its output
  owns, icons nobody has placed flow onto the primary output, and an icon over
  no output at all falls back to the primary rather than becoming unreachable.
  Icons stay inside each output's **work area** - the output minus the
  exclusive zones the shell's own panels currently reserve on it, which the
  runtime publishes after every accepted panel plan - while the surface
  itself still spans the whole output
  ([ADR-0261](../adr/0261-lay-desktop-icons-out-inside-the-panel-work-area.md)).
  Unplaced icons flow below the top bar and beside side panels, a saved
  position under a panel is drawn just clear of it without rewriting the
  store, a hidden or overlay panel reserves nothing, and a panel or output
  change reflows the icons live.
  A tile is dragged 1:1 with the pointer, a multi-icon drag is clamped as one
  rigid group, and a drag may cross an output seam - the dragging surface
  publishes volatile live positions through the shared store so the receiving
  output draws the icon before the drop, and only the drop itself is
  persisted. A dropped icon settles on the nearest free grid cell
  (`snapToGrid`, default true) and glides there; placement animations are
  suppressed until the surface has a real size, so nothing flies in at
  startup.
  Each tile has its own Open/Rename context menu, and rename crosses only File
  Manager's public identity-fenced mutation boundary. The surface also owns a right-click
  desktop context menu in `windows`, `mac`, or `traditional` style whose
  XFCE-style Applications menu sits behind a configurable modifier key. An
  explicitly styled `Templates.Menu` owns a content-model `ListView` and a
  positive implicit size before it opens. This is a Wayland process-safety
  contract, not only presentation: committing a zero-width or zero-height
  popup makes the compositor disconnect the shell. The offscreen desktop
  surface row therefore checks both dimensions before and after a physical
  right-click dispatch in addition to checking the menu's visible contents.
  An
  background context popup and the Applications popup are parented to
  pointer-positioned one-pixel anchors, because Wayland positions a window
  popup from its parent xdg anchor rather than from `Popup.x`/`Popup.y`. An
  unmodified middle click on empty desktop space also opens the same
  Applications popup directly at the pointer, in any style, without opening
  the context menu, clamped so it stays fully inside the surface near the
  right/bottom edges; the modifier-gated and traditional-style-entry paths
  keep the popup's own fixed bottom-left placement. It fails closed (no
  popup) when the borrowed launcher facade is absent, and a middle click or
  middle double-click over an icon tile stays inert rather than activating
  the tile or opening any menu. The menu's mac-style "Open" entry opens the
  Desktop folder itself through the borrowed `PlacesController` facade; the
  settings-labeled entries — mac-style "Change Desktop & Screen Saver…",
  traditional "Desktop Settings", and Windows-style "Display Properties" —
  dispatch the installed `org.qindaqt.Settings` entry's `appearance` and
  `display` desktop actions through the borrowed launcher facade, opening
  the Appearance or Display route each label promises rather than the
  primary Settings launch. Every style ends with "Edit Panels", which enters
  panel edit mode through the borrowed live customization facade (disabled
  without it; ADR-0266).
  Folder creation goes through the least-authority `NewFolderController`
  seam, which writes only under the user's Desktop directory; the manifest
  requests only `applications.launch` and no new capability enum. See
  [ADR-0125](../adr/0125-host-desktop-zone-applets.md) and
  [ADR-0161](../adr/0161-persist-and-mutate-desktop-icons.md); and
- The desktop-control entries are registered built-ins with compiled
  `QindaQt.Shell.DesktopControls` implementations: active application,
  application tiles, command HUD, command palette, dashboard, overview,
  Places, quick launch, show desktop, system menu, system status, workspace
  switcher, and workspace tiles. Each presentation consumes only its borrowed
  shell facade; application tiles activate the bounded pinned launcher
  projection, quick launch (the dock, ADR-0265) also holds
  `windows.read` and `windows.activate` for its running indicators and to
  bring a running pinned application forward, while command-menu rows carry
  their rendered publication generation through activation. Workspace operations re-read compositor
  state after completion so property writes converge even when a change signal
  is absent. See [Desktop controls](desktop-controls.md) and
  [ADR-0075](../adr/0075-desktop-controls-and-workspaces.md).

Desktop-zone instances take a separate hosting path: the shell resolves them
through `AppletInstanceResolver::resolveDesktopBuiltin` and hosts the
resolved set on one shell-owned background-layer desktop surface per output,
between the wallpaper and ordinary windows. The five resolution gates, typed
statuses, and capability-policy evaluation are identical to the panel path —
the zone changes where an instance renders, never what it may do — and the
audited registry remains the only route into the zone
([ADR-0125](../adr/0125-host-desktop-zone-applets.md)).

The notification-center, audio, Bluetooth, network, and power entries remain valid
compiled applets when the shell starts without presentation-token
provisioning, but the notification facade is absent and its control is
visibly disabled. Audio, Power, Bluetooth, and Network access are independent of the
notification token and fail closed on their own capability/client state. The
Power controller additionally borrows one shell-owned session-actions client;
its Session buttons never enter the reusable applet API. Meta+L is owned by
KWin's ksmserver "Lock Session" component — the shell registers no competing
global lock shortcut, and every QindaQt lock button dispatches the same
typed Lock request through session_actions
([ADR-0132](../adr/0132-finish-session-locking.md)).
The preview keeps deterministic static applet fixtures rather than connecting
to live clock, notification, global-menu, clipboard, task-list,
status-notifier, audio, Bluetooth, network, or power state.

Both shell executables publish the selected theme into their engine-owned
`QindaQt.Tokens` facade before constructing the dispatcher. Hosted applets may
therefore treat `Tokens.ready` and every QST-1 role as startup invariants. A
publication or republish failure terminates the shell with an explicit error;
continuing with undefined Controls bindings is not an available mode.

## Installed shell component closure

Every install component that carries `qindaqt-shell` is independently
runnable through the shell's relative loader layout. The current inventory is
the default `QindaQt` component plus `AudioAppletRuntime`,
`BluetoothAppletRuntime`, `ClipboardAppletRuntime`, `GlobalMenuAppletRuntime`,
`LauncherAppletRuntime`, `PowerAppletRuntime`, `TaskListAppletRuntime`, and
`StatusNotifierAppletRuntime`.
Each carries every directly linked applet backing library plus
`qindaqt_controls_qml` in the install library directory and
`qindaqt_tokens_qml` in the sibling `Tokens` directory required by Controls'
baked `$ORIGIN/../Tokens` RUNPATH. A component-filtered install must not rely
on another component to supply either library.

Every shell-carrying component also installs the compiled Clipboard, Task List,
Status Notifier, and Icons module directories (`qmldir`, typeinfo, QML, and
backing/plugin artifacts) because `BuiltinAppletContent.qml` imports the first
three and `AppletChip.qml` imports Icons unconditionally. The closure probe
rejects any module missing from an isolated component stage and inventories
each literal shell-carrying component declaration.

`qindaqt.shell-runtime-component-closure` installs each member of that
inventory alone beneath the active build root, authenticates both resolved
library paths, and launches the staged shell with ambient loader, display,
Wayland, and session-bus variables cleared. A new shell-carrying component is
incomplete until it is added to this inventory and passes the same proof.

Every new applet module imported or linked into the production shell must also
be added to the data-driven `DesktopVirtual` applet-module inventory in
`tests/session/DesktopVirtualAppletModules.cmake`. The non-nested
`desktop.virtual.stage-closure` row must pass before a nested desktop is used as
evidence; it rejects a missing linked library and a missing imported-module
`qmldir` without waiting for shell readiness to time out.

Task-list and the older `system-tray` (status-tray.json) manifests remain
accepted contracts. Task List and the newer `status-notifier` manifest resolve
`ready` and are rendered by the production panel dispatcher; the older
`system-tray` manifest now selects the same compiled status-notifier implementation. Launcher, Global Menu,
and Clipboard also resolve `ready` and are rendered by the production panel
dispatcher. Profile plug-in IDs with
no catalog manifest resolve as `missing-manifest`. They may remain visible for
layout fidelity, but they are not counted as delivered features. The
status-notifier value, ownership foundation, applet slice, and production
hosting are documented in
[Status notifier tray](status-tray.md).

## Startup and failure behavior

`qindaqt-shell` discovers installed manifests and the capability policy under
the QindaQt data prefix. `--applet-dir`/`QINDAQT_APPLET_DIR` and
`--applet-policy`/`QINDAQT_APPLET_POLICY` are explicit development or packaging
overrides. A malformed or absent required catalog or policy aborts startup
before panel surfaces are created. Runtime resolution never executes manifest
content and never mutates the selected profile.

The pure `src/applet_runtime` module owns resolution. `src/shell` converts its
descriptors to QML data; QML does not repeat trust, host, or capability policy.
The compiled registry and QML renderer inventory are separate fail-closed
gates and must remain aligned through focused tests. A shell-private facade is
not part of `QindaQt.Applets 1.0` and must not be exposed to third-party hosts.
The architectural decision is in
[ADR-0002](../adr/0002-native-qindaqt-applet-api.md), and manifest fields are in
[Applet manifest schema v1](../reference/applet-manifest-schema-v1.md).

## Production rendering recovery

The production shell defaults to Qt Quick's single-threaded `basic` render
loop; an explicit `QSG_RENDER_LOOP` overrides it for diagnosis. This mitigates
the render/window-lifetime path seen in the September 7 shell crash, but does
not establish that its underlying memory fault is fixed. Other applications
and the compositor retain their own rendering choices. See
[ADR-0104](../adr/0104-serialize-shell-rendering.md) for evidence and limits.
