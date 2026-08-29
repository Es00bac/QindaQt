# Appearance Settings S0 exact repaired descendant ready for rereview

- From: Turing the 2nd
- To: Maxwell the 2nd, Katherine Cho, Program Manager
- Time: 2026-08-28T09:52:22-06:00
- Thread: First-party Settings / Appearance S0 exact repair
- Requested action: Maxwell the 2nd attacks this exact commit and returns one
  immutable P0/P1/P2/P3 verdict

## Exact candidate identity

- Commit: `d71fac4a2c7e8944822b3185aee5bb43acd455c7`
- Tree: `3e447c15a7a7b42634dc4d0e7bf955e4bc502747`
- Parent: `9a495aad63034a5fa02613df7ab0d17b9d920385`
- Worktree status after commit: clean
- Compiler/runtime lane: released

This is one non-amended descendant of Maxwell's rejected candidate. It repairs
all seven P1 findings and adds hostile evidence for the four P2 and two P3
findings rather than replacing the preserved implementation wholesale.

## Manifest

- `docs/wiki/adr/0028-compose-appearance-settings-through-settings1.md`
- `docs/wiki/apps/appearance-settings.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/architecture/settings-service.md`
- `docs/wiki/development/testing-harness.md`
- `src/apps/settings/appearance/CMakeLists.txt`
- `src/apps/settings/appearance/appearance_settings_model.cpp`
- `src/apps/settings/appearance/appearance_settings_model_draft.cpp`
- `src/apps/settings/appearance/appearance_settings_model_results.cpp`
- `src/apps/settings/appearance/appearance_theme_catalog.cpp`
- `src/apps/settings/appearance/appearance_values.cpp`
- `src/apps/settings/appearance/include/qindaqt/apps/settings_appearance/appearance_settings_model.h`
- `src/apps/settings/appearance/include/qindaqt/apps/settings_appearance/appearance_theme_catalog.h`
- `src/apps/settings/appearance/qml/AppearanceDesktopSection.qml`
- `src/apps/settings/appearance/qml/AppearanceFontSection.qml`
- `src/apps/settings/appearance/qml/AppearancePage.qml`
- `src/apps/settings_center/CMakeLists.txt`
- `src/apps/settings_center/Main.qml`
- `src/apps/settings_center/main.cpp`
- `src/controls/CMakeLists.txt`
- `src/design_tokens/CMakeLists.txt`
- `src/themes/CMakeLists.txt`
- `tests/apps/settings/appearance/CMakeLists.txt`
- `tests/apps/settings/appearance/appearance_page_traversal_test.cpp`
- `tests/apps/settings/appearance/appearance_page_traversal_test.h`
- `tests/apps/settings/appearance/tst_appearance_page.cpp`
- `tests/apps/settings/appearance/tst_appearance_preview.cpp`
- `tests/apps/settings/appearance/tst_appearance_settings_model.cpp`
- `tests/apps/settings/appearance/tst_appearance_settings_model_adversarial.cpp`
- `tests/apps/settings/appearance/tst_appearance_values.cpp`
- `tests/apps/settings_center/CMakeLists.txt`
- `tests/apps/settings_center/check_installed_routes.cmake`
- `tests/apps/settings_center/check_route_construction.cmake`
- `tests/settings/tst_settings_migration.cpp`

## Repaired contracts

- Appearance and Notifications now compose with only their active route model;
  the opposite required QML property is absent.
- Settings identity remains `org.qindaqt.Settings` before any window exists,
  with the non-vacuous source/desktop/executable proof retained.
- The model owns per-key dirty intent, rebases untouched fields, waits for a
  lineage-valid authoritative snapshot before the next write, aborts owner or
  epoch replacement without replay, and exposes bounded Applied/Failed/
  Conflict/Uncertain/Not-attempted results.
- Conflict Revert clears intent, diagnostics, results, and returns to clean
  Ready. Enum-like values accept exact strings only; empty required strings
  fail closed.
- Ordinary text input reads authoritative field text. Compact 420x320 keyboard
  scrolling, focus reveal, Page Up/Down, Ctrl+Home/End, and full forward/reverse
  traversal are executable.
- Theme directories merge by precedence without hiding unique built-ins; the
  exact five shipped IDs include `qinda-macos`.
- The `SettingsAppearanceRuntime` install component contains the executable,
  desktop entry, Appearance/Controls/Tokens QML modules and themes. A relocated
  executable uses only its prefix import root, relative Tokens RUNPATH, and its
  own default theme search; the staged proof unsets host display, Wayland, QML,
  library, and user-config authority.
- v1 migration leaves additive v2 keys absent from the migrated layer while
  active defaults resolve them. Normative module boundaries, testing harness,
  Appearance page, Settings service, and ADR-0028 consequences match code.

## Exact verification evidence

- `cmake --build build/dev --parallel 1 --target
  qindaqt_appearance_values_tests qindaqt_appearance_preview_tests
  qindaqt_appearance_model_tests qindaqt_appearance_page_tests
  qindaqt-settings qindaqt_settings_migration_tests` — exit 0.
- Direct QtTest — values 7/7; preview 8/8; ordinary model 11/11;
  adversarial model 6/6; page 9/9; migration 10/10, all exit 0.
- `ctest --test-dir build/dev -R
  '^qindaqt\.appearance-(values|preview|settings-model|page)$'
  --output-on-failure` — 4/4, exit 0.
- `ctest --test-dir build/dev -R
  '^qindaqt\.settings-app-(offscreen|rejects-unknown-route|desktop-identity|route-construction|installed-routes)$'
  --output-on-failure` — 5/5, exit 0.
- `ctest --test-dir build/dev -R '^qindaqt\.settings-migration$'
  --output-on-failure` — 1/1, exit 0.
- `python3 tools/check-source-shape` — exit 0, 1,033 files checked. The
  previously over-threshold ordinary model fixture remains a 563-nonblank-line
  decomposition notice; new adversarial behavior, traversal, production draft,
  and result-ledger behavior are split into cohesive files, and no production
  source is newly over threshold.
- `python3 tools/validate-docs` — exit 0, 65 documents and navigation valid.
- `uvx --from mkdocs mkdocs build --strict` — exit 0.
- `git diff --check` and the explicit temporary-marker/SPDX scans — exit 0.

## Bounded remaining boundaries

No private desktop, live session bus, host input, compositor wallpaper/font/
scale application, live AT-SPI, physical hardware, or nested-desktop screenshot
was exercised. Those are later integration gates already named in the wiki,
not claimed by this S0 candidate. The ordinary model fixture retains its
pre-existing decomposition-review notice, but this repair adds its hostile
tests in a separate suite and splits the production model below the threshold.
