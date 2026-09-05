# Architecture and ownership

QindaQt divides the desktop into compositor authority, shell presentation,
platform services, and ordinary applications. Pure models sit below adapters so
invariants can be tested without a live bus, device, or compositor. The
[module-boundary specification](../architecture/module-boundaries.md) is normative.

## Runtime responsibilities

| Boundary | Responsibility |
| --- | --- |
| `qindaqt-wm` and KWin plugin | Launch the pinned compositor and integrate authoritative window/container behavior in process. |
| Session supervisor | Start essential components, provision authenticated descriptors and compositor identity, and couple lifetimes. |
| Shell | Compose panels/applets, present state, and dispatch bounded user intents. |
| Resident services | Own bounded public snapshots and policy, and confine platform-specific adapters. |
| Settings service | Validate versioned preference data, persist atomically, and serialize optimistic commits. |
| Applications | Own their documents, filesystem or PTY workflows, local actions, and presentation. |
| Test harness | Create isolated sessions and evidence; test input authority is not normal production authority. |

The source pin is KWin 6.6.6 at the handbook snapshot. Native plugins must be
rebuilt for each KWin release; public layer-shell client qualification alone
cannot qualify that binary ABI. KDecoration3 handles member decoration,
LayerShellQt supplies production panel surfaces, and KGlobalAccel mediates
shell-wide shortcut registration/conflicts.

## Hybrid dependency flow

The core model owns valid container trees and persistence-neutral values.
Hybrid topology owns session-wide membership and atomic commands. Constraints
solve sizes and retain restore values. Input translates gestures into intents;
chrome computes paint/hit plans without mutating topology. KWin integration
composes these collaborators and owns actual scene changes. Shell code cannot
bypass these boundaries to edit private compositor objects.

## Service and client patterns

Public boundaries are versioned early. Clients validate complete snapshots and
track the exact service owner, epoch, and revision as appropriate to each
protocol. A reply from a replaced owner cannot become current state. Operations
are bounded, errors are typed, and timeout/restart behavior is part of the
contract. An optimistic settings commit must use the expected revision; an
uncertain operation must not be blindly replayed.

These are recurring patterns, not an assertion that every service uses the same
wire fields or authentication scheme. The [reference catalog](catalog/reading.md)
links the exact Compositor, Settings, Audio, Display, Power, Session, Bluetooth,
Network, Clipboard, notification, and portal contracts.

## Public APIs and extensions

Modules expose narrow public headers or protocols and retain implementation
headers privately. Constructors make collaborators visible. Applets declare
manifests and capabilities; audited built-ins may be hosted in process. The
accepted third-party extension architecture requires isolation, but a manifest
or SDK declaration alone does not prove a completed external-host runtime.
AppShell and Controls are reusable boundaries, not permission to reach into
application or shell internals.

See [repository catalog](catalog/repository.md) for source locations, [platform
services](platform.md) for providers, [privacy](privacy.md) for failure semantics,
and [handbook index](index.md) for the full library.
