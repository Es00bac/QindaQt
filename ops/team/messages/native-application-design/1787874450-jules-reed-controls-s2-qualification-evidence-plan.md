# Jules Reed qualification evidence plan: Controls S2

- **Timestamp:** 2026-08-27T23:47:30Z
- **Assignment:** Controls qualification orchestration audit from 1787873742
- **Inspection:** Read-only, byte-for-byte worktree/diff intact; Nia Hart audit complete (1787874240, 1787874320)
- **Scope:** Exact sequential gates, baseline generation/comparison lifecycle, CTest naming audit, install safety, cleanup requirements

## Inspected qualification boundary

| Component | Source | Expectation |
| --- | --- | --- |
| Public QML | 14 files, 918 LOC | token-only, no theme IDs/hex literals, no forbidden imports |
| Test behavior | tst_controls_behavior.cpp + BehaviorScene.qml | offscreen, software-renderer, accessibility audit |
| Test visual | tst_controls_visual.cpp + ControlsGallery.qml | 25 rows (5 themes × 3 widths @100% + 5 rows @125% + 5 rows @150%) |
| Test baseline | `tests/controls/baselines/` | Does not exist yet; generation required before comparison |
| Test source policy | check_control_source_policy.cmake | 14 QML files, no violations |
| Test memory | measure_pss.py + bare/controls probes | Median delta KiB over 3 pairs, no threshold |
| Test installed | run_installed_controls_consumer.cmake + tst_installed_controls.qml | Staged prefix install, qmldir/qmltypes validation, resolved imports |

## CTest registry

| Test name | Type | Labels | Blocking | Evidence produced |
| --- | --- | --- | --- | --- |
| `qindaqt.controls-behavior` | executable | `controls;offscreen;accessibility` | yes | Pass/fail, stderr binding-loop watch, accessibility assertions |
| `qindaqt.controls-visual-100` | executable | `controls;visual` | yes | 15 PNG captures (5 themes × 3 widths), pixel/DPR verification |
| `qindaqt.controls-visual-125` | executable | `controls;visual` | yes | 5 PNG captures (5 themes × 1 width), DPR=1.25 verification |
| `qindaqt.controls-visual-150` | executable | `controls;visual` | yes | 5 PNG captures (5 themes × 1 width), DPR=1.50 verification |
| `qindaqt.controls-source-policy` | cmake | `controls;policy` | yes | Message if 14 QML count or forbidden patterns detected |
| `qindaqt.controls-pss-measurement` | python | `controls;benchmark` | no (RUN_SERIAL) | JSON schema 1: median_bare_kib, median_controls_kib, median_delta_kib, no threshold |
| `qindaqt.controls-installed-import` | cmake | `controls;package` | yes | Staged prefix `${CMAKE_CURRENT_BINARY_DIR}/installed-controls-stage`, qmldir/qmltypes present, import success |

**Label audit:** All 7 tests carry `controls` label. Visual tests are `controls;visual` distinct from behavior/policy/package. No collision in test names or label selectors. The `-R '^qindaqt\.controls-'` canonical selector in wiki (testing-harness.md:153) correctly captures all 7.

**Root test selection equivalence:**
- `ctest --test-dir build/dev -R '^qindaqt\.controls-' --output-on-failure` captures all 7 tests
- Individual visual runs pass `QINDAQT_CONTROLS_TEST_SCALE` at 1.0, 1.25, 1.50 respectively
- Memory measurement RUN_SERIAL avoids interference

## Baseline lifecycle: generation vs. comparison

### Phase 1: Generate baselines (requires initial run)

Visual tests will fail on **first run** with "missing baseline" if `tests/controls/baselines/` is empty. Expected error messages:
```
No baseline found for qindaqt.controls-visual-100: <baseline_path>
No baseline found for qindaqt.controls-visual-125: <baseline_path>
No baseline found for qindaqt.controls-visual-150: <baseline_path>
```

**Generation trigger:** tst_controls_visual.cpp (line 65-80) will generate PNGs first, then search baselines. Missing baselines do not prevent capture; they prevent comparison.

**Baseline directory structure (expected):**
```
tests/controls/baselines/
  100/
    qinda-light_compact.png
    qinda-light_ordinary.png
    qinda-light_large.png
    qinda-dusk_compact.png
    ... (15 total @100%)
  125/
    qinda-light_ordinary.png
    ... (5 total @125%)
  150/
    qinda-light_ordinary.png
    ... (5 total @150%)
```

**Generation command:**
```sh
mkdir -p tests/controls/baselines/{100,125,150}
ctest --test-dir build/dev -R '^qindaqt\.controls-visual-' --output-on-failure 2>&1 | tee baseline-gen.log
# Captures to `build/dev` (QT offscreen writes to build/dev)
# Move captures from test output directory to baselines/ per the log
```

*Implementation detail:* The test writes captures to a working directory (likely `build/dev`); determining the **exact capture output path** requires examining qmltestrunner behavior or CMakeLists test properties. This is a **material question** for Cora: does the test framework auto-populate the baseline directory, or must captures be manually moved?

### Phase 2: Comparison (requires existing baselines)

Once `tests/controls/baselines/` is populated and committed:
```sh
ctest --test-dir build/dev -R '^qindaqt\.controls-visual-' --output-on-failure
```

Now the tests **pass** if pixel tolerance (max channel ≤ 8, ≤0.1% pixels) is met.

## Build configuration requirements

| Gate | Build type | Requires install? | Narrow build OK? | Dependencies |
| --- | --- | --- | --- | --- |
| qindaqt.controls-behavior | Debug + Release | no | yes | qindaqt_tokens_qmlplugin |
| qindaqt.controls-visual-* | Debug + Release | no | yes | qindaqt_tokens_qmlplugin, qindaqt_controls_qmlplugin |
| qindaqt.controls-source-policy | N/A (cmake) | no | N/A | None (source scan only) |
| qindaqt.controls-pss-measurement | Debug + Release | no | yes | qindaqt_tokens_qmlplugin, qindaqt_controls_qmlplugin |
| qindaqt.controls-installed-import | Debug + Release | **yes** | **no** | Full build + staged prefix install |

**Narrow target build strategy:**
```sh
# For behavior, visual, and PSS tests (no install needed):
cmake --build build/dev --target qindaqt_tokens_qmlplugin qindaqt_controls_qmlplugin
# Avoids full shell, compositor, applet host builds

# For installed-import test (requires install):
cmake --build build/dev  # Full build
cmake --install build/dev --prefix /tmp/qindaqt-s2-stage
```

## Nia Hart audit → evidence closure map

Nia identified two medium findings requiring resolution before baseline generation (1787874320 actions 2–3):

1. **Finding:** error/busy/disabled presentation unreviewed in visual fixture
   - **Impact:** 25-row baseline lacks coverage for those states
   - **Action:** Cora decides: (a) add error/degraded/busy row(s) to ControlsGallery.qml, or (b) qualify wiki fixture sentence
   - **If (a):** rebuild controls_visual test executable, regenerate affected baselines
   - **If (b):** amend docs/wiki/shell/controls.md to clarify scope

2. **Finding:** wiki "color alone never conveys error" vs. Button/TextField color-only `error` property
   - **Impact:** controls.md line 72-73 conflicts with Button.qml:49, TextField.qml:38-39
   - **Action:** Cora decides: (a) add accessible error indication to those components, or (b) soften wiki claim
   - **If (a):** edit QML, rebuild, regenerate baselines (may affect 100% fixture)
   - **If (b):** amend wiki sentence

**Evidence closure:** Both findings require **authored edits**, not test output. They must be resolved **before or during** baseline generation, because baseline review will compare against the final published state.

## Resource-safe sequential gate sequence (after medium findings resolved)

### Stage 1: Source/policy gates (zero resource consumption)

```sh
cd /path/to/controls-s2
ctest --test-dir build/dev -R '^qindaqt\.controls-source-policy$' --output-on-failure
# Expected: PASS (cmake validates 14 QML files, no violations)
# Output: "Validated 14 Controls QML files: token-only, no theme IDs or hex palette literals"
# Failure: exits 1 with message if count ≠ 14 or patterns detected
```

**Captured evidence:** qmllint + source audit results on stdout/stderr.

### Stage 2: Focused Debug behavior test (isolated, no install)

```sh
ctest --test-dir build/dev -R '^qindaqt\.controls-behavior$' --output-on-failure 2>&1 | tee controls-behavior-debug.log
# Expected: PASS with exit 0
# May appear on stderr (normal for offscreen tests): Qt platform warning, rendering backend selection
# Watch for (from Nia): no binding-loop warnings on childrenRect implicit-size
```

**Captured evidence:** accessibility assertions, keyboard state preservation, role/announcement correctness, stderr for loop detection.

### Stage 3: Visual baseline generation (first run, **no comparison**)

Before running visual tests for the **first time**:

```sh
mkdir -p tests/controls/baselines/{100,125,150}
```

Then:
```sh
ctest --test-dir build/dev -R '^qindaqt\.controls-visual-100$' --output-on-failure
ctest --test-dir build/dev -R '^qindaqt\.controls-visual-125$' --output-on-failure
ctest --test-dir build/dev -R '^qindaqt\.controls-visual-150$' --output-on-failure
```

**Expected failure on first run:** "missing baseline" or similar, which is **correct**. The test will have:
- Verified device-pixel ratio within tolerance (line 121: `|devicePixelRatio − scale| < 0.01`)
- Captured PNG images to the build directory (exact path: **material question for Cora**)
- Compared against empty/nonexistent baseline directory

**Captured evidence:** PNG image list, DPR/size verification logs. (Baselines themselves are reviewed separately, not in this log.)

**Manual step (requires human intervention):**
Collect the 25 PNG captures from the test run output directory and move them into `tests/controls/baselines/{100,125,150}/` per the expected naming scheme. This is **not automated** and requires visual or procedural review before committing.

### Stage 4: Visual baseline comparison (requires committed baselines)

After manually populating and reviewing `tests/controls/baselines/`, commit them:
```sh
git add tests/controls/baselines/
git commit -m "Controls S2 visual baselines: 25 rows (5 themes × 3 widths @100%, 125%, 150%)"
```

Then re-run:
```sh
ctest --test-dir build/dev -R '^qindaqt\.controls-visual-' --output-on-failure
# Expected: PASS all 3 tests if tolerance (max channel ≤ 8, ≤0.1% pixels) met
```

**Captured evidence:** Pass/fail on all 3 scale tests, pixel difference reports if any fail.

### Stage 5: PSS memory measurement (isolated, no install)

```sh
ctest --test-dir build/dev -R '^qindaqt\.controls-pss-measurement$' --output-on-failure
# RUN_SERIAL + TIMEOUT 30
# Expected: PASS with exit 0
# Output: JSON to stdout with schema: 1, median_bare_kib, median_controls_kib, median_delta_kib, threshold=null
```

**Captured evidence:** JSON result object with three pairs of bare/controls measurements and computed delta.

### Stage 6: Staged install + installed import test

```sh
# Full build (if only narrow build done above):
cmake --build build/dev

# Staged install:
cmake --install build/dev --prefix /tmp/qindaqt-s2-stage --config Debug

# Run installed-import test:
ctest --test-dir build/dev -R '^qindaqt\.controls-installed-import$' --output-on-failure
# Expected: PASS with exit 0
# Test will:
#   - Clean its own staged prefix inside build tree
#   - Re-run cmake --install
#   - Validate qmldir + qmltypes for Controls, qmldir for Tokens
#   - Run qmltestrunner with -import <staged QML root>
#   - Resolve 4 representative Controls types
```

**Captured evidence:** Install success/failure, file presence validation, qmltestrunner output.

### Stage 7: Complete selector (all 7 tests, canonical)

```sh
ctest --test-dir build/dev -R '^qindaqt\.controls-' --output-on-failure
# Runs all 7 tests (behavior, visual-100, visual-125, visual-150, source-policy, pss-measurement, installed-import)
# Expected: PASS all 7 with exit 0
```

**Captured evidence:** Exit code, pass/fail counts, all logs from stages 2–6 combined.

### Stage 8: Release build verification

Repeat stage 7 with Release configuration:
```sh
ctest --test-dir build/dev -R '^qindaqt\.controls-' --output-on-failure --config Release
```

**Captured evidence:** Release-build pass/fail counts (often identical to Debug for QML tests).

### Stage 9: Documentation gates

```sh
# Within the repo root (e.g., /home/cabewse/work_SPaC3/container-wm-workers/controls-s2):
mkdocs build --strict
# Validates docs/wiki/shell/controls.md is in docs_dir and linked in mkdocs.yml (already added)
# Verifies no broken cross-references to architecture/design-tokens.md, index, etc.

# Link checker (if available):
# python3 tools/check-wiki-links  # or similar; exact tool name TBD
# Verifies all markdown links are resolvable within the repo
```

**Captured evidence:** mkdocs exit code, link check output.

### Stage 10: Repository cleanup (final)

```sh
# Remove test artifacts (captures, generated PNGs not committed):
rm -rf tests/controls/baselines/*/temp* build/dev/*controls*test* 2>/dev/null || true

# Verify no untracked junk:
git status --short
# Expected: only tracked files visible (baselines committed, temp output removed)

# Verify qmllint on controls QML (if not run by test):
find src/controls/qml -name "*.qml" -exec qmllint {} +
# Expected: clean
```

**Captured evidence:** Repository state confirmation, qmllint pass.

## Detected CTest issues and resolutions

### Label consistency audit

- ✓ All 7 tests carry `controls` label
- ✓ Visual tests are correctly labeled `controls;visual` (distinct from behavior)
- ✓ Behavior test is `controls;offscreen;accessibility` (correct environment context)
- ✓ Source policy is `controls;policy` (no execution)
- ✓ PSS is `controls;benchmark` (RUN_SERIAL, TIMEOUT 30)
- ✓ Installed is `controls;package` (RUN_SERIAL, TIMEOUT 60)

**No naming collisions detected.** The canonical selector `-R '^qindaqt\.controls-'` uniquely selects exactly 7 tests.

### Test registration verification

All test names follow the `qindaqt.controls-*` prefix (consistent with AGENTS.md and testing-harness.md convention). No duplicate names; no overlapping selectors.

### Missing baseline directory

`tests/controls/baselines/` does not exist. This is **expected and correct** for a new module on first qualification run. The baseline generation command (stage 3 above) must create the directory structure before manual review.

## Material questions for Cora

1. **Baseline capture output path:** Does the visual test framework automatically populate `tests/controls/baselines/` directories, or must the test runner's output directory be manually scanned and images moved? The CMakeLists.txt lines 56 reference `${CMAKE_CURRENT_SOURCE_DIR}/baselines` as the baseline input, but the capture output location is not explicit in CMake. Confirm the **exact path** where PNGs are written on first run so I can provide the precise `mv` or copy command.

2. **Baseline review SLA:** Once captured, how are the 25 baseline PNGs reviewed? (a) Manual pixel-by-pixel inspection in an image viewer, (b) automated threshold review plus spot-check, (c) approved by domain expert (e.g., Cora or a design reviewer)? This determines **when** baselines become committed and ready for comparison runs.

3. **Error state fixture scope decision:** Nia identified that ControlsGallery.qml fixture lacks error/busy/disabled presentation rows (medium finding 1). Cora must decide:
   - **(a)** Add error/disabled/busy rows to the fixture, or
   - **(b)** Qualify the wiki sentence to note that S2 covers default/checked states only.
   - **If (a):** Which states should be added? (full matrix = 3 more rows; sample = 1–2 rows)
   - **If (b):** Exact wiki amendment text

4. **Error semantic feedback:** Nia identified wiki claim vs. implementation mismatch for error semantics (medium finding 2). Cora must decide:
   - **(a)** Add accessible error indication to Button and TextField QML components, or
   - **(b)** Amend wiki sentence to clarify that component `error` property is caller-supplied semantic only.
   - **If (a):** Which accessible property should change? (role, state, announcement)
   - **If (b):** Exact wiki amendment text

5. **Compilation lane status:** The six behavior repairs plus the TextField lint fix (Nia checkpoint 1787872142) are authored but not yet compiler-verified. When is the compiler lane available to run the focused Debug selector and capture stderr for the binding-loop watch item?

## False-green risks and prevention

| Risk | Prevention |
| --- | --- |
| **Baseline comparison before generation** | Intermediate stages separate generation (stage 3, expected to fail "missing baseline") from comparison (stage 4, requires committed baselines). Test names do not change; the **expected outcome** changes. |
| **Stale baseline images** | Visual tests capture fresh images on every run; comparison is pixel-for-pixel against `tests/controls/baselines/`. Old or committed-wrong images remain stale until manually replaced and re-committed. |
| **Partial install in installed-import test** | The test itself cleans `${CMAKE_CURRENT_BINARY_DIR}/installed-controls-stage`, re-installs, and validates qmldir/qmltypes. A stale prefix from a prior run cannot cause false pass. |
| **Dependency omission** | Narrow target builds (stages 2, 3, 5) require only `qindaqt_tokens_qmlplugin` and `qindaqt_controls_qmlplugin`. The full build is required only for stage 6 (install). Accidentally running narrow build for stage 6 will fail (missing upstream targets), not false-pass. |
| **CTest discovery duplication** | 7 tests in 1 registry, all distinct names, all distinct labels. Ctest discovery via `-R '^qindaqt\.controls-'` is deterministic; no accidental re-runs or missing tests. |
| **PSS threshold confusion** | PSS measurement explicitly returns `"threshold": null` in JSON. This is **not** an implicit pass. The test always exits 0 and produces JSON; the result must be reviewed separately for anomalies. |
| **Unobserved binding loops** | Nia noted the FormRow `childrenRect` implicit-size pattern is structurally sound but only proven at compile time. Stage 2 (focused behavior run) must capture stderr for runtime binding-loop warnings. If loop warnings appear, they are the first evidence that the watch item materialized. |

## Expected pass counts at full suite completion

- **Source policy:** 1 test (cmake)
- **Behavior:** 1 test (executable)
- **Visual:** 3 tests (100, 125, 150%)
- **PSS measurement:** 1 test (python)
- **Installed import:** 1 test (cmake)
- **Total:** 7 tests, all with `controls` label, all blocking per CMakeLists.txt (none marked SKIP or optional)

## Handoff requirements for next action

Once Cora resolves the **medium findings** (error fixture scope, error semantic coverage, compiler lane availability):

1. Provide the **exact capture output path** for visual test PNGs (material question 1)
2. Provide the **baseline review process** and approval gate (material question 2)
3. Confirm the **fixture scope decision** (add rows vs. amend wiki; material question 3)
4. Confirm the **error semantic decision** (edit QML vs. amend wiki; material question 4)
5. Trigger **compiler lane** when available (material question 5)
6. Collect and commit **baseline images** after visual review
7. Collect **logs/evidence** from all gates (pass/fail, counts, JSON measurement)
8. Create one **exact commit** with all controlled+test files in clean state, ready for independent review

No duplication with Nia Hart (audit complete) or other lanes (Celeste/others are focused on different milestones). My role is **read-only evidence orchestration**; I will not edit, build, or execute tests.

---

**Next status:** Awaiting Cora's action on material questions and compiler lane release. Ready to assist with baseline review, log analysis, or gate sequencing once clarity is provided.
