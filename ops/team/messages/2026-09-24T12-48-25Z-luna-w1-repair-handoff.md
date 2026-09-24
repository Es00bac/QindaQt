# Handoff: repair W1 Settings search review findings

- Worker: `luna-w1-repair`
- Branch: `worker/claude-w1-settings-search-20260923`
- Base: `e59d47872001bb45e35b7a582753c9e31ed13fab`
- Candidate commit: `8309c574857913a3622c81e4372836380ec2d14e`
- Worktree: `/home/cabewse/work_SPaC3/container-wm/.cache/claude-plan-20260923/w1-settings-search`

## Outcome

- Active Input capture reports to Settings `Main.qml`, which disables its
  navigation, Ctrl+K, Escape, and Quit shortcuts while the capture is active.
  The capture control claims `ShortcutOverride` for handled keys. The real
  `Main.qml` regression asserts Ctrl+K is assigned, the palette remains closed,
  and Ctrl+1 is assigned without changing the Input route.
- Filtered Settings palette matches are put in one Results section so QindaTK
  section grouping preserves global relevance order; an empty filter retains
  the sidebar groups. Regression assertions cover the cross-section `screen`
  order and the sole `battery` result, Power.
- Updated Settings Center and Input Settings documentation and corrected
  ADR-0257. QindaTK was not modified.

## Changed paths

- `docs/wiki/adr/0257-search-settings-routes-through-a-qindatk-command-palette.md`
- `docs/wiki/apps/input-settings.md`
- `docs/wiki/apps/settings-center.md`
- `src/apps/settings/input/qml/InputPage.qml`
- `src/apps/settings/input/qml/InputShortcutRow.qml`
- `src/apps/settings/input/qml/InputShortcutsSection.qml`
- `src/apps/settings/input/qml/ShortcutCaptureButton.qml`
- `src/apps/settings_center/Main.qml`
- `src/apps/settings_center/SettingsCommandPalette.qml`
- `src/apps/settings_center/SettingsRouteShortcuts.qml`
- `src/apps/settings_center/SettingsSearchCommands.js`
- `tests/apps/settings_center/CMakeLists.txt`
- `tests/apps/settings_center/tst_settings_command_palette.cpp`
- `ops/team/messages/2026-09-24T11-50-12Z-luna-w1-repair-claim.md`
- `ops/team/workers/luna-w1-repair.md`

## Verification

- `cmake --build build/dev -- -j8 -l20` — exit 0; final build reported no work.
- `QT_FATAL_WARNINGS=1 ctest --test-dir build/dev --output-on-failure -R '^qindaqt\.settings-(route-search|command-palette)$'` — exit 0, 2/2 passed.
- `ctest --test-dir build/dev --output-on-failure -R '^qindaqt\.settings-input'` — exit 0, 12/12 passed.
- `ctest --test-dir build/dev --output-on-failure -L settings` — exit 0, 135/135 passed.
- `./tools/validate-docs` — exit 0; validated 389 Markdown documents and `mkdocs.yml` navigation.
- `mkdocs build --strict` — unavailable, exit 127 (`mkdocs` is not installed); no package was installed.
- `ctest --test-dir build/dev --output-on-failure -R 'docs|links'` — exit 0, no matching tests are registered.
- `git diff --check` — exit 0.

## Caveats and review request

The offscreen Qt test helper emits Ctrl as a separate key event and that event
drops active focus from the QML Button before the chord key is delivered. The
Main.qml test therefore invokes the same production `claimsShortcutOverride`
and `handleCapturedKey` handlers on the active scene capture item, then checks
the assigned chord and shell state. It does not simulate native desktop input.
Earlier exploratory tests using direct `QCoreApplication::sendEvent` to a
QQuickItem or QQuickWindow segfaulted in Qt and were discarded; the final CTest
gates above pass. No hardware input check was available.

Requested next action: review exact candidate commit
`8309c574857913a3622c81e4372836380ec2d14e` before integration.
