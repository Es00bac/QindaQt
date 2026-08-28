# ADR-0016: Make Display1 the QindaQt display-transaction authority

- **Status:** Accepted
- **Date:** 2026-08-27
- **Owners:** Display platform services
- **Supersedes:** None
- **Superseded by:** None

## Context

Pinned KWin 6.6.5 owns live output state, persists every successful output
configuration, restores known sets after hotplug or restart, and generates
defaults for unknown sets. A preview is therefore already KWin-persistent
before the user confirms it. Reading or writing `kwinoutputconfig.json`, or
maintaining a second desired-topology store, would create two restore
authorities that can fight after hotplug.

A safe preview still needs a QindaQt owner for the pre-image, confirmation
deadline, rollback attempts, topology-change reconciliation, and restart
recovery. A Settings page or shell overlay can disappear and must not become
that owner. The accepted process and module boundary is described in
[Display service](../architecture/display-service.md).

## Decision

KWin remains the sole live and restore authority. QindaQt never reads, writes,
or infers current topology from KWin's private configuration store.

The future D-Bus-activated `org.qindaqt.Display1` process is the only QindaQt
production writer to the pinned KDE output-management protocol. It owns one
revision-fenced transaction, a monotonic confirmation deadline, the durable
pre-image journal, bounded rollback, restart recovery, and topology-settle
reconciliation. Shell, Settings Center, Color, brightness, and other consumers
use typed clients. Their countdowns and rescue controls are projections, not
timer or mutation authority.

Class-A topology changes require preview confirmation. Timeout, cancellation,
lock, suspend, or observation failure reverts; a timeout never commits. A
forward request with an uncertain outcome is never replayed. Rollback makes at
most three attempts and retains `Stuck` plus its journal when convergence
cannot be proven.

When the output set changes during a transaction, Display1 waits for an
explicit settled snapshot and reverts only mode, scale, and transform on
surviving stable identities that were enabled in the pre-image. If the original
set returns it uses the full pre-image; if survivors already match it avoids a
redundant write. It never replays the old set's enable, position, priority, or
replication relationship. Cancel, lock, suspend, and external-abandon actions
wait behind the settle barrier. A newer external configuration aborts the
QindaQt transaction without a competing write.

The D1 milestone implements only the pure protocol, topology, journal value,
and injected-clock/port state machine. It creates no service, transport
adapter, timer, file, lock monitor, logind inhibitor, or compositor mutation.
D2 must supply those adapters without changing this authority split.

## Consequences

- KWin and Display1 have distinct, non-overlapping ownership: KWin owns live
  state and restore; Display1 owns QindaQt transaction safety.
- A successful preview may remain visible after a Display1 crash until the
  service is activated and consumes its journal. Activation and durable
  storage are therefore D2 release requirements.
- Initial journal storage and final journal clearing are hard gates. Refreshes
  of an already-durable pre-image during rollback are best effort so a storage
  outage cannot suppress the rollback attempt.
- A failed confirmation clear leaves the transaction awaiting confirmation.
  Once safe live truth is known, a failed clear becomes cleanup-only
  `Stuck(JournalFailure)` and retry cannot issue a compositor mutation.
- The side-effect port must serialize each immutable apply request, preserve
  token identity, and report at most one completion. Its owner redelivers the
  live snapshot after every callback or apply deadline, even when unchanged.
  No callback is a modeled transport failure, not permission to replay a
  forward mutation.
- Callback-before-device-observation and cross-client ordering against an
  already queued apply remain D2 nested-runtime obligations. The D1 fake port
  does not prove or silently serialize those windows. The three-attempt bound
  is per rollback sequence in one process; topology settle/restart starts a
  new recovery sequence and no sequence replays the forward candidate.
- Direct KDE protocol integration is the production path. libkscreen and
  `kscreen-doctor` may be test/oracle inputs only unless a later ADR changes
  this decision.
- The exact protocol XML/checksum, service activation, lock authority, logind,
  persistence adapter, nested recovery, and physical qualification remain
  later milestones.

## Revisit when

Reconsider only if KWin exposes a public atomic preview/confirm/rollback
contract that survives process restart, or if measured D2 evidence proves the
single-writer public-protocol design cannot recover without reading private
KWin state. A UI preference or library convenience is not sufficient.
