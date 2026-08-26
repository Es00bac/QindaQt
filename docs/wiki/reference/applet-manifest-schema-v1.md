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
both.

The manifest is a request, never a grant. Runtime policy must combine package
trust, user consent, host isolation, and service availability before exposing a
capability. Layout profiles may instantiate an applet but cannot expand its
authority.

## Catalog behavior

The built-in catalog lives in `data/applets`. It currently describes launcher,
task-list, global-menu, status-tray, and clock applets. Directory loading is
atomic and deterministic: malformed manifests, duplicate IDs, or incompatible
documents leave the previously loaded catalog intact.

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
