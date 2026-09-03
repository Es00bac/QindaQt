# Katherine Booth — QQ-006.02 Controls visual gate host independence — handoff

- **Thread:** native-application-design
- **From:** Katherine Booth (`katherine-booth`, Z.AI GLM `zai-coding-plan/glm-5.3-flash`, reasoning high), resuming the lane a second time after the GLM and Kimi provider outages.
- **Lane:** QQ-006.02 Reusable QindaQt.Controls — byte-pinned fonts instead of host font substitution.
- **Date:** 2026-09-03T01:49:29-06:00

## Candidate

- **Exact candidate commit:** `bf1c83a15206c190d0c56950c261c1ada0196281`
- **Exact candidate tree:** `572e5be107963f2a16a9322e86ce7c9835d9b6f4`
- **Exact base:** `ce9228d9694622d503d92a38d01986f8f124f188` (the `main` tip when the lane was claimed; recorded in the first-party queue row)
- **Branch:** `worker/controls-visual-fonts`
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/controls-visual-fonts`
- **Build root:** `/home/cabewse/work_SPaC3/builds/qindaqt/controls-visual-fonts`

Commit chain on the branch (oldest first): `74b1246` (original GLM WIP preserve, by the manager),
`f5b182c` (Kimi continuation WIP preserve, by the manager; also carries the manager's
byte-for-byte sync of already-integrated work — untouched by me), `bf1c83a` (my product commit:
sfnt `numTables` width hardening in the pinning probe, with an `AGENT-GUARD`). A following
coordination-only commit adds this message and the worker record; review attacks `bf1c83a`.

## Changed paths (sorted, base → candidate, owned paths only)

- docs/wiki/adr/0021-isolate-controls-visual-rows.md
- docs/wiki/development/testing-harness.md (Controls sections only)
- docs/wiki/shell/controls.md
- tests/controls/CMakeLists.txt
- tests/controls/control_test_support.cpp
- tests/controls/control_test_support.h
- tests/controls/fonts/LICENSE-OFL.txt
- tests/controls/fonts/NotoSans-Bold.ttf
- tests/controls/fonts/NotoSansMono-Regular.ttf
- tests/controls/fonts/NotoSans-Regular.ttf
- tests/controls/fonts/NotoSans-SemiBold.ttf
- tests/controls/fonts/README.md
- tests/controls/fonts/rename_family_names.py
- tests/controls/run_controls_font_fixture_negative.cmake
- tests/controls/tst_controls_font_fixture_negative.cpp
- tests/controls/tst_controls_font_pinning.cpp
- tests/controls/tst_controls_visual.cpp
- tests/controls/baselines/{100,125,150}/*.png (25 regenerated baselines)
- ops/team/workers/katherine-booth.md (coordination commit)
- ops/team/messages/native-application-design/1788421769-katherine-booth-handoff.md (coordination commit)

`f5b182c` additionally contains the manager's sync of unrelated integrated files
(`src/shell/**`, `ops/team/**`, other wiki pages, and so on); those are byte-identical to what the
manager committed and I did not modify them. No product source under `src/` was changed by this
lane. No JSON files were changed, so the `python3 -m json.tool` gate had nothing to check.
`mkdocs.yml` is unchanged (no page added).

## Outcome delivered

1. `tests/controls/fonts/` vendors Noto Sans Regular/SemiBold/Bold and Noto Sans Mono Regular
   plus `LICENSE-OFL.txt` (SIL OFL 1.1), `README.md` (provenance and renewal procedure), and
   `rename_family_names.py`. The vendored name records declare the repository-owned families
   `QindaQt Sans` / `QindaQt Sans Mono`, so no host-installed font can shadow or collide with the
   fixture regardless of Qt/fontconfig match order. The file set is minimal and justified from the
   QML: only `Font.Normal`/`Font.DemiBold` (`src/controls/qml/Button.qml:44` and four more
   `font.weight` sites) and `font.bold: true` (`ThemeCard.qml:168`, `CheckBox.qml:44`) occur; no
   italic is used anywhere.
2. `pinDeterministicFonts()` (`tests/controls/control_test_support.cpp`) registers the four files
   with `QFontDatabase::addApplicationFont`, requires every registration to expose exactly the
   expected family, substitutes `Inter`/`JetBrains Mono` to the renamed families, fixes the C
   locale, and aborts via `qFatal` with the path on a missing, unreadable, or wrongly named
   fixture. `QINDAQT_CONTROLS_FONT_DIR` is passed through
   `tests/controls/CMakeLists.txt` next to `QINDAQT_CONTROLS_BASELINE_DIR`.
3. Four focused guard rows: `qindaqt.controls-font-pinning` (the engine's resolution of `Inter`
   must be `QindaQt Sans` Regular serving a `name` table byte-identical to the vendored Regular
   file) and `qindaqt.controls-font-fixture-{missing,corrupt,wrongfamily}` (each hostile fixture
   mode must abort the process with the matching diagnostic).
4. All 25 baselines regenerated; a re-run of the update pass (`QINDAQT_UPDATE_CONTROLS_BASELINES=1`)
   left `git status` empty, proving the committed baselines are byte-identical to a fresh render.
5. Docs updated in the same change: ADR-0021 gained the dated "Amended (2026-09-02): fonts are
   byte-pinned fixtures" section (history preserved); the Controls sections of the testing-harness
   page describe the pin, the four guard rows, and the recorded `FONTCONFIG_FILE=/dev/null`
   determinism command; `docs/wiki/shell/controls.md` documents the vendored files and the OFL
   license (this wiki has no separate third-party-content page; `controls.md` is the page that
   carries the vendored-font provenance).

## Verification evidence (all commands run by me in this session, exit statuses included)

Build root `<ROOT>` = `/home/cabewse/work_SPaC3/builds/qindaqt/controls-visual-fonts`.

- Configure Debug and Release with the standard recipe + initial cache: both exit 0.
- `cmake --build <ROOT>/debug --parallel 3 --target qindaqt_controls_visual_tests
  qindaqt_controls_behavior_tests qindaqt_controls_font_pinning_tests
  qindaqt_controls_font_fixture_negative_tests qindaqt_controls_memory_probe
  qindaqt_controls_bare_memory_probe qindaqt_controls_qml qindaqt_tokens_qmlplugin
  qindaqt_controls_qmlplugin` → exit 0. Same for `<ROOT>/release` → exit 0. Both targets rebuilt
  again after the final source edit (Debug and Release pinning executables) → exit 0.
- Baseline regeneration pass: `QINDAQT_UPDATE_CONTROLS_BASELINES=1 ctest --test-dir <ROOT>/debug
  -R '^qindaqt\.controls-visual-'` → 25/25 passed; `git status --short tests/controls/baselines`
  afterwards: empty (committed baselines are byte-identical to a fresh regeneration).
- Required matrix on the final tree:
  - Debug `ctest -R '^qindaqt\.controls-visual-'` run 1: **25/25 passed**, exit 0.
  - Debug same selector run 2 (consecutive): **25/25 passed**, exit 0.
  - Release same selector: **25/25 passed**, exit 0.
  - Debug `-R '^qindaqt\.controls-'`: **33/33 passed**, exit 0 (behavior, 25 visual, font pinning,
    3 fixture negatives, source policy, installed import, PSS).
  - Release `-R '^qindaqt\.controls-'`: **33/33 passed**, exit 0.
  - `FONTCONFIG_FILE=/dev/null ctest -R '^qindaqt\.controls-visual-100-qinda-light-compact$'`:
    **1/1 passed**, exit 0 (matches the command recorded in the testing-harness page).
  - Debug `-R '^qindaqt\.controls-font-'`: **4/4 passed** (pinning + three negative modes).
- Negative-control proof (mutation): temporarily replacing the `Inter` substitution target with
  the host family `Noto Sans` and rebuilding made `qindaqt.controls-font-pinning` **fail**
  ("substitution resolved to family noto sans", 0/1); reverting the mutation and rebuilding
  restored 4/4 on the font selector. The tree is clean of the mutation (`git status` verified).
- Static gates, run from the worktree root after the final edit, all exit 0:
  `./tools/validate-docs`; `mkdocs build --strict --site-dir <ROOT>/site` (docs venv binary);
  `./tools/check-source-shape`; `git diff --check`.
- Independent fixture-integrity check (outside the repo): for each of the four vendored TTFs, all
  sfnt tables except `name` are SHA-256-identical to the current host `/usr/share/fonts/noto/`
  builds (16/17 tables each); only the renamed name records differ. `fc-scan` confirms the
  declared families are exactly `QindaQt Sans` (Regular, SemiBold, Bold) and
  `QindaQt Sans Mono` (Regular).

## Baseline review (outcome item 6)

I compared every regenerated baseline against the pre-lane version at the base commit
(`git show ce9228d:<path>`) with a PIL script kept under the build root
(`<ROOT>/baseline-review-2/`), not in the worktree:

- All 25 rows: old and new dimensions identical (e.g. 420x840, 720x840, 1080x840, 900x1050,
  1080x1260); zero rows with a size change (script exit 0).
- Drift is small and text-shaped: 5,746–12,430 changed pixels per row (0.63%–1.67%), always
  inside the text column bounding boxes.
- I visually inspected old|new|diff composites for one row per scale —
  `100/qinda-light-compact`, `125/qinda-dark-ordinary`, `150/qinda-high-contrast-ordinary`
  (the max-drift row). Layout, element geometry, and every line-break decision are identical
  between old and new; the diff heatmap shows glyph-shape ghosting at unchanged text positions
  only.

**Finding: only glyph rendering changed; no layout changed.** This is the expected
old-Noto → new-Noto glyph rasterization drift that motivated the lane. No stop-and-report
condition occurred.

## Remaining bounded caveats (deliberate non-claims)

- No live host-desktop, hardware, nested-compositor, uinput, AT-SPI, or network evidence; the
  lane's ground rules prohibit it and none was performed.
- The gate still depends on the documented host fontconfig configuration for rasterization
  parameters and the Latin-supplement fallback glyph (a DejaVu face). This dependency is now
  explicit in ADR-0021's amendment: a host fontconfig or DejaVu change remains visible as
  reviewed baseline drift, not as a silent pass.
- `FONTCONFIG_FILE=/dev/null` passes because fontconfig falls back to the standard
  configuration; it witnesses the absence of a configuration-file path dependency, not font byte
  pinning (documented in the testing-harness page and the ADR amendment).
- The branch bases on `ce9228d` while `main` has since advanced (`9831d42`). `main` also touched
  `docs/wiki/development/testing-harness.md` (other lanes' sections), so the integration merge
  may need conflict resolution there; the Controls sections on this branch are the authoritative
  ones for this outcome.
- Vendored font provenance: the four files are the current upstream Noto builds (post the
  2026-09-02 host package update) with only their name records rewritten; the fonts README
  records the renewal procedure for future agents.

## Requested next action

Independent exact review of `bf1c83a` (diff `ce9228d..bf1c83a`), then manager integration.
