# Independent review: W1 Settings command palette

- Verdict: **REJECT**
- Exact candidate: `e59d47872001bb45e35b7a582753c9e31ed13fab`
- Exact base: `2e415cad0d1c2ece5b3e3af6962e1d51a9b6f347`
- Worktree: detached at the candidate in `.cache/claude-plan-20260923/review-luna-review-w1`

The candidate builds and its focused and full Settings test suites pass. I found two blocking behavior and contract mismatches in independent reproductions.

## Blocking findings

1. **Ctrl+K opens the palette and is also recorded by active shortcut capture.**

   `src/apps/settings_center/SettingsRouteShortcuts.qml:30-33` installs an unqualified window-context Ctrl+K `Shortcut`. The Input shortcut capture at `src/apps/settings/input/qml/ShortcutCaptureButton.qml:99-105` handles the later key press with `Keys.onPressed`, but does not claim `ShortcutOverride`. With Input → Shortcuts capture active, pressing Ctrl+K opens the modal palette and the same key event reaches the capture handler, which can save Ctrl+K as the custom command shortcut and loses focus to the palette. A Qt Quick window reproduction exited 0 with:

   ```text
   capture focus + modal popup: shortcut=1 capture-handler=1 popup-handler=0 popup-open=1
   ```

   This contradicts ADR-0257's claim at `docs/wiki/adr/0257-search-settings-routes-through-a-qindatk-command-palette.md:89-91` that a capture control without `ShortcutOverride` simply loses Ctrl+K to the palette. Disable the global shortcut during active capture or make capture claim the override, then add a regression through the real Settings `Main.qml` proving the keypress cannot both open search and mutate the captured shortcut.

2. **QindaTK section grouping breaks the documented cross-section relevance order.**

   `src/apps/settings_center/SettingsSearchCommands.js:94-118` sorts all commands globally by the advertised rank, but `Tk.CommandPalette` regroups filtered commands by section. For query `screen`, the global order is Screen saver (title prefix), Display (exact keyword), Login screen (label substring), Power (keyword prefix). The installed QindaTK palette displays `screensaver, login-screen, display, power`, because it groups the two General results ahead of Hardware. The offscreen reproduction exited 0 and printed:

   ```text
   screen query row order: screensaver, login-screen, display, power
   ```

   This violates the visible row-order contract in `docs/wiki/apps/settings-center.md:281-283`. The battery assertion at `tests/apps/settings_center/tst_settings_command_palette.cpp:211-219` does not exercise ranking: About's “battery health” is in its description, which is not supplied as a search keyword, so Power is the only battery match. Preserve the relevance order in the visible result list (for example, put filtered matches in one section) and add an overlapping cross-section query assertion. If only first-Enter selection is intended, revise the documented and tested contract accordingly.

## Non-blocking notes

- W1's `Tk.CommandPalette` API requires QindaTK `0.1.0-r4` or newer; installed QindaTK is r5. The desktop ebuild currently has a lower bound of r2 and must be raised to at least r4 for W1. If shipping the plan's W4 KeyCap work in the same desktop build, use r5.
- The `InputPage.qml` `onInitialDestinationChanged` addition is a small and appropriate bridge for a destination request arriving while Input is already loaded. It ignores the controller's empty intermediate value and opens valid destinations; the new test covers switching and repeating destinations. Route registration order and digit shortcuts remain stable, the ADR-0250 construction witness is unchanged, and the Bluetooth Escape condition remains intact.
- `src/apps/settings_center/Main.qml` has 384 non-blank lines against the source-shape limit of 350; the base already had 377. `./tools/check-source-shape` exits 1 for the repository (41 errors, 99 warnings), and Main.qml is the only changed path it flags. The seven-line increase is cohesive shell wiring and remains below the project-wide 500-line decomposition review threshold, so I treat this as a non-blocking existing shape debt that grew slightly.
- `mkdocs build --strict` could not run because `mkdocs` is not installed; I did not install packages. `./tools/validate-docs` passed. The all-target build also printed `qmlimportscanner` missing-file messages for the installed GlobalMenuActionEntry QML path in unrelated targets; the build completed successfully, and both new tests passed under `QT_FATAL_WARNINGS=1`.

## Verification

- `cmake --preset dev` — exit 0.
- `cmake --build build/dev -- -j8 -l20` — exit 0.
- `QT_FATAL_WARNINGS=1 ctest --test-dir build/dev --output-on-failure -R '^qindaqt\.settings-(route-search|command-palette)$'` — exit 0, 2/2 passed.
- `ctest --test-dir build/dev --output-on-failure -L settings` — exit 0, 135/135 passed.
- `./tools/validate-docs` — exit 0; validated 389 Markdown documents and `mkdocs.yml` navigation.
- `mkdocs build --strict` — unavailable (`mkdocs: command not found`); no package was installed.
- `./tools/check-source-shape` — exit 1; 41 errors and 99 warnings across the tree, with changed `Main.qml` at 384 lines (base: 377; configured limit: 350).
- `git diff --check 2e415cad0d1c2ece5b3e3af6962e1d51a9b6f347 e59d47872001bb45e35b7a582753c9e31ed13fab` — exit 0.
- `QT_QPA_PLATFORM=offscreen build/dev/shortcut_override_repro` — exit 0; reproduced the simultaneous shortcut and capture handlers above.
- `QT_QPA_PLATFORM=offscreen build/dev/palette_order_repro` — exit 0; reproduced the `screen` row order above.
