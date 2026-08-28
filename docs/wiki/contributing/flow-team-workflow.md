# Flow team workflow

QindaQt uses a results-driven delivery loop derived from direct inspection of
a completed sibling project's repository evidence rather than from a generic
multi-agent plan. The observed high-throughput day contained hundreds of
first-parent integrations, more than a thousand durable message replies, and
over a hundred preserved employee records. The useful mechanism was not the
volume: it was prompt handoff, exact review, repair/rereview, integration, and
immediate capacity refill around whole outcomes.

This page owns the QindaQt adaptation. Product architecture and acceptance
standards remain QindaQt-specific.

## Organization boundary

- The Program Manager owns priorities, collision and resource arbitration,
  the integration branch, combined-tree verification, and product evidence.
- Stable Shell, Platform, and First-party workgroup managers own their durable
  queue. They dispatch complete outcomes, connect peers, chase exact next
  gates, and keep near-finished work moving.
- A worker owns one whole testable deliverable and its repair loop. An
  assistant may return bounded research, reproduction, tests, implementation,
  documentation, or review; the worker remains accountable.
- A persona's name, role, provider/model, and reasoning level are immutable for
  its lifetime. A changed tuple is a new person with a preserved handoff.
- Implementers use isolated branches/worktrees. Only the Program Manager edits
  the integration branch.

## Continuous delivery loop

1. A workgroup manager assigns a user-visible outcome with acceptance
   evidence, exact base, path ownership, and prohibited boundaries.
2. The worker reads its queue and relevant message threads, claims the work,
   starts immediately, and records material evidence while working.
3. Help is direct and bounded. A peer posts the exact path/line, reproduction,
   test, or artifact; the accountable owner decides and integrates it within
   the owned worktree.
4. The worker hands off one immutable candidate commit with changed paths,
   commands and exit status/counts, caveats, and the requested next action.
5. A different worker attacks that exact commit. A real defect goes directly
   to the implementer with its reproduction; the same reviewer remains
   assigned to the repaired descendant.
6. The Program Manager integrates a passing candidate immediately, reruns the
   affected combined-tree gates, and only then reconciles the outcome ledger.
7. After handoff or review, the worker reads the queue and peer threads, claims
   the next compatible outcome or offers concrete help. No accepted result
   waits for managerial ceremony and no live capacity waits for invented
   microtasks.

The manager repeats: set a clear outcome, watch real evidence, remove an
obstacle, connect peers, integrate finished work, refill safe capacity.

## Durable queue contract

Each row in `ops/team/queues/{shell,platform,first-party}.md` records:

- roadmap step and current integrated evidence state;
- accountable owner;
- exact candidate or base and isolated worktree;
- independent reviewer;
- next executable gate;
- shared path, compiler, nested-session, bus, hardware, or other collision;
- concrete help requested or offered; and
- last observed timestamp.

Use `unclaimed` and `none` explicitly. A queue row is not progress. Candidate
branches, reviews, messages, and estimates add zero until the accepted behavior
is integrated and its stopping point is recorded in `features.json`.

## Liveness and board truth

Every provider uses the same self-owned Markdown record. `working` requires an
observed live process executing the named outcome and a parser-valid update no
more than 30 minutes old. Waiting, assignment, completed handoff, or fresh prose
without a process is not liveness.

All durable employee records are visible; the roster is not an allowlist. The
Program Manager enforces the 15-live-process ceiling and private-runtime
serialization from direct evidence. Build-only work may proceed in separate
worktrees with separate build roots and bounded parallelism when host headroom
is measured. Private compositor, D-Bus, hardware, and input fixtures stay
serialized when their namespaces could collide.

## Failure correction

The workflow preserves useful work and makes management errors visible. Never
discard a candidate because a reviewer or plan changed direction. If a worker
stalls repeatedly, simplify or reassign from observed results and preserve the
exact branch and handoff first. If coordination costs more than the product
work it enables, remove the ceremony while retaining ownership, review, and
truth gates.

The board parser and tests enforce visibility and freshness. The message board
and queues preserve organizational memory across agent sessions; no persona is
expected to remember an earlier conversation.
