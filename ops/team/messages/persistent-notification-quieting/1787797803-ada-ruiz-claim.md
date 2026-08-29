# Claim: persistent notification quieting continuation

- Worker: Ada Ruiz (OpenAI, `gpt-5.6-sol`, reasoning high)
- Timestamp: 2026-08-27T02:30:03Z
- Exact clean base: `496e5135ee4f40359f8b871eec130f0b8b02a241`
- Branch: `worker/ada-settings1`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/ada-settings1`

## Decision-complete outcome

I claim the complete active persistent-notification-quieting outcome under the
authoritative manager decisions in `1787796417` and `1787796869`, including the
UI and contract audit acceptance matrices. I will implement one independently
verified candidate in this clean worktree and will not edit, stage, commit, or
run Git in the shared checkout or Mira Chen's paused worktree.

The implementation boundary is fixed: immutable schema v1 plus active schema
v2 and explicit validated v1-to-v2 migration; generic bounded JSON-native
Settings1 protocol; independently D-Bus-activatable copy-on-write service;
exact-unique-owner asynchronous client with activation and lineage fencing;
DND-scoped shell/controller projections; fail-quiet shell baseline and
retain-last-confirmed loss behavior subordinate to lock privacy; standalone
ordinary `qindaqt-settings --page notifications`; Settings1-backed quick
toggle; and a fixed focusable settings launch action. No settings applet,
layer-shell settings UI, global shortcut, supervisor coupling, or PID/executable
attestation is in scope.

## Owned paths

- `data/settings/`, new activation/application install data
- `src/settings/`, new `src/services/settings_{protocol,service,client}/`
- new `src/apps/settings_center/`
- focused shell composition/controller/QML paths needed for the DND bridge and
  fixed route launcher
- additive build/test registries and focused tests for the outcome
- ADR-0012 and affected settings/service/shell/testing/roadmap/task/handoff docs

Shared registries will receive the smallest additive changes. The paused
`worker/mira-settings1` tree is inspection-only; any compatible idea I adopt
will be recorded as independently ported and validated, not claimed as Mira's
test evidence or candidate.

## Evidence target

Focused Debug and Release, private-bus lifecycle/client/restart scenarios,
offscreen structural UI checks, full `qindaqt.*` registry when feasible,
production build, QML lint, source-shape, strict docs/link validation,
whitespace/diff checks, and staged activation/install. Live compositor,
session-bus, lock, KGlobalAccel, assistive-technology, and injected-input claims
remain explicitly out of scope.
