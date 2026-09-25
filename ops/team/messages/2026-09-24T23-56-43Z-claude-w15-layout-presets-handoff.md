# Handoff: W15 Settings switches layout presets; editing happens on the panels (claude-w15-layout-presets)

- Branch `worker/claude-w15-layout-presets-20260923`, base `689a9df0` (head of
  `round/code-first-20260924` with W14 and W10; fast-forwarded from
  `6151fa24` before any edit). The candidate is the commit that adds this
  message. Code-first round: configured and syntax-checked, not built.
- **Parity first:** every capability of the old Settings editor was checked
  against the panels (table in `ops/team/workers/claude-w15-layout-presets.md`
  and in `docs/wiki/apps/customize-settings.md#where-editing-went`). Six were
  missing and are now on the panels, each appended after the existing menu
  entries so no keyboard position moved:
  - applet menu entry 8 **Duplicate** (`LiveCustomizationController::duplicateApplet`);
  - Panel ▸ **Displays** ▸ This display only / All displays
    (`configurePanel(…, "output", id|"*")` → `MovePanel`; `PanelLiveCustomization`
    passes the surface's output id);
  - Panel ▸ Size spin box 20–192 (was 16–160; the engine accepts 20–192);
  - desktop menu Desktop icons ▸ **Show/Hide desktop icons** and the
    bounded-number rows (icon size); the menu resolves the layout's own
    instance id (`desktopAppletId`), and `DesktopSurface.qml` now finds the
    desktop-icons settings by plugin, so a re-added
    `desktop-icons-instance-N` keeps its settings.
  - Gap reported, not closed: keyboard-only editing (the old outline and
    Space move mode). Panel menus work by keyboard once open; opening them
    needs the pointer. Follow-up: chip focus traversal in edit mode.
- **Contracts that landed (ADR-0267; supersedes ADR-0213 in part):**
  - Settings → Customize is a preset gallery: built-ins (installed order),
    then My presets (by name), `CustomizeProfileCard` miniatures with text
    badges (Default, Modified). Clicking switches via the confirmed Settings1
    `panels.layoutProfile` write: reported only after same-owner/epoch readback
    at the commit revision (stale waits, 4 s deadline; refusal, conflict,
    owner loss and uncertainty reported, never replayed).
  - Provenance: `loadPresetCatalog(stock, user)` keeps installed and user-store
    sides apart (user copy wins). Own preset = only in the store; edited
    built-in = store copy of an installed id.
  - Save current layout as preset… (re-reads the store first, so direct panel
    edits are included; does not switch), rename (same id/file), duplicate
    ("‹name› copy", "copy 2"…), delete (asks). New ids are `user-` + slug
    (+ `-N`), never a built-in id. Names: trimmed, 1–64 printable, unique
    ignoring case; at most 50 own presets.
  - Modified built-ins: Restore original (asks; `UserProfileStore::remove`, new
    in the profiles module) and Save as new preset.
  - Deleting the active preset commits `macos-inspired` (else first remaining
    built-in) first and removes the file only after that switch is confirmed.
  - The auto-hide delay control stays on the page (moved into
    `CustomizePanelOptions.qml`, same objectNames and contract). The chord
    never had a Settings control and still has none.
  - Removed with their tests: canvas, palette, outline, property panes,
    pointer gestures, action bar, wallpaper/window previews, output provider,
    applet-setting validator. `RepositoryCustomizeEditorHost` is kept
    (unused by the page) as the parity reference for
    `qindaqt.customize-editor-live-host-parity` and
    `qindaqt-customize-parity-tool`.
  - The model publishes `dirty` as constant false, so the Settings Center's
    Customize departure fence never engages (left in place).
  - Search keywords/description updated ("layout presets", "saved layouts",
    "edit panels", "hide delay", …); the Welcome tutorial's Customize step now
    teaches presets + editing on the panels (it described drafts/Apply).
- **Checks run:** `cmake --preset dev -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
  (exit 0, re-run after the CMake edits). `syntax-check.sh` on every changed
  existing file: 10 C++ ok, 14 QML ok, 8 test sources NEEDS-GENERATED (only
  their `.moc` include), 0 FAIL; the 8 re-checked with the `.moc` line
  stripped: all ok, plus the 3 Settings Center tests that include the new
  stub. (The no-argument run also lists the deleted files as SKIP/ok; they no
  longer exist.) `./tools/validate-docs`: 403 documents OK. The Customize
  boundary scan (`cmake -P tests/apps/settings/customize/check_boundary.cmake`,
  a static source scan) passes. No build, no ctest.
- **Final build: look first at** `qindaqt.settings-customize-presets` (new),
  `qindaqt.settings-customize-page` (rewritten, warning-fatal),
  `qindaqt.settings-customize-window-lifecycle` (rewritten: Main.qml hosts,
  close/leave never prompt), `qindaqt.settings-customize-panel-delay`,
  `qindaqt.settings-customize-boundary`/`-poison`/`-installed-route` (route
  construction witness, ADR-0250), `qindaqt.profile-formats` (new
  `userStoreRemovesExactlyItsOwnFile`), the Settings Center suites that host
  Main.qml with the rewritten stub (`qindaqt.settings-navigation-*`,
  `-command-palette`, `-route-search`), and on the shell side
  `qindaqt.shell-live-customization-controller` (new
  `settingsEditorParityActionsPersist`), `qindaqt.live-customization-offscreen`
  (Duplicate entry 8, Displays, size bounds),
  `qindaqt.desktop-surface-customize-menu` (Show/Hide, icon size, new
  `showDesktopIconsAddsTheAppletWhenTheLayoutHasNone`), and the nested
  `shell.live-customization.menu.*` rows (`APPLET_COUNT` is now 9).
- **Caveats:** nothing built or run. Live-only: the shell adopting a restored
  or deleted user copy through its store watcher, and Displays ▸ This display
  on a real multi-output session. `tst_livecustomizationcontroller.cpp` is 582
  non-blank lines (test file, over the 500 review threshold, under 600).
  Welcome's tutorial text changed (no test pins it). ADR number used: 0267.
- **Next:** manager merge into the round branch.
