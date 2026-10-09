# ADR-0366: Coordinate ordinary Windows launches through public ports

- Status: Proposed
- Date: 2026-10-09
- Owners: Windows product delivery
- Related: [ADR-0352](0352-foreign-runtime-identity-and-installation.md),
  [ADR-0275](0275-qindalutris-installs-games-and-manages-pinned-proton.md)

## Context

The existing QindaLutris launch planner is game-shaped and private to that app.
Ordinary registered Windows applications need the same fixed Wine/Proton
environment rules without depending on game records, stores or shell-private
runtime code. A successful spawn cannot establish a running application,
authenticated window identity or independent prefix ownership.

## Decision

Add the public Qt Core `QindaQt::WindowsRunner` module. Its compact Windows
binding carries a canonical UUID, current registration revision and immutable
recipe digest. It produces the stable `qindaqt.managed.windows.<UUID>` catalog
identity, and receives admitted recipe/materialization state through a borrowed
registration port. Catalog adapters admit typed origin separately.

Separate payload verification and process dispatch into borrowed public ports.
A payload lease must own stable package/payload handles, verify exact pins and
private profile receipts, and remain current through dispatch. The coordinator
compares all pins and rechecks the complete registration after inspection.
The process port consumes the checked handles, uses literal argv and the
complete sanitized environment, and submits at most once per caller operation.
Accepted yields Starting; uncertain dispatch yields OutcomeUnknown with no retry.

No force-stop, profile deletion, game installer, runtime discovery or process
authority is added to the core. A broker retains the protocol's deduplication
ledger. A Proton process adapter refuses until its leased exact Steam Runtime
and Proton tuple is executable without runtime downloads or path reopening.

## Consequences

The core can be independently tested and consumed without a display, bus or
QindaLutris private include. Existing QindaLutris is unchanged; it can adopt
the neutral boundary in a later owning change. Public catalog adaptation,
installed recipe/materialization and stable-handle launch adapters are explicit
remaining work, alongside real ordinary app, window, audio and menu journeys.
This is a source boundary, not proof of a delivered Windows desktop service.

Focused validation and delivery details live in
[Public ordinary Windows application runner](../architecture/windows-runner.md).
