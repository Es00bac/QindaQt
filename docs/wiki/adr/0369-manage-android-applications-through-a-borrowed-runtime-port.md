# ADR-0369: Manage Android applications through a borrowed runtime port

- **Status:** Accepted
- **Date:** 2026-10-08
- **Owners:** Android Applications and Platform
- **Supersedes:** None
- **Superseded by:** None

## Context

The owner requires QindaQt/QindaTK to own Android setup/status/application
management instead of exposing a separate Waydroid desktop. The accepted
[catalog values boundary](0368-merge-managed-applications-without-desktop-key-authority.md)
provides stable registered identity but does not authenticate a runtime producer
or prove a mapped guest app. A management UI must not hide those remaining gates
by constructing stock app names, running a CLI directly or treating spawn as
Running.

## Decision

An ordinary QindaTK Android Applications window consumes one borrowed public
nonblocking GUI-thread `RuntimePort`. A separate Qt Core controller owns copied
catalog/status projection, bounded search and serialized launch intent. The
port owns source authentication, package/receipt provenance, attachment lifetime
and actual dispatch. QML owns neither process nor package/persistence authority.

Complete Android-only snapshot validation joins each current status to its exact
managed registration revision/recipe digest, with a positive snapshot revision
and canonical attachment UUID. Invalid, stale or changed same-revision values
withdraw the projection. Runtime readiness, app state and optional window count
remain independent. Only fresh port observations can change Running/counts.

One launch request carries a fresh operation UUID, attachment/snapshot revision
and exact catalog binding. Pending is installed before dispatch to handle a
synchronous result. Acceptance is pending rather than Running. Owner/recipe loss
retires the pending relationship, and late results cannot settle a fresh request.
Terminal completion triggers read-only refresh. The default 30-second deadline
reports outcome uncertainty without cancellation or mutation replay.

The manager's Android catalog badge uses QindaTK's semantic green tint with
explicit Android label/accessibility. Runtime status/error/focus remain separate.
The native management window retains ordinary geometry and close behavior;
container topology and per-surface origin remain compositor-owned.

## Consequences

The production composition deliberately uses a null port until a qualified
adapter is available. It displays unavailable state and no synthetic app rows or
actions. This slice ships source UI/public port, not an actual Android app launch,
setup/install/removal, per-app icon source, runtime recovery, live window identity,
container/input qualification or Audio1 routing. No Waydroid shell-out, VM,
package transaction or full release rebuild is introduced. The
[application page](../apps/android-applications.md) owns these next gates.

The new library is static and exposes owning typed C++ values; consumers rebuild
together. There is no new transport/persistence compatibility promise. A short
controller/offscreen/compiled smoke uses synthetic ports and temporary roots;
separate release/package gates qualify installed UI and real runtime journeys.

## Revisit when

The admitted Android adapter is ready, managed package/materialization actions
need presentation, or a public file/notification/window/audio capability can be
composed without moving its authority into this controller.
