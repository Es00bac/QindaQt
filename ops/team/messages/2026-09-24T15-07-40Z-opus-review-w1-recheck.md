# Independent review recheck: W1 Settings command palette

- Reviewer: `opus-review-w1-recheck` (Claude Opus). I took over the recheck from `luna-review-w1`, which is no longer available.
- Verdict: **ACCEPT**
- Exact candidate: `fdeaa75f89ae34e79ee1890e8636978fec4ff24e` (implementation `8309c574857913a3622c81e4372836380ec2d14e`; the head adds only ops records)
- Previous review: `e59d47872001bb45e35b7a582753c9e31ed13fab` (REJECT, `2026-09-24T11-43-18Z-luna-review-w1-review.md`)
- Base: `2e415cad0d1c2ece5b3e3af6962e1d51a9b6f347`
- Worktree: detached at the candidate in `.cache/claude-plan-20260923/review-opus-review-w1-recheck`

I read all of `git diff e59d4787 fdeaa75f` and rechecked the rest of the candidate against `2e415cad`. Both earlier blocking findings are fixed. I reproduced both fixes with real key events, not only with the handler calls that the committed test uses. I found no new blocking defect.

## Earlier blocking findings

1. **Ctrl+K was recorded by capture and also opened the palette: fixed.** While capture is active, `ShortcutCaptureButton.qml:105-114` accepts `ShortcutOverride` for every non-zero key. `Main.qml:89-103` keeps `inputShortcutCaptureActive` set. That flag disables the route shortcuts (`Main.qml:108`, and through `SettingsRouteShortcuts.qml` also Alt+Left, Ctrl+K and Ctrl+digit), Escape (`Main.qml:167`) and Quit (`Main.qml:182`). The flag clears through `Qt.callLater`, so the key event that ends capture cannot also fire a shortcut. A generation counter stops a stale clear from overriding a newer capture. I sent real key events through the window's shortcut path with `QTest::keyClick` (a separate Ctrl press, then the key) and with single `QTest::sendKeyEvent` presses. I tested both the row capture and the custom-command capture on the real `Main.qml`:
   - Ctrl+K (click and single event, in both captures), Ctrl+1 (click and single event), Alt+Left, and Ctrl+Q: 8 of 8 rows passed. In each row the palette stayed closed, the route stayed Input, the window did not close, the chord was assigned (`67108939` = Ctrl+K), and the shell shortcuts were enabled again afterwards.
   - Override-only check: during capture I forced `settingsRouteShortcuts.enabled = true`, so the Ctrl+K `Shortcut` was live. Ctrl+K was still recorded, and the palette stayed closed. The capture's `ShortcutOverride` claim is therefore enough on its own. As a control, the same Ctrl+K opened the palette once capture had ended.
   - Exit paths that re-enable the shortcuts: commit, Escape, Tab, focus moved to another item, window deactivation, route change (InputPage destroyed), a model reset that replaces the capturing delegate, and starting another capture. During the handover to a second capture, the shortcuts stayed disabled while the second capture was active. All passed with `QT_FATAL_WARNINGS=1`.
2. **Section grouping scrambled the relevance order: fixed.** `SettingsSearchCommands.js:123-130` (`paletteCommands`) re-sections every match under one translated "Results" section when the filter is not empty. It leaves an empty or whitespace-only filter untouched. I dumped the real palette rows, headers included:
   - empty and `"  "`: `[General] … [Personalization] … [Hardware] …`, the sidebar grouping, with Input's destinations after Input;
   - `screen`: `[Results] screensaver, display, login-screen, power, streaming, input/touch`, which matches the order documented in `settings-center.md` and asserted by the new test;
   - `battery`: `[Results] power` only, with Power as the current row. The About description is not indexed, as the docs now say;
   - `key`: `[Results] input/keyboard, input/shortcuts, power`.

## Blocking findings

None.

## Non-blocking notes

1. **The Input composition is now built at every Settings launch.** `Main.qml:36` `property var inputSettings: InputRouteComposition` evaluates the singleton when the window is created. Before this change, only the `InputPage` component named it, so it was built only when Input opened. I checked this: with the route set to Notifications and no `inputPage` in the scene, `Main.inputSettings` already held a live `QindaQt::Apps::SettingsInput::InputRouteComposition`. Its constructor starts the tablet-mapping Settings1 client (`input_route_composition.cpp`, the `tabletSettingsClient.start` block) and builds the touch client and the KWin tablet port. The composition's own contract says construction is cheap, so I do not treat this as blocking. It is an unrequested startup change made only so the test can inject a fixture. It also makes `settings-center.md:210-211` ("like Input, binds the page to the route's composition singleton inside its component") inaccurate. Suggested fix: use `property var inputSettings: null` in Main, and `inputSettings: root.inputSettings !== null ? root.inputSettings : InputRouteComposition` inside the `inputRouteComponent`.
2. **A section swap inside Input leaves the capture flag set (latent).** I started a capture, then called `navigation.selectRouteDestination("input", "keyboard")` while the capture button still had focus. The Loader at `InputPage.qml:167` destroyed the Shortcuts section, but `false` never reached Main: `inputShortcutCaptureActive` stayed `true`, and route, search, Escape and Quit shortcuts stayed disabled until Input was left. I ran this row without `QT_FATAL_WARNINGS`, because my shortcuts-only fixture has no keyboard models and the Keyboard section logs TypeErrors. As far as I can find, no production path reaches this today. Destination tabs are `StrongFocus` (`src/controls/qml/TabButton.qml:19`), so clicking one ends capture first. The palette takes focus when it opens. `--destination` is read only at startup in `main.cpp`, and Display's pen link runs only from the Display route. A future runtime deep link would expose the bug, though. Suggested hardening: have `InputPage` report `false` when `sectionLoader` changes items, for example `onItemChanged: root.shortcutCaptureActivityChanged(false)`.
3. **The committed test does not exercise real key delivery.** The committed test calls `claimsShortcutOverride` and `handleCapturedKey` directly (`tst_settings_command_palette.cpp:192-207`, used at `:404` and `:423`). So the `ShortcutOverride` path and the Qt shortcut map are never run by the committed test. In my runs, the handoff's caveat that "the offscreen Ctrl helper drops QML Button focus" did not reproduce. `QTest::keyClick(window, Qt::Key_K, Qt::ControlModifier)` on the same `Main.qml` fixture kept capture focus and recorded the chord. I recommend switching the regression to real key events and fixing that comment. The docs' coverage wording ("Ctrl+K and Ctrl+1 captured without invoking shell shortcuts") holds only because the shortcuts are also asserted disabled.
4. **File sizes.** `Main.qml` now has 414 non-blank lines: 377 at the base, 384 at `e59d4787`. `./tools/check-source-shape` flags it against its 350 limit. That error existed at the base, but this repair added 30 lines of capture wiring. The file is still below the AGENTS.md 500-line review threshold. The wiring could move into a small shell component. `tst_settings_command_palette.cpp` has 511 non-blank lines and hits the 500-line decomposition-review warning. The capture regression could move into its own test file, split by behaviour.
5. **Packaging.** W1 needs QindaTK ≥ `0.1.0-r4` (`CommandPalette`), and W4 needs r5 (`KeyCap`). The desktop package must depend on r5 when both ship.
6. `mkdocs build --strict` could not run because `mkdocs` is not installed. I did not install anything. `./tools/validate-docs` passed. The only build warning was ninja's "premature end of file; recovering". It came from `.ninja_log` after I interrupted an earlier build of my own, not from the candidate.

## Merge report

- **Against hub `main` `3dfec4c1` (W0 and W6 integrated):** `git merge-tree --write-tree hub/main fdeaa75f` exits 1, with conflicts only in `docs/wiki/adr/index.md` and `mkdocs.yml`. Both branches append one line after ADR-0256: main adds ADR-0259 (screen saver preview), and W1 adds ADR-0257. Keep both, in numeric order (0257, then 0259). All code merges cleanly. Main's code changes since `2e415cad` are in the screen saver route and the network secret agent, which do not overlap W1's paths.
- **Against W4 `worker/claude-w4-keycap-20260923` `e41a4817`:** `git merge-tree --write-tree e41a4817 fdeaa75f` exits 0 (tree `5fda679c`), so there is no textual conflict. The merged `InputShortcutRow.qml` keeps W4's KeyCap keys cell and W1's `captureButton` with its `captureActivityChanged` forwarding. The merged `InputShortcutsSection.qml` has W4's `QindaQtTheme {}` and both W1 forwards. The integrator should rerun `qindaqt.settings-command-palette` and the Input page tests on the combined tree, because W1's fixture rows will then draw `Tk.KeyCap` (QindaTK r5 at runtime), and should raise the QindaTK bound to r5. I did not build the combined tree.

## Verification

- `git worktree add --detach .cache/claude-plan-20260923/review-opus-review-w1-recheck fdeaa75f…`: HEAD `fdeaa75f89ae34e79ee1890e8636978fec4ff24e`.
- `cmake --preset dev`: exit 0.
- `cmake --build build/dev -- -j8 -l20`: exit 0. The final run built 6423 steps, after earlier targeted builds.
- `QT_FATAL_WARNINGS=1 ctest --test-dir build/dev --output-on-failure -R '^qindaqt\.settings-(route-search|command-palette)$'`: exit 0, 2/2 passed.
- `ctest --test-dir build/dev --output-on-failure -R '^qindaqt\.settings-input'`: exit 0, 12/12 passed.
- `ctest --test-dir build/dev --output-on-failure -L settings`: exit 0, 135/135 passed (237 s).
- `./tools/validate-docs`: exit 0. It validated 389 Markdown documents and the `mkdocs.yml` navigation.
- `mkdocs build --strict`: unavailable (`mkdocs` is not installed).
- `./tools/check-source-shape`: exit 1 (repo-wide). Changed paths it flags: `Main.qml` 414 > 350 (error, also present at the base) and the test file at 511 (warning).
- `git diff --check 2e415cad fdeaa75f`: exit 0.
- Reproductions: a temporary patch to `tests/apps/settings_center/tst_settings_command_palette.cpp` in this worktree, reverted after use and saved at `qinda:/tmp/opus-w1r-repro.patch`. Run offscreen with the ctest environment (`QT_FATAL_WARNINGS=1` except where noted):
  - `reviewRealKeyChords`: 8/8 passed.
  - `reviewOverrideOnly`: passed.
  - `reviewCaptureExitPaths`: escape, tab, focus-loss, window-deactivate, route-change, model-reset and recapture-other passed. destination-change failed with the flag stuck `true` (note 2; run without fatal warnings).
  - `reviewEagerInputComposition`: failed as expected, showing that `InputRouteComposition` is constructed before Input opens (note 1).
  - `reviewHeaders`: passed; the row dumps are quoted above.
