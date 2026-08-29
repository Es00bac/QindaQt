# Keira Dunn — exact repair handoff for status-notifier S0 rereview

- 2026-08-28T14:43:37Z — Repair commit:
  `78725a95920880930acb55ca0f322c72b4148f17`, tree
  `fc52f584223d010bc4f3325de037ee14e974af42`, directly parented by the
  preserved reviewed candidate `637cb94ea1c2e79a6c2f541b60a64ccbbbfab54f`
  (itself parented by base `9db68c4`). One non-amended descendant on
  `worker/system-tray-s0`; `git show --check` passes; worktree clean; the
  reviewed commit is untouched.

## Findings disposition (Shannon 0/4/6/1 — all repaired)

- **P1-1** Menu validation composed into admission:
  `validateItemDescriptor` now validates `descriptor.menu` and carries an
  AGENT-GUARD naming it the single gate; `registerItem` keeps it as its sole
  descriptor check, so both boundaries reject embedded hostile menus.
  Composed tests added in both suites (over-depth and bad-parent inside an
  otherwise valid descriptor; malformed replacement degrades with
  last-known-good retention).
- **P1-2** Generation lifecycle: duplicate begin of a live name now
  rebaselines (items dropped, identity claims freed, fresh generation); old
  generations stay fenced for registration, removal, mass removal, request,
  and loss. `ownerLost(uniqueName, expectedGeneration)` refuses stale loss so
  a later generation survives. New `beginWatcherEpoch()` resets the
  population bit, so watcher (re)connection returns presentation to
  fail-closed Loading until repopulation. Explicit tests for duplicate/live
  appearance, removal/re-registration, loss/restart, stale
  registration/removal/request/loss, rebaseline, and reconnect.
- **P1-3** Watcher loss reconciled to the accepted contract without
  supersession: presentation now projects last-known-good items in Degraded
  with the watcher-unavailable diagnostic; the test requires the retained
  item and asserts the intent stays acceptable. Code, test, ADR-0032, and the
  tray page now state one behavior.
- **P1-4** ADR renamed to reserved **ADR-0032** (git-tracked rename):
  filename, ADR title, index row, `mkdocs.yml` nav, `status-tray.md`,
  `module-boundaries.md`, harness text — no `0026`-status-notifier reference
  remains (grepped).
- **P2-1** Owner history bounded (`kMaxTrackedOwners = 256`, live owners only;
  loss frees the slot); generations come from a globally monotonic quint64
  seed — never reissued after loss/eviction — and both table exhaustion and
  counter exhaustion fail closed (0) instead of wrapping. Tests: bounded
  admission, exhaustion refusal, rebase while full, slot reuse after loss,
  cross-owner uniqueness, restart monotonicity.
- **P2-2** Whitespace-only identity/title/tooltip text rejected
  (`blank-or-control-text`, `invalid-identity`); control rejection extended
  to C1 (0x007F–0x009F) alongside NUL/C0/DEL; projected items keep a nonblank
  accessible name via the locale-independent identity fallback; all
  assistive/status/keyboard strings are injected through
  `PresentationTexts` (localization boundary), with deterministic defaults
  proven independent of injected instances.
- **P2-3** Menu children must have submenu parents
  (`menu-parent-not-submenu`), with hostile child-under-item and
  child-under-separator tests.
- **P2-4** Transport now attaches to a narrow `StatusNotifierEventSink`
  (owner/item/epoch events only — no observation, request evaluation, or
  degradation acknowledgement through the seam). Non-null, no-re-attach,
  idempotent detach, sink-outlives-attachment, single-thread confinement,
  and destructor-detach obligations are written into both installed headers;
  the fake mirrors the null/re-attach refusals and a test exercises them.
- **P2-5** `evaluateRequest` returns a typed `RequestEvaluation` whose
  accepted `RequestIntent` binds the exact owner key (with current
  generation), the item identity snapshot, and the kind, with an explicit
  revalidate-before-execution lifetime documented in `status_notifier_types.h`
  and the tray page; rejection leaves the intent defaulted and tests assert
  that.
- **P2-6** Fail-closed wiring: `add_subdirectory(src/shell/status_notifier)`
  and `add_subdirectory(tests/shell/status_notifier)` are present in the two
  parent CMakeLists (reviewer-mandated addition to my previously hands-off
  shared files), and the test CMake `FATAL_ERROR`s on a missing target
  instead of silently skipping; the selector registers exactly three tests.
- **P3-1** `testing-harness.md` gains the "status-notifier foundation proof"
  section with the focused selector and its strictly source/unit-only
  stopping point; the tray page's state and lifecycle claims are enumerated
  to match the (now existing) loss and reconnect regressions; the stale
  "QML layer binds" comment is replaced with the truthful
  implementation-unavailable wording.

## Gates actually run on this exact tree

- `git diff --check`: PASS.
- `PYTHONDONTWRITEBYTECODE=1 python3 tools/check-source-shape
  --warnings-as-errors`: PASS, 1017 source files, 0 allowlisted skips.
- `PYTHONDONTWRITEBYTECODE=1 python3 tools/validate-docs`: PASS, 65 Markdown
  documents plus navigation.
- Static signature cross-check of sink virtuals vs registry overrides vs cpp
  definitions: PASS (scripted; no compiler).
- A stale-reference sweep (`ADR-0026`/`0026-status-notifier`, one-arg
  `ownerLost`, removed helper calls): clean.

## Caveats and requested reviewer/integrator action

- **No compile/CTest ran this session** — Victor owns the compiler lane and
  the assignment forbade compiler invocation; my prior candidate's offline
  compile/run evidence predates these edits. Requested: build with the
  repository warning set and run
  `ctest --test-dir build/dev -R '^qindaqt\.status-notifier-(values|registry|presentation)$'`
  (exactly three tests) plus `mkdocs build --strict` on this exact tree as
  rereview/integration evidence. `mkdocs` itself remains unavailable in this
  environment.
- The bounded-owner table (256) and the quint64 global generation seed are
  new cross-module contract values recorded in `status_notifier_limits.h`,
  the tray page, and ADR-0032.

Requesting Shannon the 2nd's exact rereview of the immutable commit above.
