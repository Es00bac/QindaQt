# Devika Shah — PB-0 aggregation review repair exact handoff

- Time: 2026-08-28T07:16:47-06:00
- Owner: Devika Shah
- State: waiting; Priya exact rereview requested
- Exact commit: `30783867d7f2f49c9ad740c90f1c824614510b72`
- Tree: `0fb14c92301dd374a8b9d39859ec20f1bbf37aff`
- Parent: `cea3fb9a5b3d1a1aa8d0bc23570218ed86722f05`
- Branch/worktree: `worker/power-pb0` at
  `/home/cabewse/work_SPaC3/container-wm-workers/power-pb0`

## Exact seven-file manifest

- `docs/wiki/architecture/power-service.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/power1-v1.md`
- `src/services/power_protocol/src/power_aggregation.cpp`
- `tests/services/power_protocol/tst_power_aggregation.cpp`
- `tests/services/power_protocol/tst_power_protocol_codec.cpp`
- `tests/services/power_protocol/tst_power_protocol_values.cpp`

## Reviewer finding closure

- P2 charging estimate: unanimous `timeToFull` publishes only for a charging
  aggregate; mismatch stays unknown; canonical positive round trip and
  in-memory fixed-QtDBus positive marshalling are executable.
- P2 negative limit: both +8 MW and -8 MW exact boundaries are pinned.
- P3 exact-full arithmetic: stable summed energy is divided by stable summed
  full energy before scaling; an eight-source exponent-spread exact-full row
  publishes exactly 100 without false rejection.
- P3 precedence: adjacent pairs pin every coarse-level and warning rank in
  both enumeration orders.
- P3 lineage guard: an AGENT-GUARD now fixes epoch unification before
  opaque-ID-only dedup and states the fail-open risk.
- PB-1 malformed private-bus array behavior remains explicitly later scope and
  was not changed.

## Exact evidence

- serial focused build: 13/13 actions, pass;
- exact `^qindaqt\.(power-protocol-|power-aggregation-)` CTest: 3/3 pass;
- direct QtTest: 39/39 pass (values 14, codec 11, aggregation 14);
- source-shape: 1,011 files, zero allowlist skips;
- docs/navigation: 65 Markdown pages;
- strict MkDocs, whitespace, staged-diff, and clean-worktree gates: pass.

The QtDBus row marshals in memory without any connection or private bus. No
service, upstream daemon, session, Wayland, sysfs, hardware, display/input, UI,
or mutation runtime ran. Request Priya rereview this exact immutable commit and
route any concrete blocker back to Devika. PB-0 is not labeled accepted or
complete before rereview and manager integration gates.
