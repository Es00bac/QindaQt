# Corin Vale — Power applet P1 exact review midpoint

- Timestamp: 2026-08-28T18:00:00Z (2026-08-28 12:00:00 MDT)
- Exact candidate: `d11a69d36c30d5100c3878fd0fa505c792ad1c6b`, tree `d01c92fbfe3b752090ec03eac51a5da74608c02d`, parent `251c62065dcbc393c3d4067858bf28329f1f881d`
- Status: working — full in-tree out-of-tree build running (dev-preset cache variables, `-B` outside the read-only worktree); static/doc gates already complete

## Provenance and collision analysis (complete)

- `git cat-file -t`, `^{tree}`, and `^` on the candidate confirm exact commit/tree/parent match the handoff.
- `git merge-base main d11a69d` = `c498269` = current `main` tip: **main is a direct ancestor of the candidate**, so the candidate is fully rebased onto current main.
- Diff of the 9 changed paths between `merge-base` and `main` is empty: **zero path collision** with anything integrated to main since this line branched.
- `git status --porcelain=v2` / `git diff --stat HEAD` on the read-only worktree show no tracked-file changes, only the harness's own untracked `.omc/`: candidate tree is byte-clean.

## Static/doc gates (complete, all PASS)

- Boundary gate: `cmake -DSOURCE_ROOT=. -P tests/shell/power_applet/check_boundary.cmake` → pass, 10 files scanned.
- Source shape: `tools/check-source-shape --warnings-as-errors` → pass, 1024 files, 0 warnings.
- Doc validation: `tools/validate-docs --root .` → pass, 66 Markdown documents + mkdocs.yml nav.
- **Strict MkDocs** (omitted from Sela's handoff): `mkdocs build --strict --site-dir <out-of-tree>` → pass, built in 1.26s, zero warnings/errors.
- Whitespace: `git diff --check 251c620 d11a69d` → clean.

## Architecture/contract review (complete)

Full diff is 9 files / 41+/16-. Confirmed the CMake wiring is additive-only and matches the documented registry seams. The `projectSupply` repair (adding `bool *degraded` output param, set on invalid handle) is a real, necessary fix: `tst_power_applet_presentation.cpp`'s `boundsAndCapabilityGatesDegradeNotCrash` asserts `anonymousModel.phase == ServicePhase::Degraded` for a zero-epoch supply, which fails without this propagation. `projectSupply` remains anonymous-namespace-local (its only caller is `projectPowerApplet`), so the added default argument is safe.

**One P2 finding**: `src/shell/power_applet/CMakeLists.txt` and `tests/shell/power_applet/CMakeLists.txt` still carry `AGENT-NOTE` header comments stating the module "is not yet wired into the build" / "not yet wired into tests/CMakeLists.txt" — this is now false; Sela's own repair wired both via the additive `add_subdirectory` lines the notes themselves prescribe. Per AGENTS.md, "a stale marker is a defect." Not build/behavior blocking; will factor into final P2 count.

## Remaining work

Out-of-tree dev-preset build (`-DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_STRICT_WARNINGS=ON`) is at 1307/1569 targets with zero errors so far; the `qindaqt_shell_power_applet` static library and all three focused test executables have compiled cleanly with strict warnings (`-Werror`) with no AUTOMOC/moc-input errors (the prior missing-moc symptom Maya Frost/the manager reported does not reproduce once the subdirectories are actually wired, as here). Once the full build finishes I will run the 4 focused applet tests, all adjacent power/brightness CTest rows, and the direct QtTest binaries with executable totals, then post the terminal PASS/FAIL verdict.
