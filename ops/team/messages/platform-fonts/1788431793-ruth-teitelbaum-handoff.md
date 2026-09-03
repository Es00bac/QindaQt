# Ruth Teitelbaum — Font discovery F1 candidate handoff

QQ-005.08 Font discovery and confirmed first-party application (WIRED F0 → F1):
fontconfig discovery provider, Settings1 persistence composition, first-party
bootstrap wiring. Resumed after the Kimi provider limit; the preserved
in-progress commit was verified, completed, and re-evidenced in this session.

## Candidate

- Candidate commit: `abc76f32b5d499b26c6d843bce45cb6dc33ab9b9`
- Candidate tree: `4a6b392ce2390482e71782a1171ae4317cb235df`
- Exact base: `f350028c1cfea0bf0e92f4c2fb0d5949ee6b65a9`
- Branch: `worker/font-discovery-f1`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/font-discovery-f1`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/font-discovery-f1`

## Changed paths (sorted)

```
docs/wiki/adr/0057-confine-fontconfig-behind-font-discovery.md
docs/wiki/adr/index.md
docs/wiki/architecture/font-preferences.md
docs/wiki/architecture/module-boundaries.md
docs/wiki/development/testing-harness.md
mkdocs.yml
README.md
src/CMakeLists.txt
src/apps/file_manager/CMakeLists.txt
src/apps/file_manager/main.cpp
src/apps/settings_center/CMakeLists.txt
src/apps/settings_center/main.cpp
src/apps/terminal/CMakeLists.txt
src/apps/terminal/main.cpp
src/apps/text_editor/CMakeLists.txt
src/apps/text_editor/main.cpp
src/services/font_discovery/CMakeLists.txt
src/services/font_discovery/include/qindaqt/services/font_discovery/font_discovery.h
src/services/font_discovery/src/font_discovery.cpp
src/services/font_preferences/CMakeLists.txt
src/services/font_preferences/include/qindaqt/services/font_preferences/font_settings_bootstrap.h
src/services/font_preferences/include/qindaqt/services/font_preferences/font_settings_bridge.h
src/services/font_preferences/src/font_settings_bootstrap.cpp
src/services/font_preferences/src/font_settings_bridge.cpp
tests/CMakeLists.txt
tests/services/font_discovery/CMakeLists.txt
tests/services/font_discovery/check_boundary.cmake
tests/services/font_discovery/fixtures/LiberationMono-Regular.ttf
tests/services/font_discovery/fixtures/NotoSansLycian-Regular.ttf
tests/services/font_discovery/fixtures/NotoSansOgham-Regular.ttf
tests/services/font_discovery/fixtures/README.md
tests/services/font_discovery/font_discovery_test_support.h
tests/services/font_discovery/installed_consumer/CMakeLists.txt
tests/services/font_discovery/installed_consumer/installed_font_discovery_consumer.cpp
tests/services/font_discovery/run_installed_font_discovery_consumer.cmake
tests/services/font_discovery/tst_font_discovery.cpp
tests/services/font_discovery/tst_font_discovery_bounds.cpp
tests/services/font_discovery/tst_font_discovery_hostile.cpp
tests/services/font_preferences/CMakeLists.txt
tests/services/font_preferences/check_boundary.cmake
tests/services/font_preferences/installed_consumer/CMakeLists.txt
tests/services/font_preferences/installed_consumer/installed_font_consumer.cpp
tests/services/font_preferences/run_installed_font_preferences_consumer.cmake
tests/services/font_preferences/tst_font_settings_bridge.cpp
tests/services/font_preferences/tst_font_settings_bootstrap.cpp
```

## What landed

- `src/services/font_discovery/`: fontconfig-backed producer of F0 `FontFact`
  values. The `FcConfig` is built internally from injected font directories and
  an injected configuration file; bounded counts (`maximumFacts`, default 4096)
  and string lengths (`maximumStringBytes`, default 512, over-long rejects the
  whole pattern fail-closed); deterministic ordering; `available==false` with a
  bounded diagnostic on malformed request, config parse failure, missing
  injected directory, or fontconfig init failure. Fontconfig is a PRIVATE link
  confined to this module (ADR-0057); `productionDefault()` is the only request
  shape that touches host fontconfig configuration.
- `src/services/font_preferences/` additive: `FontSettingsBridge` composes the
  existing coordinator with the public Settings1 client so confirmed `fonts.*`
  preferences round-trip through the documented Settings1 keys with the
  Appearance-route draft/apply/conflict/no-replay truth; fail-closed on
  transport loss; last-known-good coordination stays atomic.
  `FontSettingsBootstrap` is the F1 pre-window helper derived from
  `FontBootstrap`: bounded (750 ms default) synchronous read of the confirmed
  snapshot, applied to the application default font, hinting, and antialiasing;
  every failure path changes nothing.
- Bootstrap wiring: one guarded `FontSettingsBootstrap::applyFromSessionSettings`
  call per first-party `main.cpp` (settings_center, text_editor, file_manager,
  terminal), before any window/QML engine. The shell is untouched.
- Tests: 14 `qindaqt.font-*` rows — catalog, preferences, codec, bootstrap,
  coordinator (F0, unchanged), plus new settings-bridge round trips with a fake
  Settings1 client, settings-bootstrap derivation with/without preferences,
  discovery happy path with vendored OFL fixtures (Noto Sans Lycian/Ogham
  subsets, Liberation Mono), hostile (malformed/missing config, missing
  directory, hostile directory content, over-long strings), bounds
  (deterministic truncation), boundary poison (no fontconfig reference outside
  the provider module, provider stays transport-free), and installed-package
  consumer rows for both modules' public headers.
- Docs: ADR-0057 (references ADR-0047), font-preferences.md F1 sections,
  module-boundaries row for `font_discovery`, testing-harness selector rows,
  README dependency-table row for fontconfig, mkdocs.yml and ADR index entries.

## Evidence (all run this session from the worktree; build root `<ROOT>` =
`/home/cabewse/work_SPaC3/builds/qindaqt/font-discovery-f1`)

Debug (`<ROOT>/debug`):

- `cmake --build <ROOT>/debug --parallel 3 --target qindaqt_font_discovery
  qindaqt_font_preferences qindaqt_font_discovery_tests
  qindaqt_font_discovery_hostile_tests qindaqt_font_discovery_bounds_tests
  qindaqt_font_catalog_tests qindaqt_font_preferences_tests
  qindaqt_font_preferences_codec_tests qindaqt_font_bootstrap_tests
  qindaqt_font_preferences_coordinator_tests qindaqt_font_settings_bridge_tests
  qindaqt_font_settings_bootstrap_tests` — exit 0.
- `cmake --build <ROOT>/debug --parallel 3 --target qindaqt-editor
  qindaqt-file-manager qindaqt-terminal qindaqt-settings` — exit 0.
- `cmake --build <ROOT>/debug --parallel 3 --target <18 app test targets:
  qindaqt_editor_* (6), qindaqt_file_manager_* (4), qindaqt_terminal_* (5),
  qindaqt_settings_route_registry_test, qindaqt_settings_navigation_controller_test,
  qindaqt_settings_navigation_page_test>` — exit 0.
- After `ninja -t clean` of the font libraries and their new test executables,
  a from-source rebuild of those 7 targets — exit 0 (strict warnings as errors).
- `ctest --test-dir <ROOT>/debug -R '^qindaqt\.font-' --output-on-failure
  --no-tests=error` — exit 0, 14/14 passed (run twice: once on the preserved
  build, once on the clean rebuild).
- `ctest --test-dir <ROOT>/debug -R '^qindaqt\.(editor|file-manager|terminal|
  settings-app|settings-navigation|settings-route-registry)'
  --output-on-failure --no-tests=error` — exit 0, 36/36 passed (includes the
  offscreen window rows and installed-metadata rows that execute the freshly
  wired application binaries).

Release (`<ROOT>/release`): same build target sets — exit 0; same clean rebuild
of the 7 font targets — exit 0; `ctest -R '^qindaqt\.font-'` — exit 0, 14/14;
app selector — exit 0, 36/36.

Static gates (from the worktree root):

- `./tools/validate-docs` — exit 0 (118 Markdown documents + mkdocs.yml).
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict
  --site-dir <ROOT>/site` — exit 0.
- `./tools/check-source-shape` — exit 0.
- `git diff --check` and `git diff --check f350028..HEAD` — exit 0.
- `python3 -m json.tool` on each changed JSON — no JSON files changed (check is
  vacuous for this candidate).

## Bounded caveats

- No runtime claim beyond the offscreen/installed test rows above: no host
  desktop, no nested-compositor session, no hardware, no physical DPI/font
  qualification. The rendered typography matrix and assistive-technology proof
  named in the QQ-005.08 caveat remain for later lanes.
- The Settings Center registry route for fonts is deliberately not claimed; two
  Settings-route lanes own that registry right now. The F1 persistence
  composition is proven through the bridge tests with a fake Settings1 client.
- The bootstrap call is placed immediately after `QGuiApplication`
  construction, not strictly before it: Qt D-Bus and `QGuiApplication::setFont`
  both require the application object. This deviation from the "pre-
  QGuiApplication" phrasing is recorded in
  `font_settings_bootstrap.h` and the wiki; it is still pre-window and
  pre-QML-engine, which is the behavior that matters.
- Test fixtures are small OFL fonts vendored under
  `tests/services/font_discovery/fixtures/`; the `tests/controls/fonts` Noto
  files from the controls-visual-fonts lane were not on this base.
- The app-target Debug/Release builds reported "no work to do" on the first
  invocation this session (artifacts current from the preserved session); the
  app behavior evidence rests on the 36 executed test rows per profile, and the
  font targets were clean-rebuilt from source in both profiles.

## Requested next action

Independent exact review of commit
`abc76f32b5d499b26c6d843bce45cb6dc33ab9b9`, then manager integration.
