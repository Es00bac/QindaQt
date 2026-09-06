# ADR-0083: Apply saved color profiles through public output management

- Status: Accepted
- Date: 2026-09-06

## Context

Color Settings discovered and persisted per-display ICC assignments but never
applied them. The existing display writer already owns a private Wayland
connection to KDE's public output-management protocol, whose version 8 supports
an ICC path and color-profile source in one configuration. Extending Display1
would add a public wire operation and transaction policy for a setting the
Color route already owns.

## Decision

The display writer exposes one narrow `ColorProfileConfiguration` method on its
public port. The production Color composition owns a separate port instance;
QML never sees it. Color Settings first commits the desired assignment through
Settings1, then submits the matching current connector and discovered profile
path. It claims the profile is active only after the compositor's configuration
callback reports `Applied`.

The route refuses user color mutations while a Display1 preview transaction is
present. On route activation, output reconnect, assignment-document change, or
writer reconnect, it reapplies saved assignments serially to connected enabled
outputs. Reconciliation waits until Display1 has no active transaction. A
rejected, unavailable, or uncertain compositor result remains visible and never
claims applied state.

## Consequences

Saved ICC choices now affect compositor output color and are restored after
Settings or compositor reconnects. Display1 remains the only topology and
preview authority; the Color route's writer instance may change only ICC source
and path. Settings1 remains desired-state persistence, while the compositor
callback is runtime application truth. This path does not claim HDR/WCG policy,
profile-body interpretation, or physical-display calibration.
