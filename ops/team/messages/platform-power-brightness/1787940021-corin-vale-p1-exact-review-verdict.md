# Corin Vale — Power applet P1 exact review terminal verdict: PASS

- Timestamp: 2026-08-28T18:00:21Z (2026-08-28 12:00:21 MDT)
- Worker: Corin Vale, Power Applet P1 cross-provider exact reviewer (Anthropic Claude Code, exact `claude-sonnet-5`, reasoning: high)
- **Exact candidate commit: `d11a69d36c30d5100c3878fd0fa505c792ad1c6b`**
- **Exact tree: `d01c92fbfe3b752090ec03eac51a5da74608c02d`**
- **Parent: `251c62065dcbc393c3d4067858bf28329f1f881d`** (Mara Voss P1 candidate)
- Author under review: Sela North (Google Antigravity Vertex ADC, `gemini-3.7-flash-high`)
- Read-only worktree: `/home/cabewse/work_SPaC3/container-wm-workers/power-applet-p1-review-corin`
- Out-of-tree build root: `/home/cabewse/work_SPaC3/container-wm-private-agent-runs/corin-power-applet-review/build`

## Verdict: **PASS** — findings P0=0, P1=0, P2=1, P3=0

Recommend the manager integrate exact commit `d11a69d36c30d5100c3878fd0fa505c792ad1c6b` immediately.

## Provenance and collision analysis

- `git cat-file -t d11a69d…` = commit; `d11a69d…^{tree}` = `d01c92fb…`; `d11a69d…^` = `251c620…` — all three match the handoff exactly.
- `git merge-base main d11a69d` = `c498269…` = current `main` tip. **`main` is a direct ancestor of the candidate** (`git merge-base --is-ancestor main d11a69d` succeeds; the reverse fails): the candidate is fully rebased onto everything currently on `main`.
- `git diff --name-only <merge-base> main -- <9 changed paths>` is empty: **zero path collision** with any work integrated to `main` since this line diverged.
- Changed paths (9 total, +41/-16): `docs/wiki/architecture/module-boundaries.md`, `docs/wiki/development/testing-harness.md`, `docs/wiki/shell/power-applet.md`, `src/CMakeLists.txt`, `src/shell/power_applet/src/power_applet_presentation.cpp`, `tests/CMakeLists.txt`, `tests/shell/power_applet/tst_brightness_request_state.cpp`, `tests/shell/power_applet/tst_power_applet_controls.cpp`, `tests/shell/power_applet/tst_power_applet_presentation.cpp`.

## Candidate byte-clean proof

`git status --porcelain=v2` and `git diff --stat HEAD` on the read-only worktree show **no tracked-file changes** at any point during review; the only filesystem entry is the harness's own untracked `.omc/project-memory.json`, outside Git. All generated build/test/doc artifacts were written under `container-wm-private-agent-runs/corin-power-applet-review/` (`build/`, `build.log`, `configure.log`, `ctest-*.log`, `mkdocs-site/`), never inside the candidate worktree.

## Architecture/contract review

`projectSupply` in `power_applet_presentation.cpp` gained a `bool *degraded` output parameter, set when a supply's handle is invalid, and its sole call site in `projectPowerApplet` now passes `&degraded` through to the existing end-of-function `degraded && phase == Ready → Degraded` fold. This is a real, necessary repair, not cosmetic: `tst_power_applet_presentation.cpp`'s `boundsAndCapabilityGatesDegradeNotCrash` asserts `anonymousModel.phase == ServicePhase::Degraded` for a zero-epoch supply — an assertion that fails without this propagation, confirmed by re-deriving the pre-repair code path by hand. `projectSupply` stays anonymous-namespace-local (only caller is `projectPowerApplet` in the same TU), so the added default argument introduces no ODR or header-mismatch risk. The three test-file namespace-alias additions (`namespace Power = QindaQt::Power;` / `namespace Brightness = QindaQt::Brightness;`) and the two `add_subdirectory(shell/power_applet)` CMake registrations are minimal, additive, and match the wiki's documented registry-seam plan exactly. Doc updates (module-boundaries ownership row, testing-harness selector, power-applet.md maturity/registry-seam prose) are accurate to the now-wired state.

Full source-boundary re-read (all headers/sources in `src/shell/power_applet` and `tests/shell/power_applet`, not just the diff) confirms: pure Qt Core + public `power_protocol`/`brightness_model` seams only (no QObject/QML/transport/clocks in production code, confirmed independently of the passing boundary gate); every enum-mapping helper range-checks the raw value before switching (hostile/out-of-vocabulary enumerators degrade to `Unknown`/`Uncertain`, never UB); `BrightnessRequest` lineage/staleness/terminal-immutability rules match the documented contract line-for-line, including owner-loss and epoch-replacement handling; NaN/infinite/out-of-range percentage and rate values are rejected by `trustedPercentage`/`netRateKnown`'s `std::isfinite` + bound checks; the 8-supply bound degrades rather than truncates silently; brightness control rows fail closed (unavailable, non-adjustable) without their composition owner and never fabricate identity without a capability gate.

**One P2 finding (documentation/comment accuracy, not build- or behavior-blocking):** `src/shell/power_applet/CMakeLists.txt` and `tests/shell/power_applet/CMakeLists.txt` still carry header `AGENT-NOTE` comments stating the module "is not yet wired into the build" / "not yet wired into tests/CMakeLists.txt", each also naming the exact `add_subdirectory(shell/power_applet)` seam that Sela's own repair just added. AGENTS.md states plainly: "A stale marker is a defect; update or remove it when the associated constraint changes." Sela updated `docs/wiki/shell/power-applet.md`'s registry-seams section to reflect the wired state but missed these two source comments, which now misdescribe current reality to any future agent reading them. Recommend a trivial follow-up strike/update of both comment blocks; does not block this integration.

## Build (strict warnings, out-of-tree, full dev-preset parity)

```
cmake -S <worktree> -B <private-build-root> -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=ON \
  -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
  -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake --build <private-build-root> -j24
```
Configure exit 0; build exit 0, **1569/1569 targets, zero warnings, zero errors** (`-Werror` active repo-wide via `qindaqt_enable_warnings`). `qindaqt_shell_power_applet` and all three focused test executables compiled cleanly with correct AUTOMOC/.moc generation — the prior "missing generated MOC inputs" symptom the manager and Maya Frost reported against the unwired `251c620` tree does not reproduce once `add_subdirectory(shell/power_applet)` is actually present in both root CMakeLists, confirming that finding's root cause was the missing registration, exactly as Sela's repair addressed.

## Test execution (CTest + direct QtTest, executable totals)

**Focused (4/4 via CTest):** `ctest -R '^qindaqt\.power-applet-'` → `qindaqt.power-applet-presentation`, `qindaqt.power-applet-control-rows`, `qindaqt.power-applet-request-state`, `qindaqt.power-applet-boundary` — 100% passed, 0 failed, 0.05s.

**Adjacent power/brightness (10/10 via CTest):** `ctest -R '(power|brightness)'` → adds `qindaqt.power-protocol-values`, `qindaqt.power-protocol-codec`, `qindaqt.power-aggregation-model`, `qindaqt.brightness-model-math`, `qindaqt.brightness-model-composition`, `qindaqt.brightness-model-boundary` — 100% passed, 0 failed, 0.07s.

**Direct QtTest binary totals (8 binaries; the two remaining CTest rows, `power-applet-boundary` and `brightness-model-boundary`, are static CMake-script gates with no QtTest binary):**

| Binary | Totals |
| --- | --- |
| `qindaqt_power_applet_presentation_tests` | 12 passed, 0 failed |
| `qindaqt_power_applet_controls_tests` | 5 passed, 0 failed |
| `qindaqt_power_applet_request_tests` | 9 passed, 0 failed |
| `qindaqt_power_protocol_values_tests` | 14 passed, 0 failed |
| `qindaqt_power_protocol_codec_tests` | 11 passed, 0 failed |
| `qindaqt_power_aggregation_tests` | 14 passed, 0 failed |
| `qindaqt_brightness_math_tests` | 6 passed, 0 failed |
| `qindaqt_brightness_composition_tests` | 9 passed, 0 failed |

**Grand total: 80/80 direct QtTest assertions passed, 0 failed, 0 skipped, across 8 binaries**, plus 2/2 static boundary-gate CTest rows passed — matching the 10/10 CTest row count above exactly.

## Static and documentation gates (all PASS)

- Boundary gate: `cmake -DSOURCE_ROOT=. -P tests/shell/power_applet/check_boundary.cmake` → pass, 10 files scanned.
- Source shape: `tools/check-source-shape --root . --warnings-as-errors` → pass, 1024 files checked, 0 warnings.
- Doc validation: `tools/validate-docs --root .` → pass, 66 Markdown documents + `mkdocs.yml` navigation validated.
- **Strict MkDocs** (omitted from Sela's handoff): `mkdocs build --strict --site-dir <out-of-tree>` (mkdocs 1.6.1, pre-existing `/tmp/venv`) → pass, built in 1.26s, zero warnings/errors.
- Whitespace: `git diff --check 251c620 d11a69d` → clean, no output.

## Caveats

1. The P2 stale-comment finding above is real but non-blocking; it should be swept in a trivial follow-up, not a repair cycle.
2. No hardware, host power state, session bus, or GUI interaction was exercised, per the module's own non-claims (pure presentation/value-machine slice only) — consistent with the wiki's stated scope.
3. Full repository build (1569 targets, including KWin plugin/compositor/production shell) was run rather than a narrower target list; this exceeds the minimum needed to validate this slice but is strictly additional signal, not a substitute for the focused/adjacent rows reported above, and confirms zero fallout onto unrelated modules.

## Requested next action

Manager: integrate exact commit `d11a69d36c30d5100c3878fd0fa505c792ad1c6b` onto `main` immediately. No blocking findings remain. Corin Vale is not live after this handoff; profile updated accordingly.
