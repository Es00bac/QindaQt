# Dana Ulery — independent shell/harness review

- Persona: Dana Ulery, independent shell/harness reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Exact candidate SHA: `a52561f6accaa2cd43e20ba7251e446d8c5ae1ad`
- Tree SHA: `27d3f5fa82cac65d9e6e90e07fc835be9d8b2d78`
- Parent SHA: `349f805b685c0b5b1ad146d600dc1c3fa528281d`
- Base SHA: `349f805b685c0b5b1ad146d600dc1c3fa528281d`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/panel-visibility-proof-codex-review`
- Reviewed diff: `git diff 349f805b685c0b5b1ad146d600dc1c3fa528281d..a52561f6accaa2cd43e20ba7251e446d8c5ae1ad`

## Findings ledger

### P0

None.

### P1

#### P1-1 — Production Settings1 integers are rejected, so reduced-motion and delay settings do not apply

`data/settings/schema-v2.json:171-175` defines `panels.autoHideDelayMs` as an integer. The established schema and wire contracts canonicalize that value as signed 64-bit: `tests/settings/tst_settings_schema.cpp:85-97` requires `QMetaType::LongLong`, and `src/services/settings_protocol/src/settings_wire_decode.cpp:262-290` converts signed and accepted unsigned integer forms to `qint64`. The new consumer instead accepts only `QMetaType::Int` at `src/shell/runtime/panelvisibilityruntime.cpp:38-48`. Because `applySettings()` returns unless both values validate at `src/shell/runtime/panelvisibilityruntime.cpp:160-170`, a legitimate canonical delay also prevents the legitimate `accessibility.reducedMotion=false` value from taking effect. The new test uses a source-language `int` literal at `tests/shell_visibility_producers/tst_panelvisibilityproducers.cpp:277-280`, bypassing the production representation.

Reproduction:

```sh
QT_QPA_PLATFORM=offscreen \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/reproductions/producer_contract_repro
```

Exit status: `1` (the scratch test deliberately fails when the defect is present).

Observed:

```text
canonicalDelayType=qlonglong reducedMotion=1 durationMs=80
uniqueSourcesAdmittedAtLeast=10000 lastUnreleasedSourceStillPins=1
FAIL: canonical Settings1 integer is rejected and an unbounded source can pin indefinitely
```

Expected: the canonical `qint64(400)` delay is accepted, `reducedMotion=false` is applied, and a 320 ms theme duration remains 320 ms. Observed: the runtime retains its safe defaults (`reducedMotion=true`, 80 ms cap), so the claimed production setting support does not work.

Scratch source: `/home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/reproductions/producer_contract_repro.cpp:44-76`.

#### P1-2 — Popup holds have no admission bound or owner/lifetime fence and can pin a panel indefinitely

`src/shell/runtime/panelvisibilitypopup.cpp:57-64` stores popup sources in an unrestricted `std::map<QString, std::vector<Lease>>`. `setPopupVisible()` at `src/shell/runtime/panelvisibilitypopup.cpp:150-179` accepts every distinct nonblank `sourceId`, has no source-count or source-length bound, has no owner token/generation, and installs no expiry. The ordinary window/object paths do attempt close/destruction release at lines 104-109 and 187-193, but the explicit producer boundary in `src/shell/runtime/panelvisibilitypopup.h:26-34` admits shell-owned sources without any enforceable lifetime or owner-loss cleanup. A producer that loses its owner or misses its closing call leaves the move-only store lease live until topology replacement or producer destruction.

Reproduction: the same compiled scratch command above admits 10,000 distinct source IDs, releases 9,999, waits and processes events, and observes the last orphan still holding the panel. Exit status `1`; the relevant output is:

```text
uniqueSourcesAdmittedAtLeast=10000 lastUnreleasedSourceStillPins=1
```

Expected: a bounded source registry and a producer-owned lifetime mechanism that retires leases on owner loss, preventing a hostile or failed producer from pinning a panel forever. Observed: at least 10,000 sources are accepted, and the final source remains live with no timer or owner-loss path. This violates the review brief's bounded-lease and no-indefinite-pin contract.

Scratch source: `/home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/reproductions/producer_contract_repro.cpp:78-105`.

#### P1-3 — The installed framebuffer validator accepts six identical unrelated images as six interaction phases

The probe records D-Bus surface inventories and then independently captures an image at `tests/session/panelvisibilitysessionprobe.cpp:263-274`; it never joins panel geometry/state to pixels. The outer validator at `tests/session/test_panel_visibility_nested.py:83-96` checks each named file only for exact dimensions, decodability/checksum, and eight colors. It performs no panel-region assertion and no phase relationship check. Consequently, the only installed framebuffer proof can remain green when no panel pixel changes, or when all files depict unrelated nonuniform content. The passing live runs happened to produce five distinct hashes, but that is neither required nor checked.

Reproduction:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/reproductions/capture_phase_vacuity.py
```

Exit status: `1` (deliberate defect detector).

Observed:

```text
accepted=6 distinctPhaseHashes=1
FAIL: candidate accepted one identical framebuffer as all six interaction phases
```

The scratch test creates one checksum-valid, exact 1920x1080, eight-color PNG, copies it to all six required names, and calls the candidate `_validate_captures()` unchanged except for redirecting its hard-coded evidence directory into the assigned build root. Expected: rejection because identical arbitrary pixels cannot prove hidden, revealed, held, and closed states. Observed: all six are accepted. This makes the installed framebuffer proof vacuous where it is the only pixel-level evidence.

Scratch source: `/home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/reproductions/capture_phase_vacuity.py:35-79`.

#### P1-4 — The installed move/close row does not prove that the injected drag moved the window or that close restores the panel

`tests/session/panelvisibilitysessionprobe.cpp:321-335` calls `client.showNormal()` before injecting the Meta-drag and does not re-establish that the intelligent left panel is hidden immediately before the drag. `moveProofWindowFromPanel()` at lines 173-210 reads the proof window geometry only before injection; after injected events are merely accepted, it never reads geometry again or requires a position delta. Thus `showNormal()` or another unrelated transition can reveal the panel and satisfy `leftVisible`. The close path at lines 336-339 begins with the panel already visible and only waits for that already-true state; it neither establishes a hidden precondition nor takes a post-close framebuffer capture. The six phase names at `tests/session/test_panel_visibility_nested.py:85-88` contain no close-restore phase.

Reproduction:

```sh
python3 \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/reproductions/move_close_causality.py
```

Exit status: `1` (deliberate defect detector).

Observed:

```text
geometryReadsInMoveHelper=1 showNormalBeforeDrag=true postCloseCapture=false
FAIL: accepted row neither verifies drag changed geometry nor captures the post-close restore
```

Expected: a hidden pre-drag state followed by a verified window geometry change and panel reveal, plus an independently exercised hidden-to-visible close transition with captured evidence. Observed: injection success is credited without a geometry change, and close merely rechecks the visible state. The installed row therefore does not establish the two causal interaction claims it documents.

Scratch source: `/home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/reproductions/move_close_causality.py:14-35`.

### P2

None.

### P3

None.

## Review-question answers

1. **Policy boundary and authority loss:** No QML/presentation decision was added. Pointer, edge, popup, shortcut, and animation producers acquire leases in `PanelInteractionStore`; `ShellRuntimeApplication::reconcileSurfaces()` sends accepted inventory plus lease snapshots through the existing visibility evaluation. On missing/rejected authority, `src/shell/runtime/shellruntimeapplication_surfaces.cpp:149-166` selects `PanelRuntimePlanAssembler::safeVisible()`, whose implementation at `src/shell_orchestration/src/panel_runtime_plan_assembler.cpp:25-35` maps every panel and preserves each base `reservesWorkArea` value. This is not silent: `docs/wiki/shell/panel-visibility.md:64-71,102-108` expressly specifies safe-visible authority loss. I found no boundary violation here.
2. **Lease lifetime/bounds:** Ordinary discovered popup close/destruction paths exist, and identity replacement clears held leases, but the explicit source API is unbounded and has no owner-loss fence or expiry. P1-2 fails this question.
3. **Installed interaction proof:** Both live rows pass and archive their six files, but the semantic framebuffer validator is vacuous (P1-3), and the move/close sequences do not establish causality (P1-4). The documentation at `docs/wiki/shell/panel-visibility.md:120-134` and `docs/wiki/development/testing-harness.md:1851-1873` therefore claims a stronger installed qualification than the executable gates prove.
4. **Containment/determinism/cleanup:** `desktop.virtual.sandbox-unit` passed 1/1 before nested execution. The named rows ran serially twice. All four fresh runs reported host display/input/session bus unreachable and empty survivors; post-run host process audits found no candidate Weston, KWin, probe, or nested driver. No host bus, hardware, uinput, network, or forbidden session rows were invoked.
5. **Scope/static shape:** The candidate changes are limited to shell runtime producers, focused tests/harness, additive test registration, and the two owning wiki pages. `check-source-shape`, both doc gates, JSON parse, and `git diff --check` pass. The shared `src/shell/CMakeLists.txt` and runtime application edits are additive for this outcome. The worktree remained detached at the exact candidate and clean before and after review.

## Commands and executed evidence

### Identity and cleanliness

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^1
git rev-parse 349f805
git status --porcelain=v1
```

Initial and final results: candidate `a52561f6accaa2cd43e20ba7251e446d8c5ae1ad`, tree `27d3f5fa82cac65d9e6e90e07fc835be9d8b2d78`, parent/base `349f805b685c0b5b1ad146d600dc1c3fa528281d`, and empty porcelain output. Exit status `0`.

### Configure

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/dev -G Ninja \
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

Results: both exit `0`.

### Focused builds

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/dev --parallel 3 --target \
  qindaqt_shell_panel_visibility_producers qindaqt-shell \
  qindaqt_panel_visibility_producer_tests qindaqt-panel-visibility-session-probe \
  qindaqt_panel_visibility_modes_tests qindaqt_panel_visibility_scope_tests \
  qindaqt_panel_visibility_validation_tests qindaqt_compositor_visibility_snapshot_tests \
  qindaqt_compositor_visibility_state_tests qindaqt_compositor_visibility_wire_roundtrip_tests \
  qindaqt_compositor_visibility_client_tests qindaqt_qt_compositor_visibility_transport_tests \
  qindaqt_panel_interaction_store_tests qindaqt_panel_visibility_inventory_assembler_tests \
  qindaqt_panel_runtime_plan_assembler_tests qindaqt_output_inventory_matcher_tests \
  qindaqt_shell_runtime_options_tests
```

Debug result: exit `0`, 1204/1204 Ninja actions. The equivalent Release command omitted the Debug-only session probe and exited `0`, 601/601 actions.

Adjacent visibility and installed-component prerequisites were then built in both configurations:

```sh
cmake --build <config-root> --parallel 3 --target \
  qindaqt_shell_visibility_snapshot_tests \
  qindaqt_shell_visibility_window_admission_tests \
  qindaqt_shell_visibility_refresh_scheduler_tests
cmake --build <config-root> --parallel 3 --target qindaqt-shell-preview
```

Both configurations exited `0` (14/14 and 54/54 actions respectively). Release additionally needed the install-time plugin artifacts:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/release \
  --parallel 3 --target qindaqt_shell_launcher_qmlplugin
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/release \
  --parallel 3 --target qindaqt_controls_qmlplugin
```

Both exited `0`, 4/4 actions. Before those prerequisites, the component-closure row failed only because its install script could not find the unbuilt preview/plugin artifacts; after building the declared artifacts, the row and full selectors passed as reported below.

### Selectors and tests

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/dev -N | grep -i visib
```

Exit `0`; listed 20 rows: compositor visibility 3, shell visibility/policy/client/inventory/producers 15, and the two nested rows.

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/dev \
  -R '^desktop\.virtual\.sandbox-unit$' --output-on-failure --no-tests=error
```

Exit `0`; 1/1 passed.

```sh
ctest --test-dir <config-root> \
  -R '^(compositor\.shell-visibility|qindaqt\.shell-visibility|qindaqt\.shell-orchestration-(interactions|visibility-inventory|runtime-plan|output-match)|qindaqt\.shell-runtime)' \
  --output-on-failure --no-tests=error
```

Final results after building the explicitly required install artifacts: Debug exit `0`, 24/24; Release exit `0`, 24/24. This includes all 18 non-nested visibility rows, four orchestration rows, and all three shell-runtime rows, with the overlapping producer rows counted once.

### Authorized nested rows

The following command was executed twice, serially, with candidate-process audits before, between, and after:

```sh
QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop \
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/dev \
  --parallel 1 \
  -R '^desktop\.virtual\.panel-visibility\.(single-1080p|single-wuxga)$' \
  --output-on-failure --no-tests=error
```

- Run 1: exit `0`, 3/3 passed (package contract plus both rows), 71.03 seconds.
- Run 2: exit `0`, 3/3 passed, 69.28 seconds.
- Fresh archives: `925f122d4808c44be32cd6e80adbec7b` (1080p), `fca62e9ff05821cc45e19122b67aab20` (WUXGA), `76afb5aa4ba33b1a06ca26ea585f9d60` (1080p), and `a82eb701d1b03042f65e4aac09eb8e19` (WUXGA).
- Every archive reports `outcome=success`, six captures/five distinct hashes, all six named phases, `hostDisplayReachable=false`, `hostInputReachable=false`, `hostSessionBusReachable=false`, and `survivorPids=[]`.
- Process-audit command: `ps -eo pid,ppid,sid,etime,args | rg 'kwin_wayland|weston|test_panel_visibility_nested.py|desktop\.virtual\.panel-visibility|qindaqt-panel-visibility-session-probe' | rg -v 'rg ' || true`. Candidate-related output was empty between and after runs.

### Static gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/site
./tools/check-source-shape
git diff --check 349f805b685c0b5b1ad146d600dc1c3fa528281d..a52561f6accaa2cd43e20ba7251e446d8c5ae1ad
python3 -m json.tool tests/session/fixtures/panel_visibility_profiles/panel-visibility-proof.json >/dev/null
```

All exit `0`. `validate-docs` validated 127 documents/navigation entries. MkDocs built strictly. Source shape checked 2059 files with three pre-existing threshold warnings (`tests/compositor/CMakeLists.txt`, `tests/services/display_color_model/tst_color_model.cpp`, and `tests/shell/audio_applet/tst_audio_applet_controller.cpp`) and no failure. Diff whitespace and the sole changed JSON parse passed.

### Defect reproductions

```sh
PYTHONDONTWRITEBYTECODE=1 python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/reproductions/capture_phase_vacuity.py
python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/reproductions/move_close_causality.py
QT_QPA_PLATFORM=offscreen /home/cabewse/work_SPaC3/builds/qindaqt/review-panelproof-codex/reproductions/producer_contract_repro
```

Each exits `1` by design after observing its asserted defect. Combined status: `capture_status=1 move_status=1 producer_status=1`; exact observed text is recorded in the findings above.

## Verdict

The candidate is reproducibly buildable, contained, cleanly terminating, and preserves the pure policy boundary, but it cannot be accepted with four P1 contract failures: production settings are type-incompatible, popup lease admission/lifetime is unbounded, framebuffer validation is vacuous, and move/close interaction causality is not proved.

VERDICT REJECT P0/P1/P2/P3=0/4/0/0
