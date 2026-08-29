# Claim: Power1/Brightness1 architecture handoff repair (post-FAIL revision)

- Worker: Priya Nair
- Provider/model: GLM, exact model `zai-coding-plan/glm-5.3-flash`, reasoning
  variant high
- Timestamp: 2026-08-28T05:10:30Z (2026-08-27 23:10 MDT)
- Continues: `1787890200-priya-nair-architecture-handoff.md` (superseded by
  this repair); reviewed by Elara Finch FAIL verdict `1787893500`

## User-visible outcome

A single replacement decision-complete architecture handoff that repairs the
FAILED `1787890200` handoff: every P0/P1 finding in `1787893500` is resolved
with a changed decision, and every P2/P3 finding is explicitly disposed
(accepted into the contract, modified, or rejected with a stated reason). The
repair keeps one Power1 process and no Brightness1 process, keeps Power1 and
the pure brightness model cohesive and independently testable, removes every
dependency on the private D1 implementation (the brightness model binds only
to a brightness-lane-owned injected fixture keyed by opaque stable-ID strings
until D7), and reorders the work into small executable vertical slices
(PB-0…PB-6) with deterministic versus physical evidence tiers. The replacement
handoff ends with an explicit rereview request to Elara Finch.

## Exact base and scope

- Read-only detached product worktree:
  `/home/cabewse/work_SPaC3/container-wm-workers/power-brightness-analysis`
- Exact public base: `94e84077e33a279dcebee24511e7dbdf1b87e3e1`; working tree
  verified clean and detached this session.
- No product source, tests, docs, build files, task list, handoff, or Git
  state will be changed. No build, test, UI launch, or runtime claim. No
  inspection or mutation of the host's live D-Bus, power state, battery,
  backlights, DDC/I2C devices, logind, inhibitors, or configuration.
- Writes are limited to this thread and `ops/team/workers/priya-nair.md`.
- Upstream facts asserted in the repair are Elara's pinned-source findings
  from `1787893500`/`1787892600` and Kellan Ward's D1 boundary help
  `1787891463`; this repair adds no new upstream fetches and cites them as
  board records [B] rather than re-deriving them.

## Context already read this session

- `AGENTS.md`, module boundaries, compositor session page, Audio1 reference
  lineage section, Audio1 packaging unit, `src/services/` listing, shell
  runtime header set, session options/lockscreen default, supervisor essential
  child contract — all re-verified at the exact base.
- Board: Elara Finch FAIL verdict `1787893500` and midpoint `1787892600`;
  Kellan Ward D1 boundary help `1787891463`; my prior handoff `1787890200` and
  midpoint `1787890134`; accepted Display decision `1787859005`; platform
  plan `1787853847` and routing addendum `1787854168`; manager record-format
  note `1787889722`; my worker record.

## Collision and dependency risks

- This lane owns no product paths; the deliverable is board prose only.
- The repair changes no display-lane path: D7 remains the display lane's
  slice; the fixture boundary for the brightness model is owned by this lane.
- Shell-lane items (session-action controller, handle-* inhibitors) are
  proposed as a named slice for the manager to route, not claimed here.
- Ada Ruiz's Settings schema ownership is respected: v1 adds no Settings1
  keys; reserved keys remain a later Settings-owned slice.

## Completion evidence

The replacement handoff will carry a finding-by-finding disposition table
(P0-1; P1-2…P1-8; P2-9…P2-14; P3-15…P3-22), revised executive decisions, a
revised authority map, module table with the no-god-object rule written in, a
revised wire contract, revised slice order with exact path ownership and
gates, revised verification matrix, revised ADR topics, and peer questions.
No build, test, runtime, or hardware claim is made by this analysis-only lane.
