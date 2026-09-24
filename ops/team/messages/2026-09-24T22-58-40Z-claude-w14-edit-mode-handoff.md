# Handoff: W14 panel edit mode, drag applets within and between panels (claude-w14-edit-mode)

- Branch `worker/claude-w14-edit-mode-20260923`, base `2d896244` (head of
  `round/code-first-20260924`); the candidate is the commit that adds this
  message. Code-first round: configured and syntax-checked, not built.
- **What was broken** (read end to end; details in the worker record): edit
  mode hid behind Meta+right-click; applets kept taking presses (and the dock
  strip's own drag); a zone `Flickable` could steal the drag; the Done/Undo
  bar covered the rightmost applets and sat outside a dock's input mask; a
  release over the desktop committed the last target; cross-panel drops were
  guessed by thirds on the source's own output only; no preview; no Escape.
- **Contracts that landed (ADR-0266, supersedes ADR-0213 in part):**
  - Edit Panels in every panel's plain right-click menu (toggle, "Done
    Editing Panels" while editing), in every desktop context-menu style, and
    View ▸ Edit Panels (checkable) in the desktop menu (new ADR-0260 command
    `EditPanels` → `LiveCustomizationController::toggleEditMode`).
  - Applets inert in edit mode: a shield `MouseArea` per chip takes every
    press/wheel/hover; the `DragHandler` lives inside it; a left drag anywhere
    moves the applet, a right click opens its customize menu; zones do not
    flick. The dock strip's W13 gestures are live only outside edit mode.
  - Global drag point: the source surface publishes the pointer in global
    logical coordinates (`trackDragPoint`, `dragPoint`/`dragPointChanged`);
    every panel surface resolves points inside itself (nearest zone, halfway
    boundaries; docks keep zone rectangles) and hovers; over no panel the
    controller hovers the empty target first, so a release cancels.
    `panelSurfaceAt("")` searches every output. Works across displays.
  - Preview: `dropTarget` (accepted target) opens a gap in its zone (chips
    slide aside via `Translate`, the zone asks for the extent) with a marker.
  - Bar: Add applet… (per-zone palette picker → `addApplet`, the existing
    `InsertApplet` intent), Undo, Done, in a stretch the zones leave free at
    the material's trailing end; a dock widens its shelf by that stretch.
  - Escape: `PanelEditKeyboard` gives panels on-demand keyboard interactivity
    only while editing (LayerShellQt seam via a new factory window observer);
    Escape cancels an open drag, else leaves edit mode.
- **Checks run:** `cmake --preset dev -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
  (exit 0, re-run after the CMake edits); `syntax-check.sh` on the branch:
  18 ok (8 C++ and 10 QML files), 5 NEEDS-GENERATED (test `.moc` includes
  only), 0 FAIL; those 5 re-checked with the `.moc` include stripped: all ok,
  no warnings;
  every other TU including a changed header ok
  (`desktop_menu_controller.cpp`, `desktopmenucomposition.cpp`,
  `shellruntimeapplication*.cpp`, `shelldevelopmentevidence.cpp`,
  `livecustomizationcontroller_panels.cpp`, and the desktop-menu test TUs);
  qmllint shows no new warnings; `./tools/validate-docs`: 400 documents OK.
  No build, no ctest.
- **Final build: look first at** `qindaqt.live-customization-edit-mode-offscreen`
  (rewritten: two panel surfaces, real drags with `mouseDrag`),
  `qindaqt.live-customization-offscreen`, the new
  `qindaqt_panel_edit_keyboard_tests` / `qindaqt.shell-panel-edit-keyboard`,
  `qindaqt.shell-live-customization-controller` (new
  `dragPointClearsOffTargetDrops`, two-output lookup), the desktop-menu rows
  (`-model`, `-targets`, keyboard QML), `qindaqt.desktop-surface-customize-menu`
  (new `contextMenuEditPanelsEntersEditMode`), and the PanelContent-hosting
  suites (panel geometry, zone budget, Luna, dock geometry, global-menu hit
  targets) for the zone-extent and marker changes.
- **Caveats:** live-only: the implicit-grab crossing between layer surfaces
  and displays, on-demand keyboard focus and Escape under KWin, the cursor
  shapes. The nested row `shell.live-customization.editmode.*` now drops at the
  tray's trailing end (precise resolution replaced thirds). Not done: auto-
  hidden panels are not revealed in edit mode; applets that paint nothing
  (zero width) cannot be grabbed; clicking a panel while editing takes
  keyboard focus from the active window. `tst_livecustomizationcontroller.cpp`
  is 506 non-blank lines (test file). ADR number used: 0266.
- **Next:** manager merge into the round branch; W15 (next in lane A) can
  build its Settings change on the Add applet… picker and Edit Panels entries.
