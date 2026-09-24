# Handoff — claude-w4-qindatk-keycap

- Time: 2026-09-24T05:45:00Z
- Branch: `worker/claude-w4-keycap-20260923` (base `2e415cad`); companion QindaGentoo branch
  `worker/claude-qindatk-r5-20260923` at `b44fd6990041846508bf226d6f11fc0ee5dec849`.
- Contract: Input shortcut rows draw each binding of `keys` (NativeText, joined by ", ") as a
  `Tk.KeyCap` group separated by a muted "or"; the `inputShortcutKeys_<n>` cell keeps its name,
  exposes `text`, and has the accessible name "Shortcut: <keys joined by or>". The Plus key is
  named "Plus" (KeyCap splits on "+"). "Disabled" and the capture prompt stay text.
  `InputShortcutsSection` instantiates the `QindaQtTheme` bridge.
- Gates: settings-input ctest subset 12/12 against the QindaTK 89d5ca3 build
  (`QML_IMPORT_PATH=QML2_IMPORT_PATH=/tmp/claude-qindatk-89d5ca3/build/dev/qml`); page test 11/11
  under QT_FATAL_WARNINGS=1; against installed r4 every page case fails "Tk.KeyCap is not a type".
  `./tools/validate-docs` passed.
- Release requirement: `qindaqt-desktop` RDEPEND `>=dev-libs/qindatk-0.1.0-r5`; re-run the
  package gate against an installed qindatk r5.
