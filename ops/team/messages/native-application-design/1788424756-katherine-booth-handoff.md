# Katherine Booth — repair handoff: Controls visual gate host independence

- Persona: Katherine Booth, Controls test-fixture implementer (Z.AI GLM
  `zai-coding-plan/glm-5.3`, reasoning high)
- Exact candidate SHA: `b7b52083ab4bb11d36284020d42a6a6e854db260`
- Candidate tree SHA: `ff493f6df69fdae4484a9b41b7150ef4411dbc9d`
- Exact base SHA (candidate parent): `cf2c2a9e00513b994c077cd451b774d1cbad0a34`
  (repair descendant of rejected candidate `bf1c83a15206c190d0c56950c261c1ada0196281`;
  original lane base `ce9228d9694622d503d92a38d01986f8f124f188` unchanged)
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/controls-visual-fonts`
- Branch: `worker/controls-visual-fonts`

## What changed (repairs P1-1, P2-1, P2-2)

1. **P1-1 (host-font bypass).** `pinDeterministicFonts()` now rewrites every
   `data/themes/*.json` `fontFamily`/`monoFontFamily` onto the registered
   repository families (`QindaQt Sans` / `QindaQt Sans Mono`) into pinned
   runtime theme copies under the test build tree; `publishTheme()` loads only
   those copies, and `QFont::insertSubstitution` is kept for the original
   catalog names. The rewrite is required because a Qt substitution cannot
   redirect a family the host has installed (probe on Qt 6.11.1: requesting
   `Noto Sans` with a substitution installed resolves the host face — the
   exact mechanism of the bypass). New canary row
   `qindaqt.controls-visual-no-noto-100-qinda-high-contrast-compact` reruns the
   bypassing row under the checked-in `tests/controls/fontconfig/no-noto/fonts.conf`
   via `FONTCONFIG_FILE` (includes the standard host configuration, rejects
   exactly `Noto Sans`/`Noto Sans Mono`; `XDG_CACHE_HOME` is confined to the
   build tree). The `/dev/null` determinism claim was removed from the docs.
2. **P2-1 (invalid sfnt checksums).** `rename_family_names.py` now recomputes
   every table directory checksum and `head.checkSumAdjustment` per the
   OpenType specification; the four vendored files were regenerated from the
   upstream host builds. `qindaqt.controls-font-pinning` now also validates
   those checksums, requires direct registered-family requests to serve the
   vendored bytes, and verifies the pinned theme copies differ from the
   product catalog only in the two family fields.
3. **P2-2 (unscoped installed-consumer install).** The row installs only the
   new `ControlsQmlModules` component (declared in `tests/controls/CMakeLists.txt`
   because the product rules for those two QML modules live in the broader
   `SettingsAppearanceRuntime` component; mirrors the TextEditor per-consumer
   precedent). The exact focused targets the 34-row selector requires are
   documented in the testing-harness page.

## Changed paths (sorted)

- `docs/wiki/adr/0021-isolate-controls-visual-rows.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/controls.md`
- `tests/controls/CMakeLists.txt`
- `tests/controls/control_test_support.cpp`
- `tests/controls/control_test_support.h`
- `tests/controls/fontconfig/no-noto/fonts.conf` (new)
- `tests/controls/fonts/NotoSans-Bold.ttf`
- `tests/controls/fonts/NotoSans-Regular.ttf`
- `tests/controls/fonts/NotoSans-SemiBold.ttf`
- `tests/controls/fonts/NotoSansMono-Regular.ttf`
- `tests/controls/fonts/README.md`
- `tests/controls/fonts/rename_family_names.py`
- `tests/controls/run_installed_controls_consumer.cmake`
- `tests/controls/tst_controls_font_pinning.cpp`

No `src/` path, no `data/themes/**`, no baseline PNG, and no `mkdocs.yml`
entry changed: the 25 regenerated baselines are byte-identical to the reviewed
set (the vendored glyph tables match the current host Noto build), so
`git diff cf2c2a9..b7b5208 -- tests/controls/baselines` is empty.

## Evidence (all commands run in the assigned worktree/build root)

Debug (`<ROOT>` = `/home/cabewse/work_SPaC3/builds/qindaqt/controls-visual-fonts`):

- `cmake -S . -B <ROOT>/debug -G Ninja -C .../qindaqt-665-initial-cache.cmake
  -DCMAKE_BUILD_TYPE=Debug ...` — exit 0.
- `cmake --build <ROOT>/debug --parallel 3 --target qindaqt_controls_visual_tests
  qindaqt_controls_behavior_tests qindaqt_controls_font_pinning_tests
  qindaqt_controls_font_fixture_negative_tests qindaqt_controls_memory_probe
  qindaqt_controls_bare_memory_probe qindaqt_controls_qml qindaqt_tokens_qmlplugin
  qindaqt_controls_qmlplugin` — exit 0 (26/26 incremental steps; the same
  recipe in a fresh tree is 118/118 below).
- Red control before repair: `QT_QPA_PLATFORM=offscreen
  <ROOT>/debug/tests/controls/qindaqt_controls_font_pinning_tests
  vendoredFixturesCarryValidSfntChecksums` — failed as designed on the
  unrepaired fonts: `NotoSans-Regular.ttf table name checksum stored 0xa9d6c8ce
  calculated 0xab9bcb53` (Totals: 2 passed, 1 failed).
- `cp /usr/share/fonts/noto/{NotoSans-Regular,NotoSans-SemiBold,NotoSans-Bold,NotoSansMono-Regular}.ttf
  tests/controls/fonts/ && python3 tests/controls/fonts/rename_family_names.py` —
  exit 0; `python3 /home/cabewse/work_SPaC3/builds/qindaqt/review-fonts-codex/font-checksums.py`
  (reviewer's own validator) — all four files `table_mismatches=[]`,
  `whole_font=0xb1b0afba`.
- Post-repair pinning executable run — 6/6 passed, exit 0.
- Baseline regeneration loop: all 25 rows with `QINDAQT_UPDATE_CONTROLS_BASELINES=1`
  — all exit 0; `git status --porcelain tests/controls/baselines/` empty
  (byte-identical regeneration; zero baseline churn to review).
- `ctest --test-dir <ROOT>/debug -R '^qindaqt\.controls-visual-no-noto-100-qinda-high-contrast-compact$'`
  — 1/1 passed, exit 0 (the row that failed 0/1 with 145,270-pixel drift in the
  review).
- Positive control: with the same `FONTCONFIG_FILE` proven active
  (`fc-match 'Noto Sans'` → Liberation Sans), the light-compact row passed 1/1,
  exit 0.
- `ctest --test-dir <ROOT>/debug -R '^qindaqt\.controls-visual-'` — run twice
  back-to-back: 26/26 both times (25 rows + canary), exit 0; a third
  count-confirmed run also 26/26.
- `ctest --test-dir <ROOT>/debug -R '^qindaqt\.controls-'` — 34/34 passed,
  exit 0 (`ctest -N`: 34 total, 26 visual).

Release:

- Configure (same flags, `-DCMAKE_BUILD_TYPE=Release`) — exit 0; focused
  build — exit 0, zero error/warning lines in the log.
- `ctest --test-dir <ROOT>/release -R '^qindaqt\.controls-visual-'` — 26/26,
  exit 0; `-R '^qindaqt\.controls-'` — 34/34, exit 0.

Reviewer's P2-2 scenario reproduced from a clean tree (`<ROOT>/debug-fresh`):

- Configure — exit 0; the exact focused target build above — 118/118 steps,
  exit 0; `ctest -R '^qindaqt\.controls-'` — 34/34 passed, exit 0, while
  `src/profiles/libqindaqt_profiles.a` and `src/shell_layout/libqindaqt_shell_layout.a`
  do not exist in that tree. The staged prefix contains only the
  `QindaQt/Controls` and `QindaQt/Tokens` payload plus the consumer QML.

Font identity checks (python, build-root scratch):

- All tables of the regenerated vendored files are byte-identical to
  `/usr/share/fonts/noto` except `name` (the renames) and `head`, which
  differs only in bytes 8–11 (`checkSumAdjustment`).
- `fc-scan` reports `QindaQt Sans` Regular/SemiBold/Bold and `QindaQt Sans
  Mono` Regular for the four files.
- `FONTCONFIG_FILE=tests/controls/fontconfig/no-noto/fonts.conf fc-list
  :family='Noto Sans'` and `:family='Noto Sans Mono'` — 0 fonts each;
  `fc-match sans-serif` → Liberation Sans, `fc-match 'DejaVu Sans'` → DejaVu
  Sans (fallback preserved).

Static gates (from the worktree root):

- `./tools/validate-docs` — exit 0 (118 documents, navigation).
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict
  --site-dir <ROOT>/site` — exit 0.
- `./tools/check-source-shape` — exit 0 (1,816 files; only the two pre-existing
  decomposition warnings the reviewer also observed).
- `git diff --check` — exit 0.
- `python3 -m json.tool` — not applicable: no committed JSON changed (the
  pinned theme copies are runtime artifacts under the build root).

No nested-compositor row, host D-Bus service, hardware, uinput, or network
call was run.

## Remaining bounded caveats

- The canary proves independence from the host families the theme catalog can
  name (`Noto Sans`, `Noto Sans Mono`), not from the whole host font stack:
  the documented host fontconfig rasterization rules and the DejaVu fallback
  glyph remain a dependency, visible as baseline drift rather than a silent
  pass (unchanged ADR-0021 consequence).
- The four vendored files were renewed from the current host
  `/usr/share/fonts/noto` builds; relative to the previously vendored copies
  only the `name` table layout and the recomputed checksums differ (glyph
  tables identical), and the baselines are byte-identical.
- `QFont::insertSubstitution` is retained for the original catalog names but
  only redirects families absent from the host; a host that actually installs
  `Inter` would fail the pinning substitution check (pre-existing property).
- The selector count moved from 33 to 34 rows (the canary); the
  `^qindaqt\.controls-visual-` prefix now matches 26 rows and includes the
  canary on every run.

## Requested next action

Independent exact review of candidate `b7b52083ab4bb11d36284020d42a6a6e854db260`
by the same reviewer (Jean Hall, OpenAI Codex), then manager integration.
