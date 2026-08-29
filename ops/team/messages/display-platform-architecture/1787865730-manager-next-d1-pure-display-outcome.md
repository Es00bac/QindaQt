# Manager next Display outcome: D1 pure protocol, identity, topology and transaction

- **Timestamp:** 2026-08-27T15:22:10-06:00
- **From:** Manager
- **To:** future Display D1 implementer and exact-candidate reviewer
- **State:** queued after accepted Audio1 public integration; no branch,
  implementation or worker liveness is claimed
- **Required base:** the future exact public Audio1 milestone
- **Authority:**
  [accepted Fable decision](1787859005-manager-fable-display-decision.md),
  amended over the underlying
  [analysis handoff](1787858968-elara-finch-fable-analysis-handoff.md)

## User-visible enabling outcome

QindaQt has one deterministic, transport-free model for bounded display
snapshots, persistent output identity, candidate validation/geometry, and safe
preview/confirm/revert state. The later Display1 service, client, shell overlay,
and Settings page must consume these values rather than reimplementing display
policy around KWin, D-Bus, Wayland, QML, or files.

## Exact ownership and dependency boundary

Own:

- `src/services/display_protocol/**`
- `src/services/display_identity/**`
- `src/services/display_topology/**`
- `src/services/display_transaction/**`
- matching `tests/services/display_{protocol,identity,topology,transaction}/**`
- new `docs/wiki/architecture/display-service.md`
- new `docs/wiki/reference/display1-v1.md`
- ADR-0015 for Display1 transaction authority and ADR-0016 for persistent
  output identity
- smallest additive source/test/MkDocs/ADR registries

The modules may use Qt Core; protocol serialization may additionally use Qt
DBus. They may not import KWin or Wayland headers, QML, Settings1
implementation, shell/app types, filesystem persistence, a real clock/timer,
logind, libkscreen, or any provider QObject. D1 does not create a service,
D-Bus name, activation file, journal file, protocol XML, client, page, shell
overlay, or compositor mutation.

## Required contracts

1. Fixed typed protocol values carry a protocol version, service epoch,
   monotonic revision and bounded outputs/candidates/transactions. Every list,
   string, mode, scale, coordinate and serialized payload has an explicit
   hostile-input limit. Decoding is total and fail-closed; malformed input
   cannot partially replace a caller's prior value.
2. Persistent identity follows the accepted precedence: unique EDID identifier
   hash, then unique raw-EDID hash, then unique MST-path composite, then a
   bounded documented fallback. Connected duplicates are explicit ambiguous
   values, never silently conflated. Published IDs contain no serial, raw EDID
   or other un-hashed private hardware material.
3. Identity and alias-registry values are pure. Connector rename/hotplug can
   reconcile an accepted stable ID without treating compositor UUID as a
   persistent identity. Collision, ambiguity and registry-schema errors are
   typed and deterministic.
4. Topology validation covers enabled/primary invariants, modes, scale,
   transform transposition, mirroring/self-reference, overlap, gap warnings,
   KWin's coordinate bound, normalization to a `(0,0)` origin, exact logical
   rounding parity, canonical fingerprints, candidate diffs and no-op
   detection. It must not infer current topology from stored preferences.
5. The transaction state machine uses an injected monotonic clock and injected
   side-effect port. It owns one active transaction, base-revision fencing,
   stage/apply/observe/confirm/cancel/deadline/revert, typed rejection and
   uncertain outcomes, journal values, three bounded revert attempts, and
   `Stuck` recovery truth. It never retries an uncertain forward mutation.
6. Hotplug during preview waits for a deterministic settle input and reverts
   only surviving per-output properties; it never reapplies old per-set
   enable/position/priority/replication state to a changed output set. External
   newer intent aborts without fighting it. Lock/suspend inputs revert; service
   recovery consumes the same journal state-machine path.
7. Class-A changes require confirmation. Any class-B bypass must be an explicit
   closed policy value with tests; it cannot grow into a generic flag bag.
   Ownership, value lifetime, threading, errors, compatibility and every
   injected port pre/postcondition are documented for future service authors.

## Acceptance evidence

- Protocol unit/property/fuzz-style hostile rows prove every bound,
  round-trip, stable canonical encoding, invalid enum/NaN/infinity/overflow
  rejection, old/new version handling, and no partial output.
- Identity rows cover unique serial, duplicate identical displays without
  serials, malformed/empty EDID, unique/duplicate raw EDID, MST paths,
  connector rename, compositor-UUID change, precedence parity, registry
  migration/ambiguity, collision suffixing and privacy.
- Topology rows cover overlap, gaps, all-disabled, primary rules, coordinate
  bounds/normalization, 90/270 transforms, mirror cycles/self-reference,
  integral-extent warnings, candidate diff/no-op, and the accepted fractional
  logical-size table including 2560×1440 at 150% → 1707×960.
- Deterministic fake-clock/port rows cover every state and failure edge:
  stale stage, apply reject/timeout, observation mismatch/timeout, confirm,
  cancel, deadline, lock, suspend, external change, no-op, topology churn,
  surviving-output-only revert, each retry, `Stuck`, crash-journal recovery,
  and invalid callback ordering. Every rejected transition preserves the exact
  prior state unless its contract explicitly enters recovery.
- Strict-warning Debug and Release focused plus broad registries, sanitizer
  focused tests, source-shape/ownership/static-dependency gates, strict
  documentation/link/navigation/whitespace checks, and process cleanup pass.
- A different worker reviews the exact candidate before manager integration.

No nested or physical-output claim belongs to D1. M0/D0/D2 supply those later;
unit fakes must be labelled deterministic model evidence rather than proof that
the pinned KWin protocol accepted a real configuration.
