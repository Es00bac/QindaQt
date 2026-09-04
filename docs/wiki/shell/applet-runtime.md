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

The manifest catalog describes clock, notification center, audio, Bluetooth,
power, launcher, task list, global menu, status tray, clipboard, and status
notifier packages.
The compiled first-party registry contains ten audited entry points. The
production QML dispatcher renders all eight hosted entry points:

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
- `qindaqt.applets.launcher` renders the compiled
  `QindaQt.Shell.Launcher` module over its shell-private composition of
  injected-root scanning, public Settings1 persistence, and seam-based bounded
  execution. The audited `applications.launch` grant gates activation, owner
  loss clears persistence truth, and a null controller leaves the preview
  visibly disabled. See [Launcher](launcher.md) and
  [ADR-0062](../adr/0062-bound-launcher-execution-behind-injected-seams.md); and
- `qindaqt.applets.global-menu` renders the compiled
  `QindaQt.Shell.GlobalMenu` module over the shell-owned AppMenu registrar and
  dbusmenu coordinator. `global-menu.read` gates observation; the explicitly
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
  utility slot adjacent to notification center. Owner loss clears stale truth,
  and Clipboard1-v1 pin requests fail closed. See
  [Clipboard applet](clipboard-applet.md).
- `qindaqt.applets.task-list` is a registered built-in with a manifest, policy
  decisions (`windows.read`, `windows.activate`, `windows.manage` granted to
  the audited package by the trust default and explicitly denied to
  third-party packages), a compiled `QindaQt.Shell.TaskList` module, and a
  shell-private controller over the injected T0 source and T1
  authority/operation seams. Production-shell dispatcher composition is the
  later hosting lane, so the production dispatcher does not render it yet;
  resolution to `ready` is covered by the applet-runtime resolver tests. See
  [Task list source model](task-list.md).
- `qindaqt.applets.status-notifier` is a registered built-in with a manifest,
  policy grants (`status-items.read`, `status-items.activate`), a compiled
  `QindaQt.Shell.StatusNotifier` module, and a shell-private controller over
  the injected `StatusNotifierSourceInterface` seam presenting the bounded,
  exact-owner StatusNotifier registry. Production-shell dispatcher composition
  lands in a follow-on lane, so the stock profile does not place the applet
  yet and the production dispatcher does not render it; resolution to `ready`
  is covered by the applet-runtime resolver tests. See
  [Status notifier tray](status-tray.md).

The notification-center, audio, Bluetooth, and power entries remain valid
compiled applets when the shell starts without presentation-token
provisioning, but the notification facade is absent and its control is
visibly disabled. Audio, Power, and Bluetooth access are independent of the
notification token and fail closed on their own capability/client state. The
Power controller additionally borrows one shell-owned session-actions client;
its Session buttons never enter the reusable applet API. Meta+L uses the
existing audited global-shortcut registrar and dispatches the same typed Lock
request as the popup.
The preview keeps deterministic static applet fixtures rather than connecting
to live clock, notification, global-menu, clipboard, audio, Bluetooth, or power
state.

## Installed shell component closure

Every install component that carries `qindaqt-shell` is independently
runnable through the shell's relative loader layout. The current inventory is
the default `QindaQt` component plus `AudioAppletRuntime`,
`BluetoothAppletRuntime`, `ClipboardAppletRuntime`, `GlobalMenuAppletRuntime`,
`LauncherAppletRuntime`, `PowerAppletRuntime`, `StatusNotifierAppletRuntime`, and
`TaskListAppletRuntime`.
Each carries every directly linked applet backing library plus
`qindaqt_controls_qml` in the install library directory and
`qindaqt_tokens_qml` in the sibling `Tokens` directory required by Controls'
baked `$ORIGIN/../Tokens` RUNPATH. A component-filtered install must not rely
on another component to supply either library.

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
accepted contracts. Task list and the newer `status-notifier` manifest resolve
`ready` as registered built-ins (the hosting lanes that render them in the
production dispatcher are still separate); the older `system-tray` resolves as
`implementation-unavailable`. Launcher and Global Menu resolve `ready` and are
rendered by the production panel dispatcher; Clipboard joins them when its
public clients expose consented ready truth. Profile plug-in IDs with
no catalog manifest resolve as `missing-manifest`. They may remain visible for
layout fidelity, but they are not counted as delivered features. The
status-notifier value, ownership foundation, and applet slice are documented in
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
