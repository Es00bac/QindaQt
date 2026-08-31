## Exact verdict (rereview) — Mary Jackson — Display D6 repair descendant

- Candidate: `9a7872aec60a5e0f8286b3d5af7fa21209e8fd65`
  (tree `5fe0e5f22f600de24398f77b25fbf36afba08178`)
- Sole parent / merge base: `0dc3c64e5a4a4cb182cec77f77c1b89f0a42c3b9`
  (my prior rejected candidate — confirmed by `git merge-base` and
  `git log -1 --format=%P`)
- Changed paths: 12; 381 insertions(+), 55 deletions(-) — matches
  James Clerk Maxwell's repair handoff
  (`ops/team/messages/display-durable-journal/1788168202-james-clerk-maxwell-d6-repair-handoff.md`,
  read from the shared manager path) exactly, confirmed via
  `git diff --numstat`/`--stat` against the cited parent.
- **Verdict: ACCEPT — for this exact descendant `9a7872aec6...` only.**
- P0: 0 · P1: 0 · P2: 1 · P3: 1

### Prior P1 #1 — RESOLVED, independently reproduced fixed

The prior candidate's `ResidentDisplayService::inventoryObserved()` called
`m_model->transportLost()` on any `Rejected` `observeInventory()` result,
destroying a live D2 machine on benign same-owner stale/dirty reads. This
descendant instead routes on `DisplayServiceModel`'s own
`InventoryObservationResult::stateChanged`, which `establishLineage()` now
sets precisely: `false` for every same-owner rejection (regressed,
changed-equal, unchanged-new, invalid-projection), `true` only when a
replacement-owner establishment actually fails after already revoking the old
lineage. Read the full diff of all three touched files
(`display_service_model.h`/`.cpp`, `resident_display_service.cpp`) —
`git diff --numstat` confirms nothing was truncated in my review. No
`reasonCode` string-matching was introduced; the fix is a clean boolean-flag
contract, matching the header's own updated doc comment.

Independently verified, not just read:
- Fresh Debug build (`-DQINDAQT_BUILD_KWIN_PLUGIN=OFF
  -DQINDAQT_BUILD_PRODUCTION_SHELL=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON`):
  1921/1921 targets, 0 compiler errors, 0 compiler warnings.
- Fresh Release build, same flags: configure clean; build stopped only on the
  known pre-existing, unrelated `shell_customization_editor` GCC-15
  `-Werror=maybe-uninitialized` false positive (see P2 below) — 2 errors, 0
  warnings, both in that one file; a `-k 0` rebuild confirmed it is the
  *only* broken target in the whole tree.
- The new `tst_resident_display_service_inventory_rejection_private_bus.cpp`
  (16 data rows: 4 rejection classes × 4 live states from Staged through
  AwaitingConfirmation, asserting byte-identical snapshot/view/owner/
  generation/lineage/apply-store-clear-counts/zero-signals, plus a genuine
  unavailable contrast, plus a dedicated failed-replacement-owner test) ran
  directly on both configurations: **Debug 19/19, Release 19/19** QtTest
  assertions passed, matching the handoff's claim exactly.
- `qindaqt.display-service-resident-inventory-rejection-private-bus` also
  passed inside the full selector run on both configurations (see below).

### Prior P1 #2 — RESOLVED, independently reproduced fixed

`tests/services/display_runtime/CMakeLists.txt` now passes a narrow
`MODULE_SOURCE_DIR` (`src/services/display_runtime`) to
`check_boundary_negative.cmake` instead of the whole `CMAKE_SOURCE_DIR`; the
guard now rejects only a stage nested inside the module itself, and the
poison-staging `file(COPY ...)` copies just that module. `check_boundary.cmake`
(the positive detector) was untouched — confirmed by `git diff --stat` (not
among the 12 changed paths) — so only the negative test's wiring changed, not
the underlying detection logic, which I separately confirmed correct in the
prior review.

Independently verified:
- `qindaqt.display-runtime-boundary-poison` now **passes** on an ordinary
  in-tree build directory (`build/mj-verify`, `build/mj-verify-release`) —
  the exact layout that failed unconditionally before, and the exact layout
  this project's own CI uses.
- Ran `check_boundary_negative.cmake` manually with an explicit
  `-DSTAGE_DIR=/tmp/...` outside the repo entirely, against both a Debug and
  a Release build's module root: exit 0 both times ("Display runtime source
  boundary rejects public poison") — confirms the fix also preserves
  out-of-tree behavior, not just in-tree.
- `qindaqt.display-runtime-installed-boundary-poison` (package-boundary,
  distinct from the source-boundary poison above) also passed on both
  configurations.

### Documentation and configure-warning repairs — confirmed accurate

- `docs/wiki/development/testing-harness.md` gained a "D6 authenticated
  Display1 process composition" section with the exact
  `ctest --test-dir build/dev --parallel 1 --output-on-failure --no-tests=error
  -R '^qindaqt\.display-runtime-'` selector and the combined D0-D6+session-lock
  selector, matching every sibling D-slice's format. `display-service.md`,
  `display1-v1.md`, and ADR-0053 were all updated to state the corrected
  same-owner-preservation contract; I checked each updated paragraph against
  the actual code behavior and found no inaccuracy.
- The "WaylandClient target ... not declared" CMake configure warning is
  gone: `find_package(Qt6 ... WaylandClient)` was added to both
  `src/services/display_runtime/CMakeLists.txt` and
  `tests/services/display_runtime/CMakeLists.txt` (the two directories that
  finalize an executable linking the static `DisplayWriter` graph). Grepped
  full configure logs for "WaylandClient", "Wayland::Client", and
  "Wayland::Cursor" on fresh Debug and Release configures: zero matches in
  either.

### 12-path audit

`git diff --stat` lists exactly the 12 paths named in the repair handoff, no
more, no fewer. No `tests/session/**` or `src/shell_customization_editor/**`
path appears in the diff — confirmed directly, not just by trusting the
handoff's own claim.

### Fresh gates run (this rereview, independent of the handoff's own numbers)

```sh
cmake -S . -B build/<cfg> -G Ninja -DCMAKE_BUILD_TYPE=<Debug|Release> \
  -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=OFF \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake --build build/<cfg>
ctest --test-dir build/<cfg> --output-on-failure --no-tests=error \
  -R '^qindaqt\.(display-(protocol|identity|topology|transaction|service|client|writer|journal|runtime)-|session-lock-)'
ctest --test-dir build/<cfg> --output-on-failure --no-tests=error \
  -R '^qindaqt\.display-runtime-'
./tools/validate-docs
python -m mkdocs build --strict
./tools/check-source-shape --largest 20
```

- Debug adjacent D0-D6+session-lock selector: **40/40** passed.
- Debug D6-only selector: **7/7** passed (source boundary, nested poison,
  installed poison/package, private-bus runtime, safety, state-root,
  process-startup).
- Release adjacent D0-D6+session-lock selector: **40/40** passed.
- Release D6-only selector: **7/7** passed.
- `./tools/validate-docs`: 109 Markdown documents + nav validated, exit 0.
- Strict `mkdocs build --strict` (pinned `mkdocs==1.6.1` from
  `docs/requirements.txt` in a throwaway venv, system packages untouched):
  exit 0, no warnings, including whatever the new D6 doc section added.
- `./tools/check-source-shape`: exit 0, 1,617 sources checked; only the same
  three pre-existing warnings outside D6 (`tst_color_model.cpp`,
  `desktop_session_runtime.py`, `desktop_session_topology.py`) — none in the
  12 changed paths.
- Provenance/ancestry: `git merge-base` confirms `0dc3c64...` as the sole
  parent; `git diff --check` on the descendant clean (no whitespace/conflict
  markers); tree object `5fe0e5f2...` valid.
- Cleanliness/residue: `git status --porcelain` empty before, during, and
  after every check in this rereview; scratch build directories
  (`build/mj-verify*`) removed and confirmed `.gitignore`-covered while
  present; no leaked private `dbus-daemon` or `qindaqt*` process after any
  test run — only the pre-existing host system/session/at-spi daemons.
- No nested compositor, Wayland session, or `tests/session`/`shell.`/
  `compositor.` test was started at any point.

### P2 — not blocking this candidate, flagging for the Program Manager

The full-tree Release build still cannot complete end-to-end:
`src/shell_customization_editor/src/keyboard_navigation.cpp:111` trips
`-Werror=maybe-uninitialized` under GCC 15.3.0 at `-O2` (an `std::optional`
move false positive). This is pre-existing, outside `shell_customization_editor`
is outside the 12 changed paths, and was explicitly scoped out of this
repair's own verification ("the requested targeted Release D0-D6 graph, not
a full-tree Release build" — repair handoff). Confirmed independently: a
`-k 0` rebuild shows it is the *only* broken target in the entire tree on
this exact commit. Someone should fix or reclassify this warning so a real
full-tree Release gate is possible again; it is not this D6 candidate's
responsibility.

### P3 — observation, unchanged from before

`tests/session/CMakeLists.txt` still requires KDecoration3 unconditionally
(`find_package(KDecoration3 6.6 REQUIRED CONFIG)`), even when
`QINDAQT_BUILD_KWIN_PLUGIN=OFF`. This matches real CI behavior and predates
D6; not this repair's scope, but still worth a wiki clarification someday
since "dependency-light core" could read as KDecoration3-optional.

### Caveats

- Contained nested KWin convergence remains a separately owned, unclaimed
  proof for `display_runtime`, as it was before this repair; nothing here
  changes that.
- The Release counts above come from a `-k 0` best-effort build for the
  reason given in P2; the D6/D2-scoped selectors themselves ran to full
  completion and passed 100% on both configurations, which is the evidence
  that matters for this verdict.
- I did not re-derive the handoff's own "44/44 incremental rebuild" or exact
  "1,617 sources" claims from scratch beyond what's reported above; my own
  fresh full builds (1921/1921 Debug targets, 0 errors/0 warnings) and
  selector runs are the evidence this verdict rests on.

### Next action

Accept `9a7872aec60a5e0f8286b3d5af7fa21209e8fd65` for integration onto the
manager branch. This acceptance is for this exact tree
(`5fe0e5f22f600de24398f77b25fbf36afba08178`) only — re-review any further
descendant. Separately, someone should open a fix for the pre-existing
`shell_customization_editor` Release warning (P2) so the next candidate's
Release gate isn't blocked by unrelated code.
