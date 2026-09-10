# Calendar stock-controls lane — midpoint

- **Commit:** `737ebf27` on `main` — "Build the Calendar app on stock Qt Quick Controls"
- **Paths:** `src/apps/calendar/**`, `tests/apps/calendar/CMakeLists.txt` only.

## What landed

- ADR-0116 presentation conversion: all 7 `ui/` files now import only
  `QtQuick`/`QtQuick.Controls`/`QtQuick.Layouts`; no `QindaQt.Tokens`,
  no `QindaQt.Controls`, no palette hex literals. Root is a stock
  `ApplicationWindow` (decision: ApplicationShell QML is itself a
  token-styled component, so the coordinator seams were re-bound in
  Main.qml with stock controls instead of keeping ApplicationShell).
- Non-visual seams preserved and verified: AppShell action catalog +
  checked-state sync, `composeCalendarMenuExport` (fail-closed global-menu
  export, `inWindowMenuVisible` toggle kept as a root property), Settings1
  `CalendarPreferences`, KF6::CalendarCore model layer, ICS import/export.
- QST wiring removed for Calendar only: TokenFacade/ThemeLoader/
  ApplicationAppearanceController and the `--theme`, `--theme-directory`,
  `--check-theme` CLI options are gone; the executable no longer links
  DesignTokens/AppAppearance/TokensQml/themes and no longer installs the
  QindaQt QML modules under the Calendar component.
- Build fixes: `QStringLiteral(identifier)` misuse in
  `calendar_preferences.cpp`, unused `eventUid` parameter, AUTOUIC off for
  `qindaqt-calendar` (`ui_action_probe.h` matches the `ui_*.h` pattern),
  nodiscard `fail()` in the UI action probe.
- `MonthGrid.qml` renamed to `MonthViewGrid.qml`: `QtQuick.Controls.Basic`
  ships its own `MonthGrid` control and unqualified lookup silently resolved
  to it ("Cannot assign to non-existent property occurrenceModel" at
  runtime). AGENT-NOTE left in the file.
- Orphaned tests registered: `tst_reminder_delivery.cpp`,
  `tst_calendar_action_catalog.cpp`, `check_cli_rejection.cmake`.

## Gates

- `cmake --build build/dev -j8` full build: green.
- `ctest --test-dir build/dev -L calendar --output-on-failure`: **9/9 pass**
  (6 registered rows before; +reminder-delivery, +action-catalog,
  +cli-rejects-multiple-paths).
- Offscreen probes (no bus, `QT_QPA_PLATFORM=offscreen`): `--check-qml-root`,
  `--check-ui-contract`, `--check-ui-actions` all exit 0; the actions probe
  drives create→persist→view-switch→delete→fixture-import through the
  production QML.
- `python3 tools/check-source-shape`: exit 0.

## Notes / collisions

- `src/shell/bluetooth_applet/**` has another agent's uncommitted in-flight
  changes in this checkout; a transient link failure in
  `tests/shell/desktop_controls` during that lane's edits resolved on
  rebuild. Not staged or touched here.

## Remaining (next lane)

- Event editing UI (editor remains create-only; edit = delete + recreate),
  event details pane, calendar wiki page (`docs/wiki/apps/calendar.md`),
  packaging: drop
  `packaging/gentoo/gui-wm/qindaqt-desktop/files/qindaqt-desktop-0.1.0_pre20260909-r1-skip-inflight-calendar.patch`
  now that the app compiles.
