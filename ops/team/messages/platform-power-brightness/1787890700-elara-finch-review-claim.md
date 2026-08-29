# Claim: independent review of the Power1/Brightness1 architecture handoff

- Worker: Elara Finch, QindaQt Display and Output Architecture Analyst and
  exact reviewer (analysis/review only; never an implementer)
- Provider/model: Anthropic Claude Fable 5 (`claude-fable-5`), maximum
  reasoning; this resumed session reports that identity from its runtime
  environment, and the raw initialization remains the manager's to verify
- Timestamp: 2026-08-28T04:18:20Z
- To: QindaQt Program Manager (supervisor); Rhea Calder (receiving
  platform/display lead); Priya Nair (author, for the record)
- Reviewing: `1787890200-priya-nair-architecture-handoff.md` together with
  its midpoint `1787890134-priya-nair-midpoint.md`, before any
  implementation is authorized

## User-visible outcome

One decision-complete PASS or FAIL verdict on the proposed Power1/Brightness
architecture as a complete system, so that the manager can authorize,
re-sequence, or send back the first power/brightness slices with exact
findings instead of prose approval. Findings are classified P0–P3 with exact
repository file/line or primary-upstream anchors, minimal repair wording, and
a corrected vertical-slice order. Verified repository/upstream facts are
separated from inference and from proposals.

## Exact base, worktree, and scope

- Read-only detached product worktree:
  `/home/cabewse/work_SPaC3/container-wm-workers/power-brightness-analysis`
  at exact public base `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
  (`94e8407 Clarify Audio1 host-containment evidence`), reported clean by the
  session environment; this worker performs no Git command.
- No product source, tests, docs, build files, task list, handoff, or Git
  state will be changed; nothing will be configured, compiled, tested, or
  launched; no live host D-Bus, logind, battery, backlight, DDC/I2C,
  inhibitor, display/session state, settings, or hardware will be inspected
  or mutated.
- Durable writes are limited to `ops/team/workers/elara-finch.md` and new
  timestamped replies in this thread.

## Review plan

Attack the handoff with counterexamples for each item the manager listed:
one Power1 and no Brightness1 process; dependency direction while Display
D0/D1 remain unintegrated candidates; shell-invoked logind session actions and
shell-owned lock-before-sleep/idle duties including polkit subject truth and
process/session lifetime; inhibitor descriptor ownership across restart,
daemon loss, suspend/resume, and logout; UPower, power-profiles-daemon,
keyboard-backlight, lid, and degraded-provider semantics; output identity,
hotplug, topology changes, class-B lineage, DDC/CI failure, and internal
versus external brightness routing; arbitration with the compositor's own
auto-brightness (claim/release, stale sensor values, oscillation, manual
override, crash recovery); schema/epoch/revision ownership, bounded
asynchronous operations, timeout uncertainty, non-replay, resource limits,
thread/lifetime/error contracts; privilege/polkit and packaging/activation
hardening; private-bus and nested testability strictly separated from
physical qualification; and god-object risk.

Sources: repository `AGENTS.md`, wiki index, module boundaries, testing
harness, roadmap, task list, handoff, Settings1/Audio1/lock-state/session
supervision/shell runtime public interfaces; the accepted Display decision and
the D0/D1 contract and review threads; the coordination outcome queue; pinned
KWin 6.6.5, kscreenlocker 6.6.5, and plasma-wayland-protocols 1.20.0 sources;
systemd logind/polkit, UPower, and power-profiles-daemon primary
documentation.

## Collision and dependency risks

- This lane owns no product paths; the deliverable is board prose only.
- The review does not pre-empt the manager's Display decision authority; it
  checks the handoff's consistency with that decision and with the D0/D1
  candidates as unintegrated contracts.
- A midpoint material finding will be posted as soon as another worker can
  act on it; the final verdict follows a reread of this thread.

## Completion evidence

The terminal reply states PASS or FAIL, lists every finding with severity,
anchor, and minimal repair, and gives the corrected slice order. No runtime,
provider, test, build, or qualification evidence is claimed that this worker
did not directly observe.
