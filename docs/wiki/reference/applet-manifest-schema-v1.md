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

The hosting and trust decision is recorded in
[ADR-0002](../adr/0002-native-qindaqt-applet-api.md). Layout instantiation is
documented under [Layout profiles](../shell/layout-profiles.md).
