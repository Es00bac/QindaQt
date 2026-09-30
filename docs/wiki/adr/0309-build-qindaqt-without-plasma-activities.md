# ADR-0309: Build QindaQt without Plasma Activities

- **Status:** Accepted
- **Date:** 2026-09-29
- **Owners:** Compositor and Platform
- **Supersedes:** None
- **Superseded by:** [ADR-0311](0311-remove-plasma-runtime-from-qindaqt-compositor.md)

## Context

QindaQt's compositor is a co-installable KWin fork, but Plasma Activities and
its session service are not part of the intended desktop. Leaving the fork's
feature enabled by default keeps Plasma Activities in the compositor's runtime
dependency graph and exposes menu actions whose backend is unavailable. The
affected contracts are the exact fork build and the consumer plugin's activity
scope presentation, described in [Hybrid topology](../architecture/hybrid-topology.md)
and [Compositor control](../reference/compositor-control-v1.md).

The shell visibility wire already has an activity-neutral representation:
a canonical KWin null UUID for the current activity scope and an empty
activityIds array for each window. Workspaces and outputs are independent
scopes and must remain fully available.

## Decision

Configure qindaqt-kwin with KWIN_BUILD_ACTIVITIES=OFF by default. Keep the
upstream option available for an explicit derivative build, and export the exact
fork's setting as QINDAQT_KWIN_BUILD_ACTIVITIES from its CMake package so
consumers can compile only the API their linked fork provides.

The QindaQt compositor plugin guards its Activities headers and links
Plasma::Activities only when the exact fork exports that feature as enabled.
When the inventory is absent, the group context menu omits its Activities
submenu. A direct activity mutation is rejected with an explicit unavailable
result; it is not reported successful through KWin's no-op setter. Shell
visibility keeps its null-UUID scope and empty per-window activity membership
while preserving workspace and output state.

## Consequences

A default QindaQt compositor and plugin build no longer require Plasma
Activities, while the wire schema and activity-neutral sentinel stay stable.
The Activities submenu appears only in builds with an inventory. Derivative
builds enabling Activities must install the exact matching package and provide
its include and link targets to the consumer plugin.

Private fork and consumer builds must verify the exported feature flag,
absence of Plasma::Activities in the activity-free link graph, menu omission,
explicit mutation refusal, sentinel scope round trip, and adjacent workspace
behavior. The user-facing session service and installed package dependencies
remain a separate PF14/packaging decision.

## Revisit when

Reconsider only if QindaQt adopts a native activity authority with defined
workspace interaction and persistence, or if a supported derivative needs a
documented opt-in Activities configuration.
