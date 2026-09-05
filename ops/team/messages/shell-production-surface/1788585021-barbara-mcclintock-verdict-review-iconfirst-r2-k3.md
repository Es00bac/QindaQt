# Exact-candidate review verdict — Shell iconography I2 icon-first applets, repair recheck (round 2)

- Reviewer persona: **Barbara McClintock**, independent shell reviewer (`barbara-mcclintock`)
- Provider/model: Moonshot Kimi `kimi-code/k3`
- Candidate SHA: `7eb5372d8cc1a381dcf9fb4f1463032b1f018487` (`worker/shell-icon-first-applets`, repair by May-Britt Moser, OpenAI Codex)
- Candidate tree SHA: `63dc9d5c004b2fca94ea7f55037ded202e7ea6a6`
- Parent SHA: `3796131ef4e01c7fd503dafca77adfaaf9b3492c` (handoff record on `0af5d685`)
- Repaired candidate: `0af5d685a36e8c5bb0e00fc708248e93b92ae739` (round-1 REJECT at 0/3/6/10)
- Base SHA: `a5d78e804f28b5e11df8216ef8e0995f5d9d5191`
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/shell-icon-first-applets-k3-review` (detached at candidate; `git status --porcelain` empty before and after)
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-iconfirst-k3` (`debug/`, `release/`, `scratch/`, `site/`); fresh scratch for this round under `scratch/` (`iconproof/` updated, `prev/` = extracted `0af5d685`, `standalone-*-r2`)

All scratch reproductions live under the build root; no product path was edited.

## Disposition of the 19 round-1 findings

Every finding was checked against the `0af5d685..7eb5372d` diff and re-executed. All three P1s and all six P2s are **fixed at the cause**, each with a negative control that fails on the repaired-from tree. Nine of ten P3s are fixed or substantively addressed; five micro-precision items remain (see P3 below).

### P1-1 (audio summary icon dead logic) — FIXED, behaviorally proven

`src/shell/audio_applet/qml/AudioApplet.qml:43-61` now reads `rows[i].isOutput === true` / `isDefault === true` and thresholds the model's real 0.0–1.0 `volume` (`<= 0` → muted, `< 1/3` → low, `< 2/3` → medium, else high). Proof, not just source:

- New data-driven QML test `summaryIconTracksDefaultOutput` (`tests/shell/audio_applet/tst_audio_applet_qml.cpp:210-282`) drives four fake-transport snapshots (muted/low/medium/high) through the compiled applet with a real `IconRuntime` fixture and asserts both `summaryIconName` and the resolved provider source. Direct run:
  `env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_QPA_PLATFORM=offscreen QT_FATAL_WARNINGS=1 ./debug/tests/shell/audio_applet/qindaqt_audio_applet_qml_tests summaryIconTracksDefaultOutput` → **6 passed, 0 failed**.
- New static contract probe `tests/shell/audio_applet/verify_audio_summary_icon_contract.cmake` (registered as `qindaqt.audio-applet-summary-icon-contract`, passed in both profiles). Negative control: against extracted `0af5d685` (`cmake -DQINDAQT_SOURCE_DIR=scratch/prev -P …`) → **exit 1**, "Audio summary icon contract is missing 'rows[i].isOutput === true'". Against the candidate → exit 0.
- Wiki claim (`docs/wiki/shell/audio-applet.md:92-94`, matrix row :179) now matches the implementation and the test.

### P1-2 (`qindaqt.shell-runtime-component-closure` hard-fail) — FIXED at the cause

`src/shell/ShellAppletRuntimeInstall.cmake:40-59` now declares one **literal** component per `install(TARGETS qindaqt-shell …)` rule (with an AGENT-GUARD explaining why a parameter hides a stage from the probe); payloads keep their parameterized function, which the probe does not scan for shell-target rules.

- Real ctest row: **Passed** in Debug (8.26 s) and Release; full selector 174/174 in both profiles (previously the sole failure at 172/173).
- Negative control: the round-1 scratch probe scan (`scratch/closure-probe-repro.cmake` logic) against extracted `0af5d685` → **exit 1** ("Every qindaqt-shell install rule must name its component explicitly"); against the candidate → exit 0, declared components `QindaQt;AudioAppletRuntime;…;TaskListAppletRuntime`.

### P1-3 (three icon names absent from Breeze) — FIXED, proven against host Breeze

The three missing names were replaced: `network-wireless-offline-symbolic` → `network-wireless-off` (`AppletChip.qml:95,100`), `application-x-addon` → `preferences-plugin` (status-notifier chip, :86) and `applications-other` (unmapped-plugin fallback, :95), `dialog-warning-symbolic` → `dialog-warning` (`TaskListApplet.qml:153`; phase icon also de-suffixed at :91).

- I independently re-extracted the declared literal name set from every `ShellIcons.Icon` user in `src/shell` (grep over 9 QML files): 34 names, exactly matching the repair's new pinned fixture `tests/shell/testdata/breeze-icon-names.txt`.
- Offscreen proof (updated `scratch/iconproof/main.cpp`, real `IconImageProvider` from the Debug build, roots `/usr/share/icons`, chains `breeze-dark` and `breeze`, every name requested with `size=20&scale=1&symbolic=1&color=#ff0000`, run as `env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_QPA_PLATFORM=offscreen QT_FATAL_WARNINGS=1 ./iconproof`): **exit 0, failures=0 — 68/68 name/theme requests resolved to painted, non-placeholder pixels with correct symbolic recolor** (including the non-symbolic-asset `dialog-warning`, which the provider recolors monochromatically when symbolic is requested). Round 1 this harness measured exit 1 with failures=6.
- Production wiring trace (review question 1): `ShellRuntimeApplication` order is unchanged and correct — `initializeTokens` (synchronous QST-1) → `initializeIcons` → `initializeLauncherRuntime` (`shellruntimeapplication.cpp:239-245`); roots are `XDG_DATA_HOME/icons` + `XDG_DATA_DIRS/icons` from the single `m_dataRoots` snapshot taken in `loadCatalogs` (`:167-170`), theme name is the catalog-retained `iconTheme` (`breeze-dark` for qinda-dark) with `hicolor` appended by the locator. The nested stage therefore still differs from the installed session only by missing assets, matching the handoff caveat.

### P2-1 (status-notifier feedback popup keyboard-dead) — FIXED

`StatusNotifierApplet.qml:121-162`: popup is now `popupType: T.Popup.Window`, `closePolicy: CloseOnEscape | CloseOnPressOutside | CloseOnPressOutsideParent`, with an AGENT-GUARD and `onOpened` seeding focus to the new `statusNotifierFeedbackDismiss` button. Behavioral tests assert `Popup.Window` identity, `dismiss.activeFocus`, Space activation clearing feedback, and Escape closing without clearing (`tst_StatusNotifierApplet.qml:270-292`); accessibility test updated. Both rows passed in the selector.

### P2-2 (clipboard symbolic icon never recolored) — FIXED

`ClipboardPanelApplet.qml:67` now sets `color: Tokens.fg.default` (with `import QindaQt.Tokens 1.0`); the production-panel keyboard test asserts `icon.color.a > 0` plus exact name and resolved provider source.

### P2-3 (new chip clip cut live content) — FIXED at the cause

`AppletChip.qml:52` is now `clip: !usesLiveContent` with an AGENT-GUARD naming `PanelContent` as the sole surface-extent clip authority (`PanelContent.qml:22 clip: true` unchanged). Chip extents derive from tokens (`compactExtent = space["6"] + space["2"] = 28`). New dispatcher row `test_thinnestStockPanelDoesNotClipLiveIcon` (`tst_launcher_dispatcher.qml:105-122`) pins the 18 px minimal-panel row: `chip.height == 18`, `!chip.clip`, icon fully contained (`icon.y >= 0`, `icon.y + icon.height <= launcher.height`, extent 14). Passed in the selector.

### P2-4 (documented standalone lanes broken) — FIXED

`src/shell/launcher/CMakeLists.txt:220-224` adds a `QINDAQT_LAUNCHER_PURE_ONLY` early return (parser only, no synthesized runtime graph); the four standalone blocks add `src/themes`, `src/design_tokens`, `src/controls`, the pure launcher, and `src/shell/icons`. Re-executed all four documented configures beneath the scratch root:
`cmake -S tests/shell/{launcher,task_list,bluetooth_applet,power_applet} -B scratch/standalone-*-r2 -G Ninja -C <deps-cache> -DCMAKE_BUILD_TYPE=Debug` → **exit 0 each** (round 1: configure failed at `src/shell/launcher/CMakeLists.txt:116`). `docs/wiki/development/testing-harness.md:2530-2539` documents them as configure-only gates.

### P2-5 (closure probe/wiki missed the Icons module) — FIXED

`run_shell_component_closure.cmake:104-120` now requires the Icons `qmldir`, `qindaqt_shell_icons.qmltypes`, and `qml/Icon.qml` per component stage; `docs/wiki/shell/applet-runtime.md:179-184` names the Icons module and the literal-component inventory. Row passed in both profiles.

### P2-6 (ShellIconConfiguration re-parsed theme JSON) — FIXED at the cause

`ShellIconConfiguration::selectedThemeName` (`shelliconconfiguration.cpp:59-80`) no longer opens any file: `ThemeLoader` validates and retains `iconTheme` in `ThemeSpec` (`theme_loader.cpp:54-71,99-105`; `theme_spec.h:31-34`; surfaced through `ThemeSpec::toVariantMap`), and the shell consumes the catalog value with an AGENT-CONTRACT naming `ThemeLoader` the sole parser authority. The 256 KiB skip, second grammar, and TOCTOU window are gone. Tests: `hostileHintFailsCatalogLoad`, `largeCatalogThemeIsNotReparsed` (300 KiB theme loads through the catalog and resolves), `leadingPunctuationIsSchemaValid` — all in `qindaqt.shell-icon-runtime-configuration`, passed in both profiles.

### P3 dispositions

- P3-1 tautological disjunction — **fixed**: all six sites now assert the exact name plus a resolved provider source via `tests/shell/icon_resolution_test_fixture.h` (a real `IconRuntime::install` over a generated fixture theme); the qmltestrunner stub `Icon.qml` now models production polarity (`resolved = name.length > 0`, placeholder `visible: !resolved`).
- P3-2 coverage rows passing on base — **fixed**: the probe now requires every literal built-in summary name both declared and present in the pinned Breeze/Breeze-dark intersection fixture. Negative controls re-run: vs `0af5d685` → exit 1 at `"applications-other"`; vs base `a5d78e80` → exit 1 at `launcherAppletIcon`; vs candidate → exit 0. Residual (carried, below): the per-theme `iconTheme` row is still presence-only, mitigated by the new loader-validation tests and the offscreen proof of both used values.
- P3-3 `"white"` fallbacks — **fixed**: `BluetoothApplet.qml:68`, `PowerApplet.qml:70,80` use `Tokens.fg.default`.
- P3-4 grammar stricter than schema — **fixed**: the loader's `isValidIconThemeName` allows leading `.`/`_`/`-`, length-bounds at 128, rejects `..`; punctuation test passes.
- P3-5 empty/relative `XDG_DATA_HOME` dropped — **fixed**: falls back to `$HOME/.local/share` per the XDG spec (`shelliconconfiguration.cpp:48-52`), pinned by `invalidDataHomeUsesSpecificationDefault`.
- P3-6 two environment snapshots — **fixed**: one `m_dataRoots` snapshot in `loadCatalogs` feeds icons, launcher roots, and task-list application roots; the wiki sentence is now true.
- P3-7 preview icon-init empty error — **fixed**: `shellpreviewapplication.cpp:118-127` sets `*error`.
- P3-8 keyboard consistency — **fixed**: audio summary button has `Keys.onReturnPressed/onEnterPressed` (`AudioApplet.qml:83-90`, test now uses Return); clipboard `Accessible.checkable: false` matches `checkable: false`.
- P3-9 wiki overclaim — **fixed**: `panel-surfaces.md:112-117` scopes the `Popup.Window` claim to audio/clipboard/status-notifier and states other detail popups keep item-popup behavior with no layer-shell keyboard claim.
- P3-10 — **partially carried** (see P3 ledger below): the power-applet width test is now exact (`QCOMPARE(root->width(), 62.0)`, height 28, summary == root width).

## Findings ledger for candidate `7eb5372d`

### P0 — none

### P1 — none

### P2 — none

### P3 (carried-over precision items; none blocking)

- **P3-a.** `src/shell/power_applet/qml/PowerApplet.qml` now trips the decomposition-review threshold at 327 non-blank lines (gate exit 0, below the 500-line production ceiling). Unchanged from round 1.
- **P3-b.** `AppletChip.qml` vertical width branch remains dead in panel usage (`PanelAppletColumn.qml:35` always assigns width), and a zero-size `emptyLiveContent` chip still contributes 4 px of Row spacing. Cosmetic, contained.
- **P3-c.** Global-menu vertical entry labels still cannot elide (`GlobalMenuApplet.qml:278-289`, hard-clipped mid-glyph, contained). Unchanged.
- **P3-d.** Installed-consumer link lines still omit the icons archive's PUBLIC launcher dependency (`tests/shell/task_list/installed_task_list_consumer/CMakeLists.txt`, `tests/shell/status_notifier/applet/installed_consumer/CMakeLists.txt`); links today because no unresolved symbol is pulled. Unchanged, bounded.
- **P3-e.** The per-theme `iconTheme` row in `verify_shell_icon_coverage.cmake` is presence-only; the values themselves are validated by the loader tests and both used values (`breeze`, `breeze-dark`) are pixel-proven by the offscreen harness.

No new defects were introduced by the repair that I could find: I read the full 62-file diff, re-ran every gate, and attacked the new test fixtures for vacuity (the fixture installs the real provider and requires an exact `image://qindaqt-icon/<name>` source; the stub mirrors production polarity).

## Verified good (attacked and held)

- Real icons, not placeholders: 34/34 declared names × 2 theme chains resolve with recolor against the host's Breeze (review question 1), and the production composition passes exactly these roots/theme in the installed session.
- Panel geometry/behavior (question 2): audio 32×28 pinned; thin-panel live content proven by the new dispatcher row; labels hidden with accessible names; popups open and are keyboard-reachable (audio Return, clipboard/audio `Popup.Window`, status-notifier feedback focus-seeded `Popup.Window` with Escape/outside close); task-list buttons render resolver icon + title, degraded badge is a real Breeze glyph with accessible name/description; tray shows item icons only; global-menu empty state stays 0×0.
- Negative controls (question 3): coverage probe, audio contract probe, and closure-probe scan each fail on `0af5d685` (and base where applicable) and pass on the candidate; no hard-coded colors in new icon code (token colors everywhere; clipboard recolor fixed).
- `qindaqt.theme-formats` passes in both profiles; all five built-in themes carry valid `iconTheme`; `python3 -m json.tool` clean on the seven changed/moved JSON files.
- Nothing touches host buses, display, hardware, uinput, or network; all rows ran with host display/bus variables unset and an unreachable system bus address.

## Commands run and results

Identity: `git rev-parse HEAD` = `7eb5372d8cc1a381dcf9fb4f1463032b1f018487`; `git status --porcelain` empty before and after. Repair diff scope: `git diff 0af5d685..HEAD` = 62 files, +1066/−277 (plus the `3796131e` handoff-record commit between them).

Builds (lane recipe cache `qindaqt-system-kwin-initial-cache.cmake`, Ninja, strict warnings ON, uinput OFF):

- Full `cmake --build …/debug --parallel 3`: **exit 0**. Full `cmake --build …/release --parallel 3`: **exit 0**.

Tests (`env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`; no nested/windowed/host-service rows):

- Debug, brief selector `^qindaqt\.(shell-|applet|task-list-|global-menu-|status-notifier-|clipboard-applet-|launcher|notification-center|audio-applet|bluetooth-applet|power-applet)`: **exit 0, 174/174 passed**, including `qindaqt.shell-runtime-component-closure`, `qindaqt.shell-icon-coverage`, `qindaqt.shell-icon-runtime-configuration`, `qindaqt.audio-applet-summary-icon-contract`, `qindaqt.audio-applet-offscreen`. Log: `scratch/debug-selector-r2.log`.
- Release, identical selector: **exit 0, 174/174 passed**. Log: `scratch/release-selector-r2.log`.
- Debug/Release `^desktop\.virtual\.(sandbox-unit|package-contract|stage-closure)$`: exit 0, 3/3 each.
- Debug/Release `^qindaqt\.theme-formats$`: exit 0, 1/1 each.
- Direct focused run: `qindaqt_audio_applet_qml_tests summaryIconTracksDefaultOutput` (offscreen, `QT_FATAL_WARNINGS=1`): 6 passed, 0 failed.

Static gates (worktree root):

- `./tools/validate-docs`: exit 0 (146 documents/navigation entries).
- `mkdocs build --strict --site-dir …/review-iconfirst-k3/site` (docs venv): exit 0.
- `./tools/check-source-shape`: exit 0 (warnings only; PowerApplet.qml 327 lines — P3-a).
- `git diff --check`: exit 0.
- `python3 -m json.tool` on five built-in themes + `icon-themes-invalid/bad.json` + `icon-themes-valid/default-light.json`: exit 0, 7/7.

Negative controls (scratch, extracted trees — no in-repo edits):

- `verify_audio_summary_icon_contract.cmake` vs `scratch/prev` (`0af5d685`): exit 1 (missing `rows[i].isOutput === true`); vs candidate: exit 0.
- `verify_shell_icon_coverage.cmake` (candidate fixture injected) vs `scratch/prev`: exit 1 (`"applications-other"` undeclared); vs `scratch/base` (`a5d78e80`): exit 1 (`launcherAppletIcon` undeclared); vs candidate: exit 0.
- Closure-probe scan (`scratch/closure-probe-prev.cmake`) vs `0af5d685` install modules: exit 1 at the parameterized `COMPONENT "${component}"` rule; vs candidate: exit 0 with nine literal components.

Offscreen Breeze proof: `scratch/iconproof/iconproof` (rebuilt against the Debug `libqindaqt_shell_icons.a`): **exit 0, failures=0, 68/68 resolved + recolored** in `breeze-dark` and `breeze`.

Standalone lanes: four documented Debug configures (`launcher`, `task_list`, `bluetooth_applet`, `power_applet`) under `scratch/standalone-*-r2`: exit 0 each.

## Verdict

**ACCEPT.** P0=0, P1=0, P2=0, P3=5 (all carried-over nonblocking precision items from round 1; none introduced or worsened by the repair).

All three round-1 P1s are fixed at the cause with negative controls that fail on the repaired-from commit: the audio summary icon now tracks the default output's mute/volume through a behaviorally asserted data-driven test; the component-closure gate passes in both profiles with literal component declarations; and every declared icon name resolves to real recolored pixels in both Breeze chains, measured with the real provider against the host's icon roots. The repair also resolved all six P2s (feedback popup now a focus-seeded `Popup.Window`, clipboard recolor, chip double-clip removed with `PanelContent` as sole clip authority, standalone lanes restored, Icons module in the closure probe and wiki, single-parse theme authority in `ThemeLoader`) and tightened the previously tautological icon assertions into real provider-source proofs. The candidate satisfies the user-stated acceptance standard as far as offscreen evidence can show it: icons, not labels; working, keyboard-reachable popups; no clipped text on stock panels.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/5
