# Lane report: wallpaper — make the wallpaper chooser work

Branch: `worker/wallpaper-20260921`
Handoff commit: **the branch tip** (this report's commit). The work lands in
`188431aa` on top of the resumed WIP `6b3c9e7a`, base `402ccb3b`; integrate the
tip so the report travels with the change.

## What was broken (reproduced, then fixed)

The lane resumed from the WIP commit per RESUME.md: inspected `6b3c9e7a`,
configured and built before changing anything, and found the WIP's own tests
were **never registered** (they sat inside `if(TARGET qindaqt-shell)`, false at
this lane's configure) and its new controller test **failed on first run**.

Old-code reproduction (base `AppearanceWallpaperSection.qml` rebuilt into the
qml module, running the new row
`qindaqt.appearance-page::wallpaperFieldFollowsExternalDraftChangesAndPreviewUsesFileUrls`):

- **FAIL**, exit 1, at `tst_appearance_page.cpp:544`: typing a `qindaqt:`
  identity into the wallpaper path field leaves the draft as `"x"` instead of
  `"qindaqt:x"`. The declarative `text:` binding's display transform
  (`startsWith("qindaqt:") ? "" : …`) re-evaluated on every keystroke's draft
  round-trip and wiped the field mid-typing. (The manager's lead said the
  binding dies after the first keystroke; on Qt 6.11.1 it survives and follows
  external changes — the real break is the inverse: the live binding clobbers
  the user's own typing. Same fix either way.)
- `QWARN … Cannot open:
  qrc:/qt/qml/QindaQt/SettingsApp/Appearance/qml/x` — the preview `Image`
  received the bare draft path and resolved it against the document's `qrc:`
  base, which is how the built app actually loads this module. Custom-path
  previews were broken in production, not only "latent" as the lead supposed.

With the fix restored, the same row passes (exit 0, 3/3 functions in that run;
full binary green below).

## Manager leads — verdicts

1. **`WallpaperController::applySnapshot()` early-returns on a fresh profile —
   DISMISSED with evidence.** The client requests both keys via
   `ShellPreferenceValues::scopedKeys()`; the schema (`data/settings/
   schema-v2.json`) and profile defaults (`data/settings/profile-defaults/
   qindaqt.json`) always supply both as strings, so the layered snapshot is
   never missing them. The new row
   `qindaqt.shell-wallpaper-controller::freshProfileDefaultsAndConfirmedChangesReachBackgroundWindows`
   proves the bundled default (`qindaqt:qinda-punk`, `scaled`) reaches one
   background window per screen on a fresh profile with zero user-layer
   writes, then that live commits and an explicit empty choice reconcile. No
   controller change was needed.
2. **Two-way `TextField` — CONFIRMED (mechanism corrected, above).** Fixed in
   the WIP by the imperative `lastCommitted` adoption guard; this run kept it.
3. **`.png`-only at both ends — CONFIRMED latent, fixed at both ends.** The
   catalog globs png/jpg/jpeg/webp/bmp with per-basename priority;
   `resolveWallpaperSource()` tries the same list in the same order, so one
   `qindaqt:<name>` identity names the same file for chooser and desktop.
   ADR-0228 records the contract (superseding ADR-0078's resolution point
   only).
4. **Bare path assigned to `Image.source` — CONFIRMED live in production**
   (qrc evidence above). Fixed: the preview binds to the model's existing
   `previewWallpaper` projection (complete `file:` URL, empty for unknown /
   missing / relative values), which the Themes/Windows destinations already
   used. The FileDialog still stores a plain filesystem path in the draft —
   that is the settings contract the shell consumes; only the QML `url`
   assignment was wrong.

## Paths changed (vs base `402ccb3b`)

- `src/apps/settings/appearance/qml/AppearanceWallpaperSection.qml` — two-way field guard; preview binds the model projection
- `src/apps/settings/appearance/wallpaper_catalog.cpp` — multi-format discovery + per-basename priority
- `src/shell/runtime/shellpreferencevalues.cpp` — multi-format bundled resolution (mirrored contract comment)
- `tests/shell/CMakeLists.txt` — new controller test; **moved
  `qindaqt.shell-preference-values` and `qindaqt.shell-wallpaper-controller`
  out of the false `if(TARGET qindaqt-shell)` guard** so they register at
  `QINDAQT_BUILD_PRODUCTION_SHELL=OFF` (AGENT-NOTE in place)
- `tests/shell/tst_wallpapercontroller.cpp` — new shell row; waits for
  `Ready && !writeInFlight` between commits per the SettingsClient contract
- `tests/shell/tst_shellpreferencevalues.cpp` — format-resolution rows
- `tests/apps/settings/appearance/tst_appearance_values.cpp` — catalog format/priority/dedup rows
- `tests/apps/settings/appearance/tst_appearance_settings_model.cpp` — `previewWallpaper` projection rows
- `tests/apps/settings/appearance/tst_appearance_page.cpp` — field/preview row (the reproduction above)
- `tests/apps/settings/appearance/stub_appearance_model.h` — stub mirrors the projection
- `docs/wiki/adr/0228-bundled-wallpapers-resolve-beyond-png.md` (new),
  `docs/wiki/adr/0078-…` (supersession note), `docs/wiki/adr/index.md`,
  `docs/wiki/apps/appearance-settings.md`, `mkdocs.yml`

## Gates run (this tree, Debug + QINDAQT_ENABLE_STRICT_WARNINGS=ON)

- Configure: exit 0.
- Full build `cmake --build .build -j6 -- -k 0` (under manager.lock): exit 1 —
  **one** failed target, `tests/compositor/qindaqt_container_chrome_badge_tests`
  (`ld: cannot find -lqindaqt_hybrid_chrome_pointer_router`). Pre-existing at
  base `402ccb3b` with this exact configure (`QINDAQT_BUILD_KWIN_PLUGIN=OFF`):
  the library is declared only inside `if(QINDAQT_BUILD_KWIN_PLUGIN)` while the
  test links it inside the wider HybridChrome guard. `git diff 402ccb3b..HEAD --
  src/compositor tests/compositor` is empty. All other ~3140 edges build,
  including every target below.
- `ctest -N`: rows **#857 qindaqt.shell-preference-values** and **#858
  qindaqt.shell-wallpaper-controller** registered (absent before the guard fix).
- Focused rows, exit 0, **9/9 passed**:
  `qindaqt.appearance-values`, `qindaqt.appearance-preview`,
  `qindaqt.appearance-window-decoration-controller`,
  `qindaqt.appearance-window-decoration-page`, `qindaqt.appearance-settings-model`,
  `qindaqt.appearance-page`, `qindaqt.settings-customize-wallpaper-preview`,
  `qindaqt.shell-preference-values`, `qindaqt.shell-wallpaper-controller`.
- Docs: `python3 tools/validate-docs` exit 0 (347 docs); `mkdocs build
  --strict` exit 0.
- Broad sweep `ctest -L shell`: exit 1 — 3 failures, all out of scope and
  characterized: `status-notifier-applet-runtime-installed-package` and
  `clipboard-applet-runtime-installed-package` require a staged
  `bin/qindaqt-shell`, impossible at `QINDAQT_BUILD_PRODUCTION_SHELL=OFF`
  (pre-existing config mismatch); `task-list-dock-magnification-envelope`
  failed once (`scale` 1.48 vs 1.0) and **passed on rerun** — flake under
  multi-lane build load, in paths this lane never touched.

## Not done

- The compositor `pointer_router` link-guard defect and the two
  installed-package rows were **not** fixed: they sit in other lanes' modules
  (`tests/compositor`, status-notifier/clipboard packaging) and reproduce at
  base with the prescribed configure. Handed to the manager as found defects.
- Multi-monitor add/remove is exercised only structurally (the controller
  reconciles one window per screen; the offscreen platform offers one screen).
  No nested compositor was started, per the brief.
- No wallpaper artwork added; theme system untouched (per boundaries).
- Pre-existing, unrelated: `docs/wiki/adr/index.md` at base ends at ADR-0219
  while `mkdocs.yml` and `docs/wiki/adr/` contain 0220–0224; index lags nav.
  Both docs gates pass regardless. Not repaired here (those ADRs belong to
  other workstreams).
