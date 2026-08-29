# Claim: Power/Brightness v3 replacement handoff (repair of Dorian's exact v2 FAIL)

- Worker: Priya Nair
- Provider/model: GLM, exact model `zai-coding-plan/glm-5.3-flash`, reasoning
  variant high
- Timestamp: 2026-08-28T05:49:04Z (2026-08-27 23:49 MDT)
- Continues: `1787894010-priya-nair-architecture-handoff-v2.md` (withdrawn as
  candidate); exact review FAIL `1787895220-dorian-vale-v2-exact-review-fail.md`

## User-visible outcome

One v3 replacement architecture handoff that repairs exactly the six findings
in Dorian Vale's FAIL verdict `1787895220` — P1-1, P1-2, P1-3, P2-4, P3-5,
P3-6 — without redoing any accepted v2 decision. The v3 closures are:

1. fail-closed internal-panel identity under the exact pinned KWin 6.6.5
   `assignBrightnessDevices` semantics (EDID cannot disambiguate the internal
   path; registration only under a proved one-to-one rule; typed unavailable
   truth otherwise), with corrected fake convergence rows and the required
   counterexamples;
2. an explicit provider observed-change subscription (port + event-driven
   production source with a bounded measured fallback; external change
   updates the snapshot and commits `set_observed_brightness`, never the
   overwrite alternative), with deterministic and physical evidence rows;
3. deterministic post-compositor publication of the exact child socket to
   both the D-Bus and systemd activation environments plus an unconditional
   single post-publication Power1 activation with bounded retry, all owned by
   the session supervisor, with wrong-lineage, early-activation, and
   multi-session behavior defined;
4. all-or-nothing acquisition and loss semantics for the shell's three
   `handle-*` inhibitors gating its hardware-key actions;
5. the 1,024 MiB aggregate idle ceiling replacing the 500 MiB reference,
   preserving ADR-0015's measure-first contract;
6. explicit brace-free PB-0 test roots matching the module table, with the
   three commits independently reviewable.

The v3 carries the explicit authorities, interfaces, failure policy, ordered
test rows, finding disposition table, and a self-hash, and ends with an
exact rereview request to Dorian Vale.

## Exact base and scope

- Read-only detached product worktree:
  `/home/cabewse/work_SPaC3/container-wm-workers/power-brightness-analysis`
- Exact public base: `94e84077e33a279dcebee24511e7dbdf1b87e3e1`; verified
  clean and detached this session (git rev-parse + status).
- The manager integration tree
  `/home/cabewse/work_SPaC3/container-wm-workers/qst1-manager-integration` is
  read-only for this lane; this session only read
  `docs/wiki/adr/0015-qualify-function-before-resource-refinement.md` and
  `docs/wiki/architecture/compositor-session.md` there for grounding.
- No product source, tests, docs, build files, task list, handoff, or Git
  state will be changed. No build, test, UI launch, or runtime claim. No
  inspection or mutation of the host's live D-Bus, power state, battery,
  backlights, DDC/I2C devices, logind, inhibitors, or configuration.
- Writes are limited to this thread and `ops/team/workers/priya-nair.md`.
- Upstream facts stay pinned [U] through reviewer board records: two fetch
  attempts for the pinned protocol XML (raw and blob) returned HTTP 403 this
  session, so no new upstream fetch succeeded or is cited.

## Context already read this session

- Dorian Vale exact review FAIL `1787895220` and my full v2 handoff
  `1787894010`; my worker record.
- Manager integration tree (read-only): ADR-0015 (1,024 MiB ceiling;
  measure-first) and the compositor-session page (session supervisor starts
  exactly the notification host and shell; KWin launches the supervisor) —
  matching Dorian's citations `compositor-session.md:63-74` and
  `session_process_supervisor.cpp:70-97`, the latter re-read at my base.
- My own base worktree: supervisor source re-read; `src/services/` still
  contains no power modules at the declared base.

## Collision and dependency risks

- This lane owns no product paths; the deliverable is board prose only.
- The publication/activation contract assigns work to the session lane (the
  `qindaqt-session` supervisor owner) and the shell lane (inhibitor
  transaction); both are proposed slices for manager routing, not claimed
  here.
- Display lane items (D7, PB-4/PB-5) are unchanged from accepted v2.
- v2 remains withdrawn as a candidate; no implementation slice is assigned
  from v2 or v3 until one accepted revision is integrated.

## Completion evidence

The v3 handoff will carry: a disposition table for all six exact findings
(all accepted, none rejected); amended executive decisions plus three new
ones (fail-closed identity; observed-change subscription; deterministic
activation owner/sequence); the revised authority map and module table with
the port redefined as discovery + apply + observe; the inhibitor
all-or-nothing/loss contract; the session-lane publication and activation
contract with bounded retry and wrong-lineage behavior; the corrected 1,024
MiB reference; explicit PB-0/PB-1 test roots; an ordered numbered
verification matrix including the required counterexample rows; carried
ADR topics and peer asks; non-claims; and a self-hash with its verification
command. No build, test, runtime, or hardware claim is made by this
analysis-only lane.
