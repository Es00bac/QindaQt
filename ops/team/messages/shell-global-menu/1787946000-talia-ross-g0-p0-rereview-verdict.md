# Talia Ross — exact Global Menu G0 P0-repair rereview verdict

- **Timestamp:** 2026-08-28T19:40:00Z
- **Verdict:** **PASS**
- **Severity:** **P0=0, P1=0, P2=0, P3=2** (both carried over, unchanged,
  non-blocking)
- **Exact candidate:** `dfd916b1e015d1f8b9076c058ca7270fba2f3f35`
- **Tree:** `5f8c4d02c49f43b7e46817a7126a164ffb3e772d`
- **Sole parent:** `53490b748b90e6fe492eb15a85a5ec5805756ef4` (my own exact
  FAIL, P0=1)
- **`origin/main` (current public tip):** `146fc48358c2659436dec4fc6b6062d23c5ee746`
  (unchanged since my last review — no new commits landed)
- **Local `main`:** `c4982697858c083828bd406f1aa56c4e942bcc10` (unchanged)

The review worktree stayed detached at the exact candidate SHA/tree
throughout; no product/Git mutation, GUI/session/bus/input/config, or host
interaction occurred. All builds ran under the worktree's own `build`
symlink target, `/mnt/d/QindaQt/builds/global-menu-g0-review-talia` — the
correct isolated tree for this permanent role, not the old `/home` private
build root. No stash cycle was needed this round.

## P0-1 closed — independently reproduced, not just test-trusted

Ran my own fresh `cmake --preset dev -DCMAKE_AUTOMOC_PATH_PREFIX=ON` (adding
the global cache default on top of Aria's per-target `AUTOMOC_PATH_PREFIX ON`
property, per this round's explicit build instruction) and the equivalent
`release` preset, both with `QINDAQT_ENABLE_STRICT_WARNINGS=ON`
(`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror`,
GCC 16.1.1), building exactly the ten registered global-menu library/test
targets in each configuration:

- **Debug:** 61/61 build steps, **zero warnings, zero errors.**
- **Release:** 61/61 build steps, **zero warnings, zero errors.**

**Negative control.** To confirm the strict-warning gate is genuinely live
and that this fix — not an incidental flag/toolchain change — is what closes
it, I extracted the exact live compiler invocation for
`menu_validation.cpp` via `ninja -t commands` and reran it verbatim (same
flags, same GCC 16.1.1) against the pre-fix `53490b7` source **and** header
pair, staged in a scratch directory outside the worktree
(`/tmp/talia-negative-control`, never written into the candidate tree). It
reproduced the exact original failure — 6 errors,
`-Werror=missing-field-initializers` on `ValidationResult::reasonCode`/
`::path` at `menu_validation.cpp:89,137,160` — byte-identical to my prior
`53490b7` verdict. This proves the gate genuinely fires on the old code and
the candidate's in-class default member initializers
(`menu_validation.h:16,19` and the four sibling structs: `AuthenticationResult`,
`ExportResult`, `MenuSnapshot`, `MenuItem`) are what closes it, not a
weakened flag set.

**Public include/AUTOMOC wiring.** `src/shell/global_menu/applet/CMakeLists.txt`
now adds `target_include_directories(qindaqt_global_menu_applet PUBLIC
$<BUILD_INTERFACE:.../include> $<INSTALL_INTERFACE:...>)` alongside
`AUTOMOC_PATH_PREFIX ON`. Building with `-DCMAKE_AUTOMOC_PATH_PREFIX=ON` set
globally in addition to that per-target property produced no collision or
duplicate-flag issue in either configuration — moc generation for
`globalmenuappletaccess.h` succeeds cleanly through the public include path
in both Debug and Release.

## All 10 registered gates — independently rerun, both configurations

```
Debug:   105-114  100% tests passed, 0 tests failed out of 10  (1.65s)
Release: 105-114  100% tests passed, 0 tests failed out of 10  (1.74s)
```

Per-suite counts (Debug, run directly, not just via ctest) match Aria's
self-report exactly:

| Suite | Count |
|---|---|
| MenuProtocolTests | 23 passed |
| MenuOwnershipTests | 16 passed |
| MenuLineageTests | 17 passed |
| MenuExporterTests | 11 passed |
| QMenuBarMenuSourceTests | 15 passed |
| GlobalMenuAppletAccessTests | 14 passed |
| MenuCompositionTests | 7 passed |

One caveat for my own record, not a candidate defect: invoking
`qindaqt_global_menu_qt_widgets_adapter_tests` directly without the
`QT_QPA_PLATFORM=offscreen` environment variable that its `add_test`
registration sets (`tests/shell/global_menu/qt_widgets_adapter/CMakeLists.txt`)
hangs/crashes waiting for a real display — expected QtWidgets behavior, not
a regression. Confirmed 15/15 passing the moment the same env var ctest uses
is set by hand.

The three QML offscreen suites (behavior/overflow/accessibility) were
actually **executed** this round under `qmlscene`-based offscreen CTest in
both configurations (112/113/114, all Passed) — closing the gap from my
prior review, where I only static-parsed them via `qmlformat -n` because the
C++ half could not build.

## Unchanged findings — rechecked, not stale-copied

- **`python3 tools/check-source-shape`:** exit 0, 1051 checked, 0 skipped;
  same **P3-1** WARNING as before
  (`tst_GlobalMenuAppletOverflow.qml` at 296 non-blank lines, threshold 275)
  — unchanged because `dfd916b` touches no QML files (diff is scoped to
  `src/shell/global_menu/{protocol,ownership,exporter,applet}` and
  `tests/shell/global_menu/{composition,exporter,qt_widgets_adapter}` C++/
  CMake only). Still non-blocking.
- **`python3 tools/validate-docs`:** exit 0, 65 Markdown documents plus nav,
  unchanged. No doc update was required for this candidate: AGENTS.md's
  "documentation is part of the change" rule applies to
  behavior/architecture/schema/interface changes, and this repair is a
  narrow, behavior-preserving mechanical fix (aggregate initializers,
  one CMake property/include fix, `[[nodiscard]]`/fixture-parentage test
  fixes) — confirmed no runtime behavior differs by re-tracing each touched
  return site against its prior value.
- **`git diff --check`** from public base `d168e95^` and exact parent
  `53490b7`: exit 0, no whitespace defects, both rechecked.
- **`qmlformat -n`** on all four previously-touched QML files (unchanged by
  this candidate): 4/4 exit 0, still byte-identical to their formatted
  state.
- **`mkdocs build --strict`:** not attempted — not on `PATH` in this
  sandbox, same as my prior review; not claimed either round.
- **`origin/main` collision:** rechecked via `git merge-tree origin/main
  dfd916b` — identical outcome to my `53490b7` review: two benign textual
  CONFLICT markers in `docs/wiki/adr/index.md` and `mkdocs.yml` (additive
  same-shape insertions at different positions), `src/CMakeLists.txt`/
  `tests/CMakeLists.txt` merge clean. `origin/main` has not moved
  (`146fc483`, same tip as before) since my last review, so this is the
  identical **P3-2** already on record, not a new collision. Manager will
  still need the same trivial manual reconciliation against the current
  public tip at integration time.

## Scope boundary re-tightened

The diff touches only files under `src/shell/global_menu/{protocol,
ownership,exporter,applet}/` and `tests/shell/global_menu/{composition,
exporter,qt_widgets_adapter}/` — the exact module this role owns, no
adjacent-module or cross-boundary edits. I did not read or use Aria's
separate uncommitted `worker/global-menu-qml-followon-preservation-aria`
branch bytes.

## Exact static/dynamic evidence summary

- SHA/tree/parent/detached-clean before and after: **4/4 PASS**, no stash
  cycle needed.
- Strict-warning Debug build: **61/61, 0 warnings/errors — PASS.**
- Strict-warning Release build: **61/61, 0 warnings/errors — PASS.**
- Negative control (pre-fix source+header, live flags): **reproduces the
  exact original 6-error P0-1 — confirms the gate and the fix.**
- Registered focused C++ CTest gates (protocol, ownership,
  ownership-lineage, exporter, qt-widgets-adapter, applet-access,
  composition): **7/7 build and pass, both configurations.**
- QML offscreen suites (behavior/overflow/accessibility): **3/3 executed
  and pass, both configurations** (previously blocked; now actually run,
  not just parse-checked).
- `check-source-shape` / `validate-docs` / `git diff --check` / `qmlformat`:
  all rechecked, same results as before (P3-1 persists, non-blocking).
- `origin/main` collision: **identical benign P3-2**, `origin/main`
  unmoved.
- Worktree byte-clean at exact HEAD `dfd916b` before and after; removed the
  untracked `.omc/` (confirmed local review-harness tooling, not candidate
  bytes) immediately before this final clean-tree proof.

## Requested next action

**Accept for integration.** No P0/P1/P2 remain. The two P3s are pre-existing,
non-blocking, and already known to the manager (P3-1 since `53490b7`; P3-2
tracks a moving `origin/main`, not a candidate defect, and needs only a
trivial manual doc-list reconciliation at merge time, not another review
round). This is my second and final exact review of this lineage; no further
rereview is needed unless a new descendant lands.
