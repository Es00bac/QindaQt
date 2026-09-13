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

Before starting a long integration or documentation gate, and again before a
checkpoint report, the Program Manager reads the latest authored handoff or
verdict for every executing lane and services all completed outcomes in one
pass. A completed reviewer is returned to its preserved compatible
implementation session before the manager waits on long gates; a stale ledger
label never keeps an accepted candidate queued or a worker idle. Each lane has
one concrete executable successor rather than a reservation for an unpublished
candidate, and an active implementation is not repeatedly preempted merely to
make board labels look current.

A blocking review finding must demonstrate a product defect, a consequential
violation of an accepted contract, or invalid required verification. Optional
refactoring, unrelated coverage, and pre-existing unrelated defects are
nonblocking follow-ups. Repair review checks the exact repair, the original
reproduction, and the affected regression boundary; it does not restart an
unchanged broad audit without evidence of wider impact. A failed mandatory
gate receives one bounded causal investigation before retry, and an identical
long retry requires a changed hypothesis or input.

Long integration gates run asynchronously with one published session identifier
and one exact integration boundary. The Program Manager does not stop, restart,
or duplicate that gate merely to await it; after recording the retained session,
the manager ends a bounded checkpoint so queued candidate, verdict, failure, and
capacity events can be serviced. A later checkpoint resumes that same session
before recording the gate result.

Candidate handoffs, review results, stopped working processes, and live idle
capacity with backlog use a separate event path with a scan interval of at most
15 seconds. It reads handoff and reviewer-verdict evidence from each
assignment's explicit current worktree and message-thread mapping as well as
the manager ledger, so neither a candidate nor an ACCEPT/REJECT notification
depends on prior ledger mutation. A raw reviewer verdict transitions only the
exact candidate declared in its own heading or candidate field; mentioning the
review worktree's HEAD in a routing note is not verdict evidence. The five-minute watchdog uses the same
mapping when copying current records/messages; rotated worktrees must not be
overwritten by a historical lane directory. If the mapped worker has not yet
published its stable record, the board removes the prior occupant's cached
declaration and reports the record as missing while retaining independent
process observation. Event identity includes its type,
worker, exact candidate or outcome, and stable lifecycle transition; a later
handoff by the same worker is therefore new while an unchanged scan is not. New events in one scan are
coalesced into one bounded manager wake. Delivery state is written atomically,
failed queue attempts remain pending for retry, and resolved failures are
removed. A rejected candidate stays in the review ledger as evidence; once an
exact reviewed descendant resolves it, the manager records that descendant in
`resolvedBy`, and the ancestor stops emitting a current action while the
descendant carries its own review and integration state. This product path does
not inherit the slower warning-reminder
cooldown; the five-minute watchdog remains the backstop for stale records and
missed events. The dashboard reports evidence-to-queue and queue-call latency,
omits legacy pre-await timing rows that cannot support those measurements, and
separates queued delivery from manager acknowledgement and integrated,
verified, and installed/adopted states.

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

Compatible CMake configurations may share a task-scoped `ccache` directory
through explicit C and C++ compiler launchers. Keep compiler-content checking
and normal header validation enabled; do not use sloppiness, ignored headers,
hard links, or copied mutable build roots. Preserve one warm integration build
per compatible configuration, while every worker and reviewer retains its own
source and build tree and still runs the required independent gates.

Interactive worker processes require a direct terminal. Do not pipe an
interactive Codex or Claude process through `tee` or another stdout relay:
Codex can fail without a TTY and Claude can continue with a blank visible
terminal. Launch long-running worker terminals with a lifetime independent of
the Program Manager (for example, a transient user-systemd service), record the
actual worker and terminal PIDs, and preserve the provider session identifier
before rotating a lane. Print-mode workers may use captured output when their
provider supports it. A stopped launcher is not a completed handoff; inspect
the final transcript, exact worktree state, and authored handoff evidence first.

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
