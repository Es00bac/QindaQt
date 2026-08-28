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

The manifest catalog describes clock, notification center, launcher, task list,
global menu, and status tray packages. The compiled first-party registry and
production QML dispatcher currently contain two audited entry points:

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
  operations.

The notification-center entry remains a valid compiled applet when the shell
starts without presentation-token provisioning, but its facade is absent and
the control is visibly disabled. The preview keeps deterministic static applet
fixtures rather than connecting to live clock or notification state.

Launcher, task-list, global-menu, and status-tray manifests remain accepted
contracts but resolve as `implementation-unavailable`. Profile plug-in IDs with
no catalog manifest resolve as `missing-manifest`. They may remain visible for
layout fidelity, but they are not counted as delivered features. The
status-tray value and ownership foundation is documented in
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
