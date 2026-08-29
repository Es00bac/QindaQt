# Audio1 `1eed5b1` exact review: identity, selector repair, and Debug checkpoint

- Reviewer: Codex Audio1 exact reviewer (different worker)
- Time: 2026-08-27T15:10:57-06:00
- Exact candidate: `1eed5b1b93616e5527d238e0d8fc1a14b149686d`
- Exact tree: `a2ce4da945dcd467bb088456d3be2a668798daf4`
- Parent: `bd3a94e32aff5a5bd8bde737aae62e8330241734`
- State: review continues; no open finding in this exact descendant so far

## Exact scope

- `git merge-base --is-ancestor bd3a94e... 1eed5b1...`: exit 0.
- `git diff --check bd3a94e...1eed5b1`: exit 0.
- Exact diff: one insertion/one deletion in
  `docs/wiki/development/testing-harness.md`; no product/runtime source changed.
- The repaired canonical selector now includes
  `wireplumber-(runtime|reset-lifecycle)` and closes the prior selector-doc P2.
- Detached `HEAD`, tree, parent, subject, and empty tracked status independently
  match the values above.

## Source/Debug evidence

- Re-audited the run-scoped source owner: the disconnect idle is explicitly
  retained, creator-unreffed exactly at dispatch or cancel, destroyed during
  cleanup, carries `m_workerRun`, and rejects stopped/stale runs before epoch,
  operation, snapshot, core, or reconnect mutation. The deterministic private
  test pauses after attach, queues higher-priority stop, repeats two complete
  loss/stop/restart/second-loss cycles, requires exact epoch advances, and
  bounds FDs.
- Fresh strict Debug configure/build at the parent-equivalent runtime tree:
  exit 0, Ninja `607/607`; exact-descendant up-to-date check: exit 0,
  `ninja: no work to do` (the descendant is docs-only).
- Canonical focused discovery: exactly 7 tests, including both
  `audio-wireplumber-runtime` and `audio-wireplumber-reset-lifecycle`.
- Full registry discovery: exactly 90 tests.
- Canonical focused execution: **7/7 passed**, exit 0, including real private
  PipeWire reset-lifecycle proof.
- Full Debug registry: **90/90 passed**, exit 0.

Host-resource check after Debug reports 7.1 GiB available and swap nearly
exhausted, so the reviewer will retain serialized `-j1` builds for Release,
sanitizer, and production rather than increasing parallelism.
