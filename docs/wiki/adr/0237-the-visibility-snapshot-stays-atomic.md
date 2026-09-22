# ADR-0237: The visibility snapshot stays atomic, and says what it rejected

- **Status:** Accepted
- **Date:** 2026-09-22
- **Owners:** compositor (shell visibility), shell runtime
- **Supersedes:** None
- **Superseded by:** None

## Context

`ShellVisibilitySnapshot` validates a candidate generation as one batch. Any
invalid member rejects the whole candidate, the store keeps its previous
snapshot, and the shell selects safe-visible — every panel pinned visible —
until a clean snapshot arrives. That contract is stated in
[Panel visibility policy](../shell/panel-visibility.md): "One bad member
rejects the complete batch; partial visibility publication is not a supported
state." That second clause is what settles the decision below.

The live session made that expensive. The shell logged

```
QindaQt shell visibility snapshot is unavailable: a managed window is invalid or ambiguous
```

**8079 times in one session** (measured directly on the running r10 session
log, which is the shell's own stderr). The user-visible effect is auto-hide
stopping and restarting for no apparent reason.

The question is whether a transiently invalid window should be excluded from
the snapshot rather than voiding the batch. That could not be answered,
because **the message named nothing.** Eight distinct conditions shared one
string:

1. a non-canonical window identifier
2. a duplicate window identifier
3. a window naming an output this generation does not have
4. an unusable frame geometry
5. a frame geometry lying entirely outside its own output
6. an inconsistent `onAllWorkspaces` / `workspaceIds` scope
7. non-canonical activity identifiers
8. a window active while marked minimized or hidden

Two of these (5 and 8, arguably 3) are **transient races**: KWin's window state
is not atomic with respect to when the publisher samples it, so a window being
dragged between outputs or coming out of minimize is briefly inconsistent.
The other five indicate a **producer bug** — the compositor sent something
structurally wrong.

Excluding members is right for the first group and wrong for the second, and
the log could not tell them apart. Sampling the live snapshot over ten seconds
found it clean and stable (`status: ok`, 15 windows, no condition violated),
confirming the rejections are bursty and tied to window activity rather than
continuous, so the failing condition cannot be caught at idle either.

## Decision

**The batch stays atomic.** A snapshot is a coherent description of one output
generation; silently dropping members would change what "the set of managed
windows" means for every consumer, including those that count windows or look
for a maximized one, and would hide the five structural causes that represent
real producer bugs.

**Every rejection must name the member and the specific reason.** Each of the
eight conditions, the duplicate-active-window check, and the four output-level
conditions now emit a distinct message identifying the window or output by id
and stating what was wrong, including the offending geometry where that is the
complaint. Identifiers arrive off the wire, so they are quoted and length-bounded
before they reach the log.

This is deliberately the smaller of the two possible changes. Re-deciding
atomicity is a behavioural change to a documented contract, and it should be
made from evidence about which condition actually fires — which is exactly what
the previous message made impossible to collect.

## Consequences

- The rejection rate does not change. This ADR does not fix the 8079
  rejections; it makes the next session's log say which of thirteen causes
  produced them, per occurrence.
- A future decision to exclude rather than void has a factual basis. If the
  log shows conditions 5 and 8 dominate, excluding *those two* while keeping
  the batch atomic for structural causes becomes a defensible, narrow change.
  That is the expected follow-up, and it needs one live session's log.
- Messages are longer and contain window identifiers and geometry. They go to
  the session log, which already carries window identifiers.
- Covered by `compositor.shell-visibility-snapshot`
  (`namesTheRejectedMemberAndWhy`), which asserts each cause names its member,
  says what was wrong, and produces a message distinct from every other cause.

## Revisit when

One session's log with these messages is available. If transient races (a
window outside its output, or active while minimized) dominate, revisit
atomicity for exactly those conditions. If a structural cause dominates,
the defect is in the publisher and the contract was doing its job.
