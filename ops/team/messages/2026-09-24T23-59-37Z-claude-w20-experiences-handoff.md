# Handoff: W20 familiar desktop experiences (claude-w20-experiences)

- Time: 2026-09-24T23:59:37Z
- Branch `worker/claude-w20-experiences-20260923` on the hub, base `e8c8daec` (round head: W7, W12,
  W13, W17/W18, W19). The candidate is the commit that adds this message. Code-first round: configured
  and syntax-checked only; nothing built or run. ADR-0268.

## What landed

- **Six experiences = stock profile + the theme it names + W19 buttons + global-menu rule** (ids kept):
  Menu and Dock (`macos-inspired` + Qinda Mist, lights, global menu), Classic Taskbar (`qinda-bliss` +
  Qinda Classic Blue, `blue-tiles`, clean blue bar), Centered Taskbar (`windows-modern` + new Qinda
  Daylight, `wide`, no ads/nags wording), Corner Bar (new `beos-inspired` + Qinda Marigold, `tab`,
  top-right bar, double-click rolls up), Program Groups (new `win31-inspired` + Qinda Classic Grey,
  `bevel`, no task bar, minimize rolls up to a desktop icon, program groups = the dock's groups on a top
  bar), Workspace Dock (`nextstep-inspired` + Qinda Graphite, `bold`, system tile with the Nest mark on
  top of the column; minimized windows wait as their tiles). Global menu stays only in Mac-style,
  QindaQt, Unity-style. Vendor names removed from user-visible names (Qinda macOS → Qinda Mist,
  QindaQt Bliss → Classic Taskbar / Qinda Classic Blue with Noto Sans, Luna Classic → Weathered Blue).
- **Theme-authored title-bar behaviour** (`decoration.titleDoubleClick`, `minimizeAction`, `titleWear`):
  theme loader + `DecorationSpec`; `DecorationChrome.minimizeRollsUp/titleWorn` (+ map keys, omitted at
  defaults); `theme` double-click now = theme's action else KWin's; `minimizeAction: roll-up` swaps the
  roll-up control into minimize's place (`decorationButtonKinds`); `titleWear: false` paints an authored
  bar flat (flat branch, theme font, ink by lightness, no shadow). Documents that author a title color
  stay weathered.
- **Mac dock permanent ends**: quick-launch `items` = `file-manager` | `trash` | `others` | `all`.
  `QuickLaunchController` gains `fileManagerRow`, `trashRow`, `openTrash()`, `holdFileManagerEnd()`
  (FM windows claimed while an FM end is shown); `QuickLaunchApplet` slices rows, disables drag/drop on
  ends, menus hide move/remove/group; `PanelAppletRow` counts an end as one tile (+1 line, now 350/350).
  Mac dock: FM end, Applications, `others`, tasks, Trash end. Dock value/codec unchanged.
- **`workflow.fileManager` hint** (default `finder`; loader, serializer, validation, round trip).
  **Placeholder for W11s:** the File Manager style setting (ADR-0271) should use the selected layout's
  `workflow.fileManager` as its default until the user picks a style (`AGENT-CONTRACT` in
  `src/profiles/include/qindaqt/profiles/layout_profile.h`). Experiences: Mac/BeOS/NeXT `finder`,
  XP/Win11/Win3.1 `explorer` (Win 3.1's tree beside the list = the Explorer arrangement).

## Checks run (no builds)

- `cmake --preset dev -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`: exit 0 (re-run after the desktop_controls
  CMake edit).
- `syntax-check.sh` on the branch: 17 ok, 13 NEEDS-GENERATED (test `.moc` includes only), 0 FAIL; the
  13 re-checked with the `.moc` include stripped: 13 ok, 0 warnings.
- `./tools/validate-docs`: exit 0, 401 documents.
- Scripted (not tests): QST-1 contrast pairs for the four new themes (52/52 pass; replica agrees with
  all 12 existing themes); stock-profile invariants for all 11 profiles against the manifests (one
  menu, clipboard beside notifications, one tray, task-list per workflow, global menu top-only in 3).
- `tools/check-source-shape`: no new errors; `PanelAppletRow.qml` 350/350 and `QuickLaunchApplet.qml`
  346/350 (both already past review); `tst_applet_instance_resolver.cpp` and `tst_appearance_page.cpp`
  were already over 600 and did not grow.

## Final build/test run: look first at

1. `qindaqt.profile-formats`, `qindaqt.theme-formats`, `qindaqt.decoration-title-options`,
   `qindaqt.decoration-documents`, `qindaqt.applet-runtime-resolution` (11 profiles),
   `qindaqt.design-tokens-built-in-contrast` / `-benchmark`, `qindaqt.appearance-preview` (16 themes),
   `qindaqt.appearance-page` (glyph row now `qinda-dark`).
2. `qindaqt.desktop-controls-dock` (permanent ends, FM hold claims), `qindaqt.desktop-controls-offscreen-dock`
   (`items` slices; QT_FATAL_WARNINGS), `qindaqt.desktop-controls-boundary` (new private link to
   `qindaqt_file_manager_menu_catalog`), `qindaqt.panel-geometry-offscreen`, dock-geometry QML rows.
3. `qindaqt.global-menu-runtime-composition-private-bus`, the desktop-menu private-bus row,
   `qindaqt.shell-capture-matrix` (new experience rows under QT_FATAL_WARNINGS).

## Previews to render (final build phase)

- Shell: `qindaqt-shell-preview --profile P --theme T --width 1920 --height 1080 --screenshot out.png`
  for macos-inspired/qinda-macos, qinda-bliss/qinda-bliss, windows-modern/qinda-daylight,
  beos-inspired/qinda-marigold, win31-inspired/qinda-classic-grey, nextstep-inspired/qinda-graphite.
- Window chrome (active and inactive, 1x and 2x) for Qinda Classic Blue, Daylight, Marigold, Classic
  Grey, Graphite: e.g. `QINDAQT_APPEARANCE_CAPTURE_DIR=… qindaqt.appearance-page` with those themes, or
  a nested session.
- Live (nested session): Corner Bar tab double-click rolls up; Program Groups minimize leaves an icon
  chip on the desktop; Mac dock FM end shows one icon with a running dot; Trash end opens/empties.

## Caveats

- Choosing a layout keeps the saved theme (ADR-0074); the handbook names each pair. W15's preset
  gallery could offer to apply it.
- Under a minimize-rolls-up theme, contained windows' handlebars show no minimize (More menu keeps it);
  the control is shown like W19's roll-up button, not hidden for unminimizable windows.
- Luna taskbar dressing still requests Trebuchet MS / Tahoma by name (advisory, no font shipped); the
  bundled wallpaper label "Qinda bliss" derives from its file name (its saved id). Not changed.
- Clean authored bars follow the decoration corner radius (retro themes author 2 px via v2 `radii`).
- The `others` slice still counts a hidden stored FM/Trash in dock fit arithmetic (≤2 slots early).
- Program groups are the user-wide dock value's groups; a fresh dock has none until made.
- `profile_json_reader.cpp` 530 non-blank (review threshold, was 524); `decoration_painter.h` 376.

## Next

Manager merge into the round branch; W11s wires the `workflow.fileManager` default.
