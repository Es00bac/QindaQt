# Dana Ulery — independent repaired-candidate recheck

- Persona: Dana Ulery, independent shell/harness reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Exact candidate SHA: `e64418798cdc2ef7e4244f07664f8c9fda98a862`
- Tree SHA: `bfde197b3d9a41f5536b2324bf30f947e284020c`
- Parent SHA: `e79545e9ce739bd9c9e578cabcd93d23827359f3`
- Repaired product ancestor: `a52561f6accaa2cd43e20ba7251e446d8c5ae1ad`
- Base SHA: `349f805b685c0b5b1ad146d600dc1c3fa528281d`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/panel-visibility-proof-codex-review`
- Repair diff: `git diff a52561f6accaa2cd43e20ba7251e446d8c5ae1ad..e64418798cdc2ef7e4244f07664f8c9fda98a862`
- Whole candidate: `git diff 349f805b685c0b5b1ad146d600dc1c3fa528281d..e64418798cdc2ef7e4244f07664f8c9fda98a862`

## Verdict

ACCEPT. The four prior P1 findings are closed by mutation-sensitive registered tests and by two successful serial nested repetitions at both installed resolutions. I found no new P0–P2 defect in the repair or the whole candidate.

## Findings ledger

### P0

None.

### P1

None.

### P2

None.

### P3

None.

## Prior-finding closure

### P1-1 — canonical Settings1 signed 64-bit decoding: closed

`src/shell/runtime/panelvisibilityruntime.cpp:43-47` now requires and decodes `QMetaType::LongLong`. The registered private-bus test at `tests/shell_visibility_producers/tst_panelvisibilitysettingsprivatebus.cpp:116-121` asserts that the production snapshot carries `LongLong`, applies `reducedMotion=false`, and preserves the 320 ms theme duration. The focused Debug row passed, as did the reduced-motion row. These assertions would fail on `a52561f`, whose decoder required `QMetaType::Int` and therefore retained reduced-motion safe defaults for the same canonical value.

### P1-2 — bounded, owner/lifetime-fenced popup holds: closed

`src/shell/runtime/panelvisibilitypopup.h:22-25,36-43` exposes explicit source, aggregate lease, source-ID length, owner, and uninterrupted-lifetime contracts. `panelvisibilitypopup.cpp:170-237` rejects invalid ownership/admission, does not renew on duplicate visible notifications, fences callbacks by source generation, and schedules the fixed expiry; owner destruction and explicit close release through `releaseSource()`.

The registered assertions at `tests/shell_visibility_producers/tst_panelvisibilitypopupbounds.cpp:85-120` prove the 128-lease, 32-source, 128-character, and non-null-owner bounds. Lines 123-154 prove impostor-owner rejection, owner-destruction release, non-renewing duplicate visibility, the exact maximum delay, and expiry release. The focused row passed. The ancestor had neither the owner-bearing API nor these bounds/expiry; the prior 10,000-source/orphan reproduction therefore cannot compile against the repaired public boundary, while the registered descendant test exercises the equivalent negative controls.

### P1-3 — pixels joined to phase authority and hostile identical images: closed

`tests/session/test_panel_visibility_nested.py:141-206` validates the exact ordered eight-phase surface authority and derives the left/bottom panel rectangles from mapped, committed phase records. Lines 209-274 decode each bounded PNG, hash pixels only inside that authority-owned rectangle, and require each hidden/visible pair to differ. The registered hostile test at `tests/session/test_desktop_session_panel_visibility_unit.py:110-116` copies the same unrelated valid image into every phase and requires rejection with `panel pixels did not change`; it passed. The positive unit assertion also requires all eight canonical phases and a `panelRegion` record per capture.

The ancestor validator accepted the prior six identical images because it had no interaction argument or panel-region comparisons. The exact legacy scratch script now stops with `TypeError` because its obsolete two-argument call no longer matches `_validate_captures(width, height, interaction)`; this is structural confirmation only, not counted as the closure evidence. The registered hostile row is the executable closure.

### P1-4 — causal move and close qualification: closed

`tests/session/panelvisibilitysessionprobe.cpp:247-305` establishes overlap-hidden, re-establishes hidden authority immediately before injected movement, captures moved-away, establishes a fresh full-screen hidden close precondition, closes the window, and captures restored visibility. `tests/session/panelvisibilitysessionwindowproof.cpp:100-153,202-235` reads compositor window geometry before and after the Meta-drag, requires an actual position change accepted against output/panel geometry, and records the final pre-input surface authority. The emitted envelope records `move.before`, `move.after`, `move.surfacesBefore`, `close.before`, and `windowAbsentAfter`.

The registered negative controls at `tests/session/test_desktop_session_panel_visibility_unit.py:118-138` reject unchanged geometry, visible pre-drag authority, a still-present closed window, and non-covering close geometry. They passed. The ancestor had a single geometry read and no close capture/envelope, so these assertions reject its evidence shape. The exact legacy source-shape scratch script now stops because the helper was deliberately decomposed into `panelvisibilitysessionwindowproof.cpp`; it is not counted as closure evidence.

All four successful installed archives contain the exact eight phases. At 1920x1080, both repetitions moved the authority window from `(0,30 722x517)` to `(0,336 722x517)`; at 1920x1200, both moved it to `(0,402 722x517)`. Each archive reports the close window absent, false host display/input/session-bus reachability, and no survivor PIDs. Each required hidden/visible panel-region pair had different SHA-256 values.

## Regression probes

- Authority loss: `CompositorVisibilityClientTests::serviceLossForcesSafeVisibleAndCancelsPendingWork` passed, proving owner loss clears the snapshot, cancels in-flight work, and requires safe-visible. `PanelRuntimePlanAssemblerTests::safeFallbackMapsAndReservesEligibleSurfaces` passed, proving the fallback maps eligible panels and preserves reservations.
- Reduced motion: both `qindaqt.shell-visibility-producer-reduced-motion` and the production-shaped `qindaqt.shell-visibility-settings-private-bus` passed. The latter observes the canonical `qint64` wire value and the uncapped 320 ms duration when reduced motion is false.
- Containment/cleanup: sandbox qualification passed first. The four counted nested archives all report host endpoints unreachable and `survivorPids=[]`; process audits before, between, and after returned no candidate Weston, KWin, probe, or nested-driver process.

## Commands and executed evidence

### Identity, scope, and cleanliness

```sh
git rev-parse HEAD
git rev-parse 'HEAD^{tree}'
git rev-parse 'HEAD^1'
git rev-parse 349f805b685c0b5b1ad146d600dc1c3fa528281d
git status --porcelain=v1
git log --oneline --decorate -6
git merge-base a52561f6accaa2cd43e20ba7251e446d8c5ae1ad e64418798cdc2ef7e4244f07664f8c9fda98a862
git diff --stat a52561f6accaa2cd43e20ba7251e446d8c5ae1ad..e64418798cdc2ef7e4244f07664f8c9fda98a862
git diff --name-status 349f805b685c0b5b1ad146d600dc1c3fa528281d..e64418798cdc2ef7e4244f07664f8c9fda98a862
```

All commands exited `0`. Initial and final HEAD/tree/parent/base are the values in the header; merge-base is `a52561f6accaa2cd43e20ba7251e446d8c5ae1ad`. Porcelain output was empty before and after review. The repair is bounded to the two owning wiki pages, shell visibility runtime, its focused tests, and installed panel qualification harness, plus additive workflow records/registrations.

### Configure

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Both exited `0`. CMake emitted only the existing mixed-prefix RPATH warnings.

### Focused builds

In both configurations I ran:

```sh
cmake --build <config-root> --parallel 3 --target \
  qindaqt_shell_panel_visibility_producers qindaqt-shell \
  qindaqt_panel_visibility_producer_tests qindaqt_panel_visibility_popup_bounds_tests \
  qindaqt_panel_visibility_settings_private_bus_tests qindaqt-panel-visibility-session-probe \
  qindaqt_panel_visibility_modes_tests qindaqt_panel_visibility_scope_tests \
  qindaqt_panel_visibility_validation_tests qindaqt_compositor_visibility_snapshot_tests \
  qindaqt_compositor_visibility_state_tests qindaqt_compositor_visibility_wire_roundtrip_tests \
  qindaqt_compositor_visibility_client_tests qindaqt_qt_compositor_visibility_transport_tests \
  qindaqt_panel_interaction_store_tests qindaqt_panel_visibility_inventory_assembler_tests \
  qindaqt_panel_runtime_plan_assembler_tests qindaqt_output_inventory_matcher_tests \
  qindaqt_shell_runtime_options_tests qindaqt_shell_visibility_snapshot_tests \
  qindaqt_shell_visibility_window_admission_tests qindaqt_shell_visibility_refresh_scheduler_tests \
  qindaqt-shell-preview qindaqt_shell_launcher_qmlplugin qindaqt_controls_qmlplugin
```

Debug exited `0` after 1283/1283 Ninja actions. Release exited `0` after the incrementally resolved graph completed 632/632 actions.

### Visibility, shell-runtime, and focused repair rows

```sh
ctest --test-dir <config-root> -N \
  -R '^(compositor\.shell-visibility|qindaqt\.shell-visibility|qindaqt\.shell-orchestration-(interactions|visibility-inventory|runtime-plan|output-match)|qindaqt\.shell-runtime|desktop\.virtual\.panel-visibility\.validator-unit)'

ctest --test-dir <config-root> \
  -R '^(compositor\.shell-visibility|qindaqt\.shell-visibility|qindaqt\.shell-orchestration-(interactions|visibility-inventory|runtime-plan|output-match)|qindaqt\.shell-runtime|desktop\.virtual\.panel-visibility\.validator-unit)' \
  --output-on-failure --no-tests=error
```

Discovery found exactly 27 rows in each configuration. Debug exited `0`, 27/27 passed in 6.22 seconds. Release exited `0`, 27/27 passed in 5.67 seconds. These include the three shell-runtime rows, all adjacent visibility rows, the popup/settings repairs, and the validator unit row.

I also reran the four repair rows alone:

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/debug \
  -R '^(qindaqt\.shell-visibility-producer-reduced-motion|qindaqt\.shell-visibility-popup-bounds|qindaqt\.shell-visibility-settings-private-bus|desktop\.virtual\.panel-visibility\.validator-unit)$' \
  --output-on-failure --no-tests=error
```

Exit `0`, 4/4 passed in 1.16 seconds.

Direct authority-loss probes:

```sh
QT_QPA_PLATFORM=offscreen /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/debug/tests/shell_visibility_client/qindaqt_compositor_visibility_client_tests serviceLossForcesSafeVisibleAndCancelsPendingWork
QT_QPA_PLATFORM=offscreen /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/debug/tests/shell_orchestration/qindaqt_panel_runtime_plan_assembler_tests safeFallbackMapsAndReservesEligibleSurfaces
```

Both exited `0`; each selected behavior passed with its QtTest init/cleanup (3 passed, 0 failed).

### Authorized nested rows

First:

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/debug \
  -R '^desktop\.virtual\.sandbox-unit$' --output-on-failure --no-tests=error
```

Exit `0`, 1/1 passed in 2.01 seconds.

Then, twice serially:

```sh
QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop \
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/debug \
  --parallel 1 -R '^desktop\.virtual\.panel-visibility\.(single-1080p|single-wuxga)$' \
  --output-on-failure --no-tests=error
```

- Counted repetition 1: exit `0`, 3/3 passed (fixture package contract plus both panel rows), 89.83 seconds. Archives: `07358205507b418c637391925f89d1c7`, `15a95868385c3ec383209baef9a0043d`.
- Counted repetition 2: exit `0`, 3/3 passed, 89.78 seconds. Archives: `7dc2f12ea0206f00ef6f757d908469f6`, `ba7663a01b51b877a1456d5137159dc2`.
- An intervening invocation was externally terminated with exit `143` after its package fixture passed and before CTest published a row result. Its incomplete run ID `3f4e9719118087f64875f32661408054` had no `result.json`; it was excluded. The subsequent process audit was empty, and the clean retry above supplied the second complete repetition.

Process audit before, between, after termination, and after the retry:

```sh
ps -eo pid,ppid,sid,etime,args | \
  rg 'kwin_wayland|weston|test_panel_visibility_nested.py|desktop\.virtual\.panel-visibility|qindaqt-panel-visibility-session-probe' | \
  rg -v 'rg ' || true
```

No candidate-related process was reported.

### Prior reproductions and assertion audit

```sh
PYTHONDONTWRITEBYTECODE=1 python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/reproductions/capture_phase_vacuity.py
python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/reproductions/move_close_causality.py
```

Both legacy scripts exited `1`, but for obsolete harness shape: the first receives a missing `interaction` argument and the second looks for a helper moved into its own cohesive source file. I did not run the prior `producer_contract_repro` binary as descendant evidence because it was linked against ancestor objects. Instead, the registered focused rows above exercise the same defects against the exact candidate. I inspected their assertions with `nl -ba`/`rg -n` and compared the repair using `git diff a52561f..e644187`; the mutation-sensitive closure details and exact lines are recorded above.

### Static gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/site-r2
./tools/check-source-shape
git diff --check
git diff --check a52561f6accaa2cd43e20ba7251e446d8c5ae1ad..e64418798cdc2ef7e4244f07664f8c9fda98a862
git diff --name-only 349f805b685c0b5b1ad146d600dc1c3fa528281d..e64418798cdc2ef7e4244f07664f8c9fda98a862 -- '*.json' | \
  while IFS= read -r f; do python3 -m json.tool "$f" >/dev/null || exit; done
```

All exited `0`. `validate-docs` validated 127 documents/navigation entries; MkDocs built strictly. Source shape checked 2064 files with only the same three pre-existing threshold warnings and no failure. Both whitespace checks and all changed JSON parses passed.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
