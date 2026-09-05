# Exact-candidate review verdict — Shell iconography I2 icon-first applets

- Reviewer persona: **Barbara McClintock**, independent shell reviewer (`barbara-mcclintock`)
- Provider/model: Moonshot Kimi `kimi-code/k3`
- Candidate SHA: `0af5d685a36e8c5bb0e00fc708248e93b92ae739` (`worker/shell-icon-first-applets`, implementer May-Britt Moser, OpenAI Codex)
- Candidate tree SHA: `16dd4642d9f5a86b2b12f1f1825cac97c0703b56`
- Parent SHA: `a5d78e804f28b5e11df8216ef8e0995f5d9d5191`
- Base SHA: `a5d78e804f28b5e11df8216ef8e0995f5d9d5191` (parent == base; single-commit lane on the I1 merge)
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/shell-icon-first-applets-k3-review` (detached at candidate; `git status --porcelain` empty before and after)
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-iconfirst-k3` (`debug/`, `release/`, `scratch/`, `site/`)

All scratch reproductions live under the build root (`scratch/`); no product path was edited.

## Findings ledger

### P0 — none

### P1

**P1-1. Audio summary icon logic is dead: the panel icon is permanently `audio-volume-muted`.**
`src/shell/audio_applet/qml/AudioApplet.qml:38-62`. `defaultOutputRow()` matches
`rows[i].direction === "output"`, but the gadget projected to QML exposes `isOutput`
(bool), `volume` (0.0–1.0), `muted`, `isDefault` (`src/shell/audio_applet/audio_applet_model.h:34-38`;
projected via `QVariant::fromValue(row)` in `audio_applet_controller.cpp:114-121`). No row ever
matches, so `outputRow` is always `null` and `summaryIconName` always returns
`"audio-volume-muted"` regardless of the default output's real mute/volume state. Even if the
property name were right, line 57 reads nonexistent `normalizedVolume` and lines 58-60 threshold
a 0–100 scale against the model's 0.0–1.0 `volume`. The candidate's own wiki addition claims the
opposite behavior: `docs/wiki/shell/audio-applet.md:92-94` ("Its symbolic name follows the default
output's mute and normalized-volume state (muted, low, medium, or high)"). No test pins the icon
name: `tests/shell/audio_applet/tst_audio_applet_qml.cpp:141-147` only asserts the Icon element
exists and `resolved || placeholder.visible`, which is structurally always true (see P3-1), so the
dead logic escaped.
Expected: summary icon name follows default output mute/volume as documented.
Observed: name is constant `audio-volume-muted` for every audio state.

**P1-2. `qindaqt.shell-runtime-component-closure` — the gate the wiki names for the
install-component contract — hard-fails on the exact candidate.**
The probe (`tests/shell/audio_applet/run_shell_component_closure.cmake:54-64`) regex-scans every
`src/shell/*RuntimeInstall.cmake` for `install(TARGETS qindaqt-shell …)` rules and requires a
literal component name (`COMPONENT [A-Za-z]…`). The candidate's decomposed
`src/shell/ShellAppletRuntimeInstall.cmake:4-8` uses `COMPONENT "${component}"`, which fails the
match and trips `FATAL_ERROR "Every qindaqt-shell install rule must name its component explicitly"`.
Reproduction (exact probe logic against the worktree files, scratch copy
`scratch/closure-probe-repro.cmake`):

    cmake -P scratch/closure-probe-repro.cmake
    -- matched install rules: … COMPONENT "${component}" …
    CMake Error: Every qindaqt-shell install rule must name its component explicitly
    exit=1

Confirmed end-to-end by the real ctest row (see Commands: it fails in both Debug and Release).
The two pre-existing sibling modules (`StatusNotifierRuntimeInstall.cmake`,
`TaskListRuntimeInstall.cmake`) name literal components, so the convention existed and the
candidate broke it without updating the probe. Note the implementer's handoff selector
(`qindaqt\.(launcher|…|shell-icon|…)`) never matched `qindaqt.shell-runtime-component-closure`,
so their 130/130 evidence silently excluded this gate.

**P1-3. Three declared icon names do not exist in Breeze and render letter placeholders in the
user's session, including on the default `qindaqt` profile.**
Offscreen proof (scratch harness `scratch/iconproof/`, real `IconImageProvider` from the Debug
build, roots `/usr/share/icons`, theme chains `breeze-dark` and `breeze`,
`QT_FATAL_WARNINGS=1`, host display/bus variables unset): every declared name was requested with
`size=20&symbolic=1&color=#ff0000`; resolved images were pixel-compared against
`IconImageProvider::placeholder(20)` and recolor-verified. Result: exit 1, `failures=6` — three
names return the deterministic placeholder in BOTH themes:

- `network-wireless-offline-symbolic` — `src/shell/qml/AppletChip.qml:94,97`. This is the
  production-path icon for the `system-status` chip whenever the unresolved runtime entry is not
  ready (`networkIconName()` returns it for `liveApplets && runtime.ready !== true` and for
  `state === "unavailable"`). `system-status` ships in five stock profiles including the default
  `data/profiles/qindaqt.json`. No file named `network-wireless-offline-symbolic.*` exists in
  `/usr/share/icons/breeze`, `breeze-dark`, or `hicolor` (verified with `find`).
- `application-x-addon` — `src/shell/qml/AppletChip.qml:80` (status-notifier static chip) and
  `:89` (default for every unmapped plugin id: `workspace-switcher` in the default profile, plus
  `places-menu`, `overview-trigger`, `command-hud`, `application-tiles`, `workspace-tiles` in other
  stock profiles). Absent from breeze/breeze-dark/hicolor (exists only in Adwaita).
- `dialog-warning-symbolic` — `src/shell/task_list/applet/qml/TaskListApplet.qml:153`, the
  task-list degraded badge. Breeze ships only plain `dialog-warning.svg`; with `symbolic: true`
  the locator tries `dialog-warning-symbolic-symbolic` then `dialog-warning-symbolic`, both absent
  → placeholder "W" tile instead of a warning glyph.

Per this lane's review standard, a declared applet icon name missing from Breeze is P1: the
milestone's whole point is "icons, not labels", and these chips render first-letter tiles on the
shipped profiles in the installed session.

### P2

**P2-1. New status-notifier feedback popup is an item popup; Escape and Dismiss are keyboard-dead
on the production panel, contradicting the wiki sentence added in the same commit.**
`src/shell/status_notifier/applet/qml/StatusNotifierApplet.qml:121-142` adds a `T.Popup` with no
`popupType` (item popup inside the layer-shell panel window) and
`closePolicy: T.Popup.CloseOnEscape` only. `RuntimePanel.qml:23` sets
`Qt.WindowDoesNotAcceptFocus`, and the repo's own AGENT-GUARDs
(`StatusNotifierItemDelegate.qml:176-184`, `ClipboardPanelApplet.qml:87-89`) state item popups on
that panel cannot take focus — so `CloseOnEscape` never fires from the keyboard and the Dismiss
button is unreachable by Tab. There is also no `CloseOnPressOutside`, so the only dismissal is a
pointer click on Dismiss. The candidate's own wiki edit (`docs/wiki/shell/panel-surfaces.md:113`)
claims "operational notices live in focusable `Popup.Window` surfaces". Audio and clipboard were
converted to `Popup.Window` in this same commit; this new popup should follow that pattern.
(Base showed feedback as an inline `C.StateCard`; the popup is new here.)

**P2-2. Clipboard summary icon is symbolic but never recolored — dark glyph on dark chip.**
`src/shell/clipboard_applet/qml/ClipboardPanelApplet.qml:62-70` sets `symbolic: true` with no
`color`. Per the Icon contract (`docs/wiki/shell/iconography.md`, `src/shell/icons/qml/Icon.qml`),
recolor applies only when `color` is set. Breeze `edit-paste-symbolic.svg` is authored
`fill:currentColor` with `ColorScheme-Text: #232629` (verified on host), so on `qinda-dark`
(chip background `#2c312e`-family) the icon renders near-black on near-black. Every sibling icon
added in this commit passes a token color (`Tokens.fg.default` etc.). The icon is present and the
accessible name is intact, so this is bounded — but effectively invisible in dark themes.

**P2-3. The new `clip: true` on AppletChip newly clips live content on thin stock panels.**
`src/shell/qml/AppletChip.qml:46`. Chip height is the row height (`PanelAppletRow.qml:35`),
i.e. panel thickness − 8 (`PanelContent.qml:37`): 22 px on the default `qindaqt` top panel
(thickness 30), 18 px on `minimal`. Status-notifier delegates are
`implicitHeight = iconSize + 2×Tokens.space["2"]` = 22 + 8 = 30 px
(`StatusNotifierItemDelegate.qml:45-47`; `space["2"]` = 4.0 in
`src/design_tokens/include/qindaqt/design_tokens/design_tokens.h:66-67`), so the new chip clip
cuts the bottom ~4 px of every tray icon on the default panel; at base only the panel-level clip
applied and the 22 px icon exactly fit the 30 px panel. Same class: task-list buttons keep a fixed
`implicitHeight: 28` (`TaskListEntryButton.qml:31`) inside 22 px rows (~6 px clipped, was ~2 px at
base). Bounded workaround (thicker panel), but it is a visual regression on the default profile
introduced by this commit.

**P2-4. Documented standalone focused-lane configures are broken.**
`tests/shell/launcher/CMakeLists.txt:3-5` documents configuring that directory on its own; the
candidate made `src/shell/launcher/CMakeLists.txt` require `qindaqt_shell_icons`
(`DEPENDENCIES TARGET`, link `QindaQt::ShellIcons`, `qindaqt_install_shell_icons_runtime()` which
FATAL_ERRORs without the targets) but the standalone block never adds `src/shell/icons`.
Reproduction:

    cmake -S tests/shell/launcher -B <scratch>/standalone-launcher -G Ninja -DCMAKE_BUILD_TYPE=Debug
    CMake Error … Argument qindaqt_shell_icons is not a target!
    (src/shell/launcher/CMakeLists.txt:116 via :221) — Configuring incomplete

The same structural break exists in the standalone blocks of `tests/shell/task_list`,
`tests/shell/bluetooth_applet`, and `tests/shell/power_applet`. Repository-root configure/build is
unaffected (verified), so this is bounded, but the documented lane workflow regresses.

**P2-5. The closure probe and wiki do not cover the now-mandatory Icons module in staged
component closure.** `AppletChip.qml:3` imports `QindaQt.Shell.Icons` unconditionally, so every
shell-carrying component's runtime closure now includes the Icons QML module. The staging loop
(`src/shell/CMakeLists.txt:321-330`) does install it, but the probe's required-path list
(`run_shell_component_closure.cmake:98-115`) still checks only Clipboard/TaskList/StatusNotifier
modules, and `docs/wiki/shell/applet-runtime.md:179-183` still names only those three. A missing
Icons staging would pass the gate (once P1-2 is repaired).

**P2-6. `ShellIconConfiguration` re-parses theme JSON from disk with rules that diverge from the
catalog loader.** `src/shell/common/shelliconconfiguration.cpp:89-123` re-opens and re-parses
`*.json` under the theme directory although its own header (:20-22) says it consumes the selected,
validated catalog. A theme valid to `ThemeLoader` but over the 256 KiB skip (:95;
`src/themes/src/theme_loader.cpp:62` reads uncapped) fails shell startup with the misleading
"selected theme %1 is missing from %2" (:124-127), and the re-read opens a TOCTOU window between
catalog load and this parse. Fail-closed, but a second parser at a boundary the design assigns to
`ThemeCatalog`.

### P3

- **P3-1. Tautological icon assertion disjunction.** The new assertions
  `icon.resolved || (placeholder !== null && placeholder.visible)`
  (`tst_audio_applet_qml.cpp:147`, `tst_bluetooth_applet_qml.cpp:129`,
  `tst_power_applet_qml.cpp:175`, `tst_launcher_qml.cpp:215`,
  `tst_task_list_applet_qml.cpp:225-226`, `tst_ClipboardProductionPanelKeyboard.qml:71-73`) can
  never fail: in production `Icon.qml` the placeholder's visibility is `!resolved`, and in the
  qmltestrunner stub `resolved` is hardcoded false with the placeholder always visible. The real
  signal is carried by the existence/size assertions. This is the missing negative control that
  let P1-1 through: no test asserts the audio summary icon *name* against mute/volume state.
- **P3-2. Coverage guard rows that already pass on base.** Five of twelve
  `verify_shell_icon_coverage.cmake` declarations (`taskListEntryIcon`, `statusNotifierItemIcon`,
  `notificationCenterAppletGlyph`, `clockApplet`, `globalMenuTopLevelItem`) are pre-existing
  strings, so those rows cannot detect a revert; the guard as a whole stays non-vacuous (verified:
  exit 1 on base, see Commands). The `iconTheme` theme check is presence-only; four of five theme
  values are never validated by any test (all five are currently valid).
- **P3-3. New icon code hard-codes `"white"` fallbacks** in BluetoothApplet.qml:67 and
  PowerApplet.qml:69,79 (`root.colors.text ?? "white"`) where siblings use `Tokens.fg.*`;
  fires only if the theme facade lacks `colors.text`. Pre-existing hex fallbacks in AppletChip
  chrome are unchanged from base (5 at base, one removed here).
- **P3-4. `isValidIconThemeName` is stricter than the schema** it validates against:
  `shelliconconfiguration.cpp:38-39` requires an alphanumeric first character while
  `docs/wiki/reference/theme-schema-v1.md:19` allows leading `.`/`_`/`-`. Fail-closed direction;
  schema-valid hints would abort startup.
- **P3-5. `dataRoots` drops a set-but-empty or relative `XDG_DATA_HOME` entirely**
  (`shelliconconfiguration.cpp:62-67`) instead of falling back to `$HOME/.local/share` per the XDG
  spec's invalid-value handling. Fail-closed; fewer roots.
- **P3-6. Two environment snapshots** (`shellruntimeapplication_tokens.cpp:43-44` and
  `shellruntimeapplication_applets.cpp:69-70`) where the wiki says "one explicit freedesktop
  data-root snapshot" (`iconography.md:122-124`). Values identical in practice; doc/impl mismatch.
- **P3-7. Preview icon-init failure logs an empty reason**:
  `ShellPreviewApplication::initializeIcons` (`shellpreviewapplication.cpp:118-123`) returns
  `install()`'s false without setting `*error`, unlike the runtime variant
  (`shellruntimeapplication_tokens.cpp:47-51`).
- **P3-8. Keyboard consistency gaps in new/changed controls**: the new audio summary button
  (`AudioApplet.qml:64-95`) lacks the `Keys.onReturnPressed/onEnterPressed` handlers the clipboard
  and launcher buttons gained in this same commit (QQC2 ToolButton activates on Space only — see
  the repo's own note at `TaskListEntryButton.qml:46-47`); the popup remains Space-reachable.
  Clipboard summary sets `Accessible.checkable: true` while `checkable: false`
  (`ClipboardPanelApplet.qml:31,39-40`).
- **P3-9. Wiki overclaim for detail popups**: `panel-surfaces.md:113` says "wide detail controls
  … live in focusable `Popup.Window` surfaces", but the bluetooth, power, and launcher detail
  popups remain item popups (pre-existing code; the sentence is new).
- **P3-10. Misc precision**: `AppletChip.qml:39` `vertical ? 32` width branch is dead in panel
  usage (`PanelAppletColumn.qml:35` always assigns width); a zero-size `emptyLiveContent` chip
  still contributes 4 px Row spacing; global-menu vertical entry labels cannot elide
  (`GlobalMenuApplet.qml:278-289`, hard-clipped mid-glyph, contained); power-applet width test
  allows ~76 px slack vs the audio test's exact 32×28; `PowerApplet.qml` now trips the
  decomposition-review warning at 326 non-blank lines (source-shape WARNING, gate still exit 0);
  installed-consumer link lines omit the icons archive's PUBLIC launcher dependency
  (`tests/shell/task_list/installed_task_list_consumer/CMakeLists.txt:74-80`,
  `tests/shell/status_notifier/applet/installed_consumer/CMakeLists.txt:71-74`) — links today only
  because no unresolved symbol is pulled.

## Verified good (attacked and held)

- Install ordering: `initializeTokens` (QST-1 publication, synchronous) → `initializeIcons` →
  launcher runtime → panel QML (`shellruntimeapplication.cpp:237-245`,
  `shellruntimeapplication_tokens.cpp:16-54`); preview publishes tokens then installs then loads
  the window. A second `IconRuntime::install` is refused and treated as fatal; invalid
  `iconTheme` hints abort startup (exit 4 path) in both apps.
- Roots: `freedesktopIconRoots`/`freedesktopApplicationRoots` append `/icons` resp.
  `/applications` to every data root (`icon_runtime.cpp:32-62`); production passes
  `XDG_DATA_HOME` + `XDG_DATA_DIRS` from an explicit `QProcessEnvironment` snapshot and the
  selected theme's `iconTheme` (`breeze-dark` for qinda-dark). The nested stage therefore differs
  from the installed session only by missing icon-theme assets, matching the handoff caveat.
- All five shipped themes carry valid `iconTheme` values (`breeze-dark` ×3, `breeze` ×2);
  `python3 -m json.tool` passes on all changed JSON.
- 30 of 33 declared icon names resolve to real recolored pixels in both Breeze chains (proof
  above); symbolic recolor preserves alpha and replaces RGB with the token color.
- Panel geometry: the base Audio overflow (implicitWidth 340) is fixed at its cause
  (`AudioApplet.qml:16-17`, 32×28, pinned by test); cross-axis containment verified
  structurally for both orientations; labels hidden with accessible names preserved on every
  converted button; task-list buttons render resolver icon + elided title with keyboard
  navigation; degraded badge carries `Accessible.name`/description; tray strip shows item icons
  only; global-menu empty state collapses to a 0×0 chip.
- Negative controls: `verify_shell_icon_coverage.cmake` exits 1 on the base tree (first failure:
  `launcherAppletIcon`) and 0 on the candidate; base themes carry no `iconTheme` key;
  unapproved matrix scenario ids fail closed (`load_matrix_scenario` +
  `test_unapproved_or_nonhorizontal_rows_fail_closed`); capture rows verify dimensions,
  uniformity, digests, and per-output content regions with tampering controls.
- The CMake decomposition of `src/shell/CMakeLists.txt` is content-faithful apart from P1-2/P2-5;
  no shared-registry edit was non-additive; nothing touches host buses, hardware, network, or
  uinput.

## Commands run and results

Worktree identity: `git rev-parse HEAD` = `0af5d685a36e8c5bb0e00fc708248e93b92ae739`;
`git status --porcelain` empty before and after all work. Diff scope: `git diff
a5d78e80..HEAD` = 96 files, +1376/−528.

Builds (exact lane recipe, cache
`/home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake`,
Ninja, strict warnings ON, uinput tests OFF):

- Debug configure: exit 0. Release configure: exit 0.
- `cmake --build …/debug --parallel 3 --target qindaqt-shell qindaqt-shell-preview`: exit 0 (934/934).
- Full `cmake --build …/debug --parallel 3`: exit 0 (3287 steps).
- Full `cmake --build …/release --parallel 3`: exit 0 (4221 steps).

Test rows (all under `env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY
DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`; no nested, windowed, or host-service rows):

- Debug, brief selector `^qindaqt\.(shell-|applet|task-list-|global-menu-|status-notifier-|clipboard-applet-|launcher|notification-center|audio-applet|bluetooth-applet|power-applet)`:
  **exit 8 — 172/173 passed; sole failure `qindaqt.shell-runtime-component-closure`**
  (`run_shell_component_closure.cmake:61`: "Every qindaqt-shell install rule must name its
  component explicitly") — P1-2. Log: `scratch/debug-selector.log`.
- Release, identical selector: **exit 8 — 172/173 passed; same sole failure**. Log:
  `scratch/release-selector.log`.
- Debug `desktop\.virtual\.(sandbox-unit|package-contract|stage-closure)`: exit 0, 3/3.
  Release: exit 0, 3/3.

Static gates (worktree root):

- `./tools/validate-docs`: exit 0 (146 documents/navigation entries).
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir
  /home/cabewse/work_SPaC3/builds/qindaqt/review-iconfirst-k3/site`: exit 0.
- `./tools/check-source-shape`: exit 0 (2,573 files; warnings only, incl. new
  `PowerApplet.qml` at 326 non-blank lines — P3-10).
- `git diff --check`: exit 0.
- `python3 -m json.tool` on the five changed themes plus `tests/shell/testdata/icon-themes/
  {bad,default-light}.json`: exit 0 for all.

Negative controls:

- Base tree extracted via `git archive a5d78e80` into `scratch/base/` (read-only for the repo).
  `cmake -DQINDAQT_SOURCE_DIR=scratch/base -P tests/shell/verify_shell_icon_coverage.cmake`:
  **exit 1** ("LauncherApplet.qml lacks required icon-first declaration: launcherAppletIcon");
  on the candidate: exit 0. Base `data/themes/*.json` contain no `iconTheme` key.
- Unapproved matrix rows fail closed structurally (`desktop_session_matrix.py:76`,
  `EXECUTABLE_MATRIX_ROWS`; pinned by
  `test_unapproved_or_nonhorizontal_rows_fail_closed`).

Offscreen Breeze proof (review question 1): scratch harness `scratch/iconproof/main.cpp`
compiled against the Debug `libqindaqt_shell_icons.a`, driving the real `IconImageProvider`
with roots `/usr/share/icons` and theme chains `breeze-dark` and `breeze`, run as
`env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY
DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_QPA_PLATFORM=offscreen QT_FATAL_WARNINGS=1
./iconproof`: **exit 1, failures=6** — `network-wireless-offline-symbolic`,
`application-x-addon`, and `dialog-warning-symbolic` return the deterministic placeholder in
both themes; the other 30 declared names resolve with painted pixels and correct symbolic
recolor in both themes (P1-3). Production wiring trace (`ShellRuntimeApplication::initializeIcons`,
`shellruntimeapplication_tokens.cpp:36-54`): roots = `XDG_DATA_HOME/icons` +
`XDG_DATA_DIRS/icons` from an explicit environment snapshot, theme name = selected theme's
`iconTheme` (`breeze-dark` for qinda-dark) with hicolor appended by the locator; install occurs
after synchronous QST-1 publication and before panel QML — so the installed session resolves
exactly the names proven above, and the nested stage differs only by missing assets.

Closure-probe reproduction (P1-2): `cmake -P scratch/closure-probe-repro.cmake` (exact probe
logic over the worktree files): exit 1 at the `COMPONENT "${component}"` rule.

Standalone-lane reproduction (P2-4): `cmake -S tests/shell/launcher -B
scratch/standalone-launcher -G Ninja -DCMAKE_BUILD_TYPE=Debug`: configure fails —
"Argument qindaqt_shell_icons is not a target!" (`src/shell/launcher/CMakeLists.txt:116`).

## Verdict

**REJECT.** P0=0, P1=3, P2=6, P3=10.

The candidate is well-built and its icon runtime wiring, containment model, accessible-name
preservation, and negative controls are genuine, but three blocking defects stand:

1. P1-1 — the audio summary icon never reflects mute/volume (dead `direction`/`normalizedVolume`
   logic), contradicting the wiki contract the same commit adds, with no test that could catch it.
2. P1-2 — `qindaqt.shell-runtime-component-closure`, the named gate for the install-component
   contract, fails on the exact candidate in both profiles; the implementer's handoff selector
   never matched it.
3. P1-3 — three declared icon names are absent from Breeze and render letter placeholders on
   shipped profiles (including the default `qindaqt` profile's `system-status` and
   `workspace-switcher` chips and the task-list degraded badge), measured with the real provider
   against the host's Breeze roots in both theme chains.

Suggested repair scope for the implementer (same worktree, then re-review the repaired commit):
fix `AudioApplet.qml` to read `isOutput`/`isDefault`/`volume` (0.0–1.0 thresholds) and pin the
icon name in `tst_audio_applet_qml.cpp`; make `ShellAppletRuntimeInstall.cmake` probe-visible
(literal component names or a probe update that understands the function) and extend the probe +
wiki to the Icons module closure; replace the three missing icon names with names present in
Breeze (e.g. `network-wireless-offline` / `dialog-warning` with the locator's symbolic pass, a
mapped `workspace-switcher` name such as `virtual-desktops`, and a real fallback for
`application-x-addon`); convert the status-notifier feedback popup to `Popup.Window`; give the
clipboard icon a token color; re-check chip-clip geometry on 26–30 px panels; restore the
standalone test lanes.

VERDICT REJECT P0/P1/P2/P3=0/3/6/10
