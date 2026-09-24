# Claim — claude-w4-qindatk-keycap

- Time: 2026-09-24T05:05:00Z
- Workstream: plan W4 (QindaTK r5 release, then shortcuts drawn as keys).
- Base: `2e415cad`; branch `worker/claude-w4-keycap-20260923`.
- Owns: `src/apps/settings/input/qml/InputShortcutRow.qml`, the QindaTK theme bridge line in
  `InputShortcutsSection.qml`, `tests/apps/settings/input/tst_input_page.cpp` (one new case),
  `docs/wiki/apps/input-settings.md`.
- Checked: `worker/settings-input-20260923` does not modify either shortcut QML file; it does
  touch `tst_input_page.cpp` and `input-settings.md` (different hunks; expect a trivial merge).
