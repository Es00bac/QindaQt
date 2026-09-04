# Applet manifest schema v1

`QindaQt.Applets 1.0` packages describe their compatibility and resource needs
with a JSON manifest. The loader treats even a valid manifest as untrusted data:
validation does not approve code, grant capabilities, or select an in-process
host.

## Required fields

| Field | Contract |
| --- | --- |
| `schemaVersion` | Integer `1`. Unknown versions are rejected. |
| `id` | Stable lowercase identifier used for catalog lookup. |
| `name`, `description` | Non-empty presentation strings. |
| `apiVersion` | `major.minor`; major must match the host and the host minor must be at least the requested minor. |
| `entryPoint` | A `kind` of `builtin`, `qml`, or `executable` plus a safe non-empty `value`. |
| `placements` | At least one supported zone and orientation. |
| `sizing` | Main- and cross-axis minimum, preferred, optional maximum, and stretch intent. |
| `capabilities` | Known capability identifiers requested from host policy. |
| `settingsSchema` | Applet-local JSON Schema fragment; it is stored as data and never executed by the loader. |

Placement zones are `panel-start`, `panel-center`, `panel-end`, `panel-fill`,
and `desktop`. Orientations are `horizontal` and `vertical`. A preferred size
must not be below its minimum, and an optional maximum must not be below the
preferred size.

## Capabilities

Schema v1 recognizes narrowly named requests for application launching;
window read, activation, and management; global-menu and status-item access;
notification, audio, power, clipboard, Bluetooth, display, and settings access.
Read and control capabilities are separate where the platform service exposes
both. Power applets request `power.read` and `power.control`; the production
Bluetooth applet requests `bluetooth.read` and `bluetooth.control`. Control is
effective only with the corresponding read grant.

The manifest is a request, never a grant. Runtime policy must combine package
trust, user consent, host isolation, and service availability before exposing a
capability. Layout profiles may instantiate an applet but cannot expand its
authority.

## Catalog behavior

The built-in catalog lives in `data/applets`. It currently describes
launcher, task-list, global-menu, status-tray, clock, notification-center,
audio, Bluetooth, power, clipboard, and status-notifier applets.
Directory loading is atomic and deterministic: malformed manifests, duplicate
IDs, or incompatible documents leave the previously loaded catalog intact.

The notification-center manifest intentionally requests no capabilities.
Opening a shell-owned surface is not notification data or operation authority;
the audited in-process renderer receives only a private center-toggle/open-state
facade plus a read-only Do Not Disturb indicator from the shell. It cannot set
interruption policy. That facade is not a manifest capability and is not
available to third-party packages.

The Power manifest requests both `power.read` and `power.control`. The audited
production renderer receives a shell-private controller over the public
PowerClient. Read denial prevents client observation; control denial keeps
bounded rows visible but non-adjustable and rejects every mutation before
dispatch.

The Bluetooth manifest similarly separates read and control. Its audited
renderer receives a shell-private controller over the public BluetoothClient;
the grant cannot expose pairing/trust/keys, direct BlueZ or Agent1, addresses,
or host-radio APIs because those surfaces do not exist on the controller.

The Audio manifest requests `audio.read` and `audio.control` under the same
separation. Its audited renderer receives a shell-private controller over
the public `AudioClient`; read denial suppresses observation entirely, and
control denial keeps rows visible but refuses every mutation before dispatch.
The grant cannot expose PipeWire, WirePlumber, stream moves, default-device
changes, or service internals because those surfaces do not exist on the
controller.

The Launcher manifest requests `applications.launch`. The production shell
grants it only to the audited compiled launcher entry point, then injects a
shell-private controller whose scanner roots are explicit composition inputs
and whose process/session-bus execution routes remain behind bounded seams.
The manifest cannot choose roots, executable authority, or persistence keys.

The Global Menu manifest requests `global-menu.read` and `windows.activate`,
but the audited production policy grants only `global-menu.read` and
explicitly denies `windows.activate`. The applet receives a shell-private
facade over authenticated active-window identity and guarded dbusmenu events;
it never receives the compositor client, a general window action surface, or
registrar ownership authority. The shell, not manifest data, owns the standard
registrar name on its injected session bus.

The Clipboard manifest requests both `clipboard.read` and `clipboard.write`.
The audited renderer receives a shell-private controller over the injected
`ClipboardClientInterface` seam. Read denial withholds all observation; write
denial keeps bounded browsing visible but refuses every mutating intent
(select/promote, pin, delete, clear) before dispatch.

The Task List manifest requests `windows.read`, `windows.activate`, and
`windows.manage`. The audited policy grants all three to the audited package
through the trust default and explicitly denies `windows.read` and
`windows.activate` to third-party packages (the wildcard already denies
`windows.manage`). The audited renderer receives a shell-private controller
over the injected T0 source and T1 authority/operation seams. Read denial
withholds all observation; activation and manage denials refuse the matching
generation-fenced intents before dispatch. See
[Task list source model](../shell/task-list.md).

The Status Notifier manifest requests both `status-items.read` and
`status-items.activate`. The audited renderer receives a shell-private
controller over the injected `StatusNotifierSourceInterface` seam. Read denial
withholds all observation, including icon rendering; activate denial keeps the
bounded rows visible but refuses every intent (activate, secondary-activate,
context menu) before dispatch, and execution always stays with the item owners.

Serialization emits a normalized document suitable for round-trip and migration
tests. Field additions require either an explicitly backward-compatible minor
API rule or a new manifest schema version.

## Production resolution

The production shell loads this catalog and the installed capability policy
before it creates any panel window. Every profile instance then passes, in
order, manifest lookup, zone/orientation compatibility, host selection,
compiled built-in registration, and capability-policy evaluation. Failure at
any gate produces typed runtime metadata and no grants. A manifest entry point
is descriptive data and cannot add itself to the compiled registry.

`qindaqt-shell` accepts `--applet-dir` and `--applet-policy`; the corresponding
development overrides are `QINDAQT_APPLET_DIR` and `QINDAQT_APPLET_POLICY`.
Invalid catalogs and policies fail shell startup rather than silently falling
back. The selected profile's raw applet map is preserved and gains a `runtime`
map containing `status`, `ready`, display name, entry point, host mode, granted
capabilities, and a diagnostic. See [Applet runtime](../shell/applet-runtime.md)
for the status machine and current implementation inventory.

## Editing-time placement enforcement

One shell-customization repository copies and validates the manifest catalog at
session creation; later caller mutations cannot change that compatibility view.
For profile schema v1, top and bottom panels map to `horizontal`, left and right
panels map to `vertical`, and applet `settings.zone` maps `start`, `center`, and
`end` to the corresponding panel placement zones. An absent zone is the
canonical `start` placement.

Applet insertion and duplication always require a matching catalog manifest.
Adding a panel or changing its orientation validates every contained applet.
Moving an existing applet or changing settings requires a manifest decision
only when its orientation/zone placement signature changes. This lets an
imported legacy applet with an unavailable manifest be reordered, removed, or
moved between equivalent placements without granting it a new compatibility
claim. Operations that require an unavailable or unsupported placement fail as
`ManifestUnavailable` or `UnsupportedAppletPlacement`; the editor never treats
the manifest payload in a drag source as authority.

The hosting and trust decision is recorded in
[ADR-0002](../adr/0002-native-qindaqt-applet-api.md). Layout instantiation is
documented under [Layout profiles](../shell/layout-profiles.md).
