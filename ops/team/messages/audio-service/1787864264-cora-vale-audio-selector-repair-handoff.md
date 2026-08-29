# Cora Vale handoff: Audio1 focused-selector documentation repair

- **Timestamp:** 2026-08-27T20:57:44Z
- **Status:** handoff-ready for the same independent reviewer's exact-commit
  recheck; do not integrate before that verdict
- **Exact repaired commit:** `1eed5b1b93616e5527d238e0d8fc1a14b149686d`
- **Exact tree:** `a2ce4da945dcd467bb088456d3be2a668798daf4`
- **Parent:** `bd3a94e32aff5a5bd8bde737aae62e8330241734`
- **Branch/worktree:** `worker/audio1-service` at
  `/home/cabewse/work_SPaC3/container-wm-workers/audio1-service`
- **Changed path:** `docs/wiki/development/testing-harness.md` only

## Repair

The canonical focused Audio1 regex now nests both exact WirePlumber tests:
`wireplumber-(runtime|reset-lifecycle)`. An audit of repository-published Audio
selectors found this to be the sole focused selector. It now discovers exactly
the seven registered Audio1 tests, includes the mandatory reset-lifecycle
regression, and selects no unrelated test. Runtime and test source are
byte-identical to the reviewed parent.

## Exact evidence

All commands exited 0 and no rebuild was run.

- Existing Debug `build/audio-dev`: `ctest -N` exact **7**, selected **7/7**
  passed serially in 2.89 s.
- Existing Release `build/audio-release`: `ctest -N` exact **7**, selected
  **7/7** passed serially in 3.09 s.
- Existing ASan+UBSan `build/audio-sanitize`: `ctest -N` exact **7**, selected
  **7/7** passed serially in 3.84 s with leak detection and halt-on-error.
- Each configuration explicitly listed and passed
  `qindaqt.audio-wireplumber-reset-lifecycle` as test 70.
- `./tools/validate-docs`: **43** Markdown documents/navigation passed.
- `uvx --offline --from mkdocs mkdocs build --strict`: passed offline.
- `./tools/check-source-shape`: **748** files, zero skips/violations.
- Pre-commit cached whitespace, post-commit `git show --check`, exact one-path
  diff, and final empty `git status --porcelain=v1`: passed.
- Exact `/proc/*/exe` audit found zero candidate-worktree Audio test/service
  processes. No new matching private fixture root remained. Two empty
  `/tmp/qindaqt-audio-rapid-stop-*` directories have mtimes 13:48 local—more
  than an hour before this 14:55 local repair run—and were preserved as
  pre-existing state rather than deleting another lane's artifacts.

## Bounded caveat and requested action

This standalone commit repairs documentation selection only. It adds no new
runtime, test, package, hardware, UI, or resource evidence beyond the exact
accepted commands above. The same Audio1 reviewer should recheck exact commit
`1eed5b1...`, confirm the seven-test selector/clean diff, and append an exact
accept/reject verdict before manager integration.
